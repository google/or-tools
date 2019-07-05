<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile>
  <compound kind="file">
    <name>cp_model.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/sat/</path>
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
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/gen/ortools/sat/</path>
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
    <class kind="struct">is_proto_enum&lt; ::operations_research::sat::DecisionStrategyProto_VariableSelectionStrategy &gt;</class>
    <class kind="struct">is_proto_enum&lt; ::operations_research::sat::DecisionStrategyProto_DomainReductionStrategy &gt;</class>
    <class kind="struct">is_proto_enum&lt; ::operations_research::sat::CpSolverStatus &gt;</class>
    <namespace>internal</namespace>
    <namespace>operations_research</namespace>
    <namespace>operations_research::sat</namespace>
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
      <anchor>ac0ca8810a97bcc1b3d45269a33fd4f0c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_FIRST</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac0ca8810a97bcc1b3d45269a33fd4f0ca5e00b7cd6b433ec6a15ff913d3b2c3f3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_LOWEST_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac0ca8810a97bcc1b3d45269a33fd4f0ca0b1d456b36749d677aa4a201b22ba114</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_HIGHEST_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac0ca8810a97bcc1b3d45269a33fd4f0ca79fc0af04ed454750ecb59dc5a748e88</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_MIN_DOMAIN_SIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac0ca8810a97bcc1b3d45269a33fd4f0ca18e573e60bf8dde6880a6cfb9f697ffc</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_MAX_DOMAIN_SIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac0ca8810a97bcc1b3d45269a33fd4f0ca9bc8cd090f555c04c4fb8ec23838dc30</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_DecisionStrategyProto_VariableSelectionStrategy_INT_MIN_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac0ca8810a97bcc1b3d45269a33fd4f0ca77405cd855df69ed653be2766be0a1af</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_DecisionStrategyProto_VariableSelectionStrategy_INT_MAX_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac0ca8810a97bcc1b3d45269a33fd4f0cadecec94c9d1599ecbdfdab2f7cfcb7aa</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>DecisionStrategyProto_DomainReductionStrategy</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a20ead57ac0739497ea66f0c21b23b529</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_SELECT_MIN_VALUE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a20ead57ac0739497ea66f0c21b23b529a2f416e6e94f971bfbb75ba25e7f7b760</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_SELECT_MAX_VALUE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a20ead57ac0739497ea66f0c21b23b529ac22896facd05595ce84133b3b3043685</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_SELECT_LOWER_HALF</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a20ead57ac0739497ea66f0c21b23b529ab63e61aebddafddd1496d6ab577dab53</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_SELECT_UPPER_HALF</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a20ead57ac0739497ea66f0c21b23b529ac41d0ba8114af7179c253fda16e517ca</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_DecisionStrategyProto_DomainReductionStrategy_INT_MIN_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a20ead57ac0739497ea66f0c21b23b529a82875a7d185a8f87d56cb0fb0f37f72a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_DecisionStrategyProto_DomainReductionStrategy_INT_MAX_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a20ead57ac0739497ea66f0c21b23b529ac1c76a18c1405c9569b8afca29919e48</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>CpSolverStatus</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac14a394f629f2cf1070b84bce2e427ce</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UNKNOWN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac14a394f629f2cf1070b84bce2e427cea693e3d1636a488a456c173453c45cc14</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MODEL_INVALID</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac14a394f629f2cf1070b84bce2e427ceacb3300bde58b85d202f9c211dfabcb49</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>FEASIBLE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac14a394f629f2cf1070b84bce2e427ceae4d551fa942cba479e3090bb8ae40e73</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INFEASIBLE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac14a394f629f2cf1070b84bce2e427cea185c2992ead7a0d90d260164cf10d46f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac14a394f629f2cf1070b84bce2e427cea78e9c6b9f6ac60a9e9c2d25967ed1ad0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CpSolverStatus_INT_MIN_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac14a394f629f2cf1070b84bce2e427cea443f059ef1efc767e19c5724f6c161d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CpSolverStatus_INT_MAX_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac14a394f629f2cf1070b84bce2e427ceae535ad44840a077b35974e3a04530717</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_NAMESPACE_OPEN ::operations_research::sat::AllDifferentConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::AllDifferentConstraintProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>af9e54e2d5d81731965cba2c72fd237f5</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::AutomatonConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::AutomatonConstraintProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>a409e867844426d248649058045d91b4a</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::BoolArgumentProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::BoolArgumentProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>a26f4220a644805d216623919b4454e90</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CircuitConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::CircuitConstraintProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>aa622f7324c218952ff6e6fa76e70b5ae</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CircuitCoveringConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::CircuitCoveringConstraintProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>a053a9a83617d85d70590f9bcb69f9072</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::ConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::ConstraintProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>a8a08a0412dc7ad772d01538c4541d8fe</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CpModelProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::CpModelProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>a8bb69e78d4b1193a570cb373cbcd77f1</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CpObjectiveProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::CpObjectiveProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>aaa9871408c076cdce214c53975c778b2</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CpSolverResponse *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::CpSolverResponse &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>aa932638eb3288abef76ec6ce44abad2c</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CumulativeConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::CumulativeConstraintProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>a469cc5bec5d04722b7a2ed2157cbed69</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::DecisionStrategyProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::DecisionStrategyProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>ae9d3a1b377448fff473eb094e1a4398f</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::DecisionStrategyProto_AffineTransformation *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::DecisionStrategyProto_AffineTransformation &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>a76684065481e77a04d6a785b57a37ea0</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::ElementConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::ElementConstraintProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>aafaee55e2ef399a5e005a29ebdd5557f</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntegerArgumentProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::IntegerArgumentProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>a31af3405cf06940dbbd7e2ada3faa05e</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntegerVariableProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::IntegerVariableProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>abfe1c95f3203f48ee2e0fd985df573cf</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntervalConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::IntervalConstraintProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>a1a52f0d999d97d872a7b681048663497</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::InverseConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::InverseConstraintProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>aebc3014f5f916f36e4b00d3cb6b4221c</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::LinearConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::LinearConstraintProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>ae4641d439ae970665040cfe8c8e4cc17</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::NoOverlap2DConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::NoOverlap2DConstraintProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>ad2d5af22bab0d3f84df7c744828ab2e4</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::NoOverlapConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::NoOverlapConstraintProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>a42a27eb2a39b4d60a27f41639fdadda6</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::PartialVariableAssignment *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::PartialVariableAssignment &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>a69b0f0b690bac6d13d2cb8723d9bc746</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::ReservoirConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::ReservoirConstraintProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>ae6b0dd316e74205a0dc9ac55e4625278</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::RoutesConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::RoutesConstraintProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>a72e417ceed325aefeb735bc1269b5926</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::TableConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::TableConstraintProto &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>a32645384af0bf66e5cb51f2367bfee0a</anchor>
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
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac8eeb3305c37f40da67f55486402ac78</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>abbc472dcbb3ad76095da9926b37e49f8</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a158d3c3e8612a0cb9be525140c96267f</anchor>
      <arglist>(const std::string &amp;name, DecisionStrategyProto_VariableSelectionStrategy *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af161ecb897e60ce83c87c17d11ae7d91</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a60036e4e1e1d47218d6339e9119805c4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac22a3ab628a918dd90466ba12d6ee0cd</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a6f1fd67f2396dd88544958778b9854bf</anchor>
      <arglist>(const std::string &amp;name, DecisionStrategyProto_DomainReductionStrategy *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CpSolverStatus_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8f7f7995f8e9a03c15cdddf39b675702</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>CpSolverStatus_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad87fa7d63870ba0085a841c2303dad6b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>CpSolverStatus_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aede942101121114490d4f59631bf9292</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CpSolverStatus_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a463a1c6294a89434db5de2a5560685f4</anchor>
      <arglist>(const std::string &amp;name, CpSolverStatus *value)</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::DecisionStrategyProto_VariableSelectionStrategy &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>af88b4fbcdca26fee95079ec1dc7ff5ec</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::DecisionStrategyProto_DomainReductionStrategy &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>a58db7092358e258353cc6ab4d035ecaf</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::CpSolverStatus &gt;</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>af89daf730ccca1a8de5eebfdf9406131</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable">
      <type>const ::PROTOBUF_NAMESPACE_ID::internal::DescriptorTable</type>
      <name>descriptor_table_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>a15e31e7e010c4b2e239f514608cbf9a8</anchor>
      <arglist></arglist>
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
      <type>constexpr DecisionStrategyProto_VariableSelectionStrategy</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_VariableSelectionStrategy_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3e5fd8dd3f65b3725d38e743b450fe14</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr DecisionStrategyProto_VariableSelectionStrategy</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_VariableSelectionStrategy_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3e888f213753f1e8fac882e0a2394040</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_VariableSelectionStrategy_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a6c4f9d19c7865cdcdc3fa9c1ecfd98e8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr DecisionStrategyProto_DomainReductionStrategy</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_DomainReductionStrategy_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>adeada39a9b25093a4cc1883510e1bb08</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr DecisionStrategyProto_DomainReductionStrategy</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_DomainReductionStrategy_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aec9bb98a52b3d32d47a598fc5eafb671</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_DomainReductionStrategy_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a1742cab1f2a807d32238c453b92bdeb3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr CpSolverStatus</type>
      <name>CpSolverStatus_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a067ce64a3f75c8567b22bf8bbecf2fa5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr CpSolverStatus</type>
      <name>CpSolverStatus_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac52096bfb8221d5724ff16dc4c93647c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>CpSolverStatus_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aeccedf377b000af35b4e9091c1bc2bb8</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>cp_model_solver.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/sat/</path>
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
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/sat/</path>
    <filename>model_8h</filename>
    <class kind="class">operations_research::sat::Model</class>
    <namespace>operations_research</namespace>
    <namespace>operations_research::sat</namespace>
  </compound>
  <compound kind="file">
    <name>sat_parameters.pb.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/gen/ortools/sat/</path>
    <filename>sat__parameters_8pb_8h</filename>
    <class kind="struct">TableStruct_ortools_2fsat_2fsat_5fparameters_2eproto</class>
    <class kind="class">operations_research::sat::SatParameters</class>
    <class kind="struct">is_proto_enum&lt; ::operations_research::sat::SatParameters_VariableOrder &gt;</class>
    <class kind="struct">is_proto_enum&lt; ::operations_research::sat::SatParameters_Polarity &gt;</class>
    <class kind="struct">is_proto_enum&lt; ::operations_research::sat::SatParameters_ConflictMinimizationAlgorithm &gt;</class>
    <class kind="struct">is_proto_enum&lt; ::operations_research::sat::SatParameters_BinaryMinizationAlgorithm &gt;</class>
    <class kind="struct">is_proto_enum&lt; ::operations_research::sat::SatParameters_ClauseProtection &gt;</class>
    <class kind="struct">is_proto_enum&lt; ::operations_research::sat::SatParameters_ClauseOrdering &gt;</class>
    <class kind="struct">is_proto_enum&lt; ::operations_research::sat::SatParameters_RestartAlgorithm &gt;</class>
    <class kind="struct">is_proto_enum&lt; ::operations_research::sat::SatParameters_MaxSatAssumptionOrder &gt;</class>
    <class kind="struct">is_proto_enum&lt; ::operations_research::sat::SatParameters_MaxSatStratificationAlgorithm &gt;</class>
    <class kind="struct">is_proto_enum&lt; ::operations_research::sat::SatParameters_SearchBranching &gt;</class>
    <namespace>internal</namespace>
    <namespace>operations_research</namespace>
    <namespace>operations_research::sat</namespace>
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
      <anchor>a299d9cbf6a19e8aa8294c01b02d59aa7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_VariableOrder_IN_ORDER</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a299d9cbf6a19e8aa8294c01b02d59aa7a92760d7186df85dfd6c188eae0b9b591</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_VariableOrder_IN_REVERSE_ORDER</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a299d9cbf6a19e8aa8294c01b02d59aa7a941215af97625c63a144520ec7e02bfb</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_VariableOrder_IN_RANDOM_ORDER</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a299d9cbf6a19e8aa8294c01b02d59aa7a8de6cbc54e325b78d800c8354591d726</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_Polarity</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a517d73d1db81fd87470e6bcbe87c633e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_Polarity_POLARITY_TRUE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a517d73d1db81fd87470e6bcbe87c633ea6145ecb76ca29dc07b9acde97866a8ee</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_Polarity_POLARITY_FALSE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a517d73d1db81fd87470e6bcbe87c633ea204c91561099609cdf7b6469e84e9576</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_Polarity_POLARITY_RANDOM</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a517d73d1db81fd87470e6bcbe87c633eafaf662755a533bc2353968b4c4da4d32</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_Polarity_POLARITY_WEIGHTED_SIGN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a517d73d1db81fd87470e6bcbe87c633eaf9a6fbf18fc3445083ca746b1e920ca6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_Polarity_POLARITY_REVERSE_WEIGHTED_SIGN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a517d73d1db81fd87470e6bcbe87c633ea77094f18176663ceea0b80667cf917a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_ConflictMinimizationAlgorithm</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ada813507f9879e596a07b3850f7fc0d5</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ConflictMinimizationAlgorithm_NONE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ada813507f9879e596a07b3850f7fc0d5ae1bd62c48ad8f9a7d242ae916bbe5066</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ConflictMinimizationAlgorithm_SIMPLE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ada813507f9879e596a07b3850f7fc0d5ac1adcdd93b988565644ddc9c3510c96c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ConflictMinimizationAlgorithm_RECURSIVE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ada813507f9879e596a07b3850f7fc0d5acf7f9f878c3e92e4e319c3e4ea926af7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ConflictMinimizationAlgorithm_EXPERIMENTAL</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ada813507f9879e596a07b3850f7fc0d5a52b205df52309c4f050206500297e4e5</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_BinaryMinizationAlgorithm</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a402444328a26710265238ae8fb883496</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_BinaryMinizationAlgorithm_NO_BINARY_MINIMIZATION</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a402444328a26710265238ae8fb883496a5cefb853f31166cc3684d90594d5dde9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_BinaryMinizationAlgorithm_BINARY_MINIMIZATION_FIRST</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a402444328a26710265238ae8fb883496acefb9cb334d97dc99896de7db79a2476</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_BinaryMinizationAlgorithm_BINARY_MINIMIZATION_FIRST_WITH_TRANSITIVE_REDUCTION</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a402444328a26710265238ae8fb883496ac586955ded9c943dee2faf8b5b738dbd</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_BinaryMinizationAlgorithm_BINARY_MINIMIZATION_WITH_REACHABILITY</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a402444328a26710265238ae8fb883496a30c30629b82fa4252c40e28942e35416</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_BinaryMinizationAlgorithm_EXPERIMENTAL_BINARY_MINIMIZATION</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a402444328a26710265238ae8fb883496aeb6a38e1f5f44d7f13c6f8d6325ba069</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_ClauseProtection</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af02bc4bd103928ea008623a1da38a12c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ClauseProtection_PROTECTION_NONE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af02bc4bd103928ea008623a1da38a12ca1739f0f3322dc59ebaa2fb9fa3481d6b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ClauseProtection_PROTECTION_ALWAYS</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af02bc4bd103928ea008623a1da38a12caa7de36c91e9668bd4d3429170a3a915a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ClauseProtection_PROTECTION_LBD</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af02bc4bd103928ea008623a1da38a12ca4ce148354b01f5b1e2da32e7576edaa3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_ClauseOrdering</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab33358f8fe7b8cb7f98c226b3a070e38</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ClauseOrdering_CLAUSE_ACTIVITY</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab33358f8fe7b8cb7f98c226b3a070e38aaab0bb6b57e109185e6a62d5d0271a04</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ClauseOrdering_CLAUSE_LBD</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab33358f8fe7b8cb7f98c226b3a070e38a2dcf758b7ee7431577e2aa80a60b163e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_RestartAlgorithm</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2916a603fb108fbf4133f865d472fc0b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_RestartAlgorithm_NO_RESTART</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2916a603fb108fbf4133f865d472fc0ba698c5900a88697e89f1a9ffa790fd49f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_RestartAlgorithm_LUBY_RESTART</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2916a603fb108fbf4133f865d472fc0ba0fcf1821b877dd61f6cfac37a36a82d8</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_RestartAlgorithm_DL_MOVING_AVERAGE_RESTART</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2916a603fb108fbf4133f865d472fc0ba89e7ee47fc5c826c03f455f082f22c70</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_RestartAlgorithm_LBD_MOVING_AVERAGE_RESTART</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2916a603fb108fbf4133f865d472fc0ba5d2302ed4086b87cadaad18aa5981aed</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_RestartAlgorithm_FIXED_RESTART</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2916a603fb108fbf4133f865d472fc0ba353691b5a40f70fe5d05cc01bdf22536</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_MaxSatAssumptionOrder</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a71cb004d78f7d8c38fcc9cbc225af533</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatAssumptionOrder_DEFAULT_ASSUMPTION_ORDER</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a71cb004d78f7d8c38fcc9cbc225af533ab0500c1196441cd7820da82c2c1baf6f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatAssumptionOrder_ORDER_ASSUMPTION_BY_DEPTH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a71cb004d78f7d8c38fcc9cbc225af533a61bc7845a56fecefcc18795a536d5eb3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatAssumptionOrder_ORDER_ASSUMPTION_BY_WEIGHT</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a71cb004d78f7d8c38fcc9cbc225af533a44da070df5c6e2443fa1c00b6c25893f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_MaxSatStratificationAlgorithm</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a011a7400ac03996a9023db2a9e7df81c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatStratificationAlgorithm_STRATIFICATION_NONE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a011a7400ac03996a9023db2a9e7df81ca5bb7f0a112c4672ea2abec407f7d384c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatStratificationAlgorithm_STRATIFICATION_DESCENT</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a011a7400ac03996a9023db2a9e7df81ca0c67cde78d6314de8d13734d65709b3a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatStratificationAlgorithm_STRATIFICATION_ASCENT</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a011a7400ac03996a9023db2a9e7df81cadf547628eb3421e641512aeb95b31912</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_SearchBranching</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af29968605d0dca7194386c85c3e8269e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_AUTOMATIC_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af29968605d0dca7194386c85c3e8269eac23498a3951b707b682de68c3f2ef4ba</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_FIXED_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af29968605d0dca7194386c85c3e8269ea4b402cda1dee9234ecc9bf3f969dae9c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_PORTFOLIO_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af29968605d0dca7194386c85c3e8269ea79d67aaf6b62f71bbddd9c5177ebedc1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_LP_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af29968605d0dca7194386c85c3e8269eac0ee72ff494861f949253aac50496f42</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_PSEUDO_COST_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af29968605d0dca7194386c85c3e8269ea0959d8f131e2610b97a8830464b2c633</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_PORTFOLIO_WITH_QUICK_RESTART_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af29968605d0dca7194386c85c3e8269ea28a2409f7a5ca2ecd6635da22e4e6667</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_HINT_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af29968605d0dca7194386c85c3e8269ea7254e3257c79f6ccdb408e6f26f0c2e1</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_NAMESPACE_OPEN ::operations_research::sat::SatParameters *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::SatParameters &gt;</name>
      <anchorfile>sat__parameters_8pb_8h.html</anchorfile>
      <anchor>ab55372e60a420147082bef8a70b2bb15</anchor>
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
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SatParameters_VariableOrder_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a0a75b439d4e889cf84f7d6f6b5a37a86</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>SatParameters_VariableOrder_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9e40adad4a6a75afceefe43c8c509457</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_VariableOrder_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2b5db4bee652895d2a67171ad96cecb7</anchor>
      <arglist>(const std::string &amp;name, SatParameters_VariableOrder *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_Polarity_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a4585806adf77d6f7a56bd21230a31175</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SatParameters_Polarity_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3a1aa6bdfa59980400e6617e6a206071</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>SatParameters_Polarity_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af6e220cb137fc0462fc253744b8bc3ba</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_Polarity_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa8e76d4d2386cfab3cefb7460f62d95c</anchor>
      <arglist>(const std::string &amp;name, SatParameters_Polarity *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a90d6f173fbfa33e26ff6508013c81ffd</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8ae6f7af0b88d08cd83a4ff1a1108985</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af3ae9c39e1b2cf4733a63fb9e4f958b7</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af1125a74a1efaf1562812c9d9b1ffc00</anchor>
      <arglist>(const std::string &amp;name, SatParameters_ConflictMinimizationAlgorithm *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_BinaryMinizationAlgorithm_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3e37f554c39fbb05faf07674ac550f47</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SatParameters_BinaryMinizationAlgorithm_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8e0457f852d7716dc2d913867100dc8c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>SatParameters_BinaryMinizationAlgorithm_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aea747a1c7b91baf6f1b5486700c31e5f</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_BinaryMinizationAlgorithm_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a7c328aaf533ab0b051f9b4617bd47d43</anchor>
      <arglist>(const std::string &amp;name, SatParameters_BinaryMinizationAlgorithm *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ClauseProtection_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac1aa9d5ea93fbc96a68237c2beda3836</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SatParameters_ClauseProtection_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac92d8d18b4148e00e25b463b42c0ea3b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>SatParameters_ClauseProtection_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ae1e232826064de5442ec15d6a2ff90f2</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ClauseProtection_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a45a55c59398241500c1604ed6736e7e0</anchor>
      <arglist>(const std::string &amp;name, SatParameters_ClauseProtection *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ClauseOrdering_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa6f7c43106217e8a55877110b7d87e7c</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SatParameters_ClauseOrdering_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a6763a151acaebadf9a4be9383e91e1eb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>SatParameters_ClauseOrdering_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a87813e257ba880dc079609db5d7f5da4</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ClauseOrdering_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab38e233912e1d6e80baf8fe3bec043ee</anchor>
      <arglist>(const std::string &amp;name, SatParameters_ClauseOrdering *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_RestartAlgorithm_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab199957e5457d8356687f12d67d1aaac</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SatParameters_RestartAlgorithm_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9d2995934edcfcc59a0da77719fcb11b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>SatParameters_RestartAlgorithm_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a34b396f35aa7c449a39d2b92c3f93744</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_RestartAlgorithm_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af97fc1fcba310fb2c415278cef3df03a</anchor>
      <arglist>(const std::string &amp;name, SatParameters_RestartAlgorithm *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_MaxSatAssumptionOrder_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a4104fcd7cb88b2edc4cbc86e6b331cdf</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SatParameters_MaxSatAssumptionOrder_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa36fba890ac5ad3ce86c9f70b8352bb5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>SatParameters_MaxSatAssumptionOrder_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa49899c1c9df530d20f240b519437c6d</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_MaxSatAssumptionOrder_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac304d2e190884ab7f230876fe1bd1d9f</anchor>
      <arglist>(const std::string &amp;name, SatParameters_MaxSatAssumptionOrder *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a5fcee51ba7784a7c403731301af6e14c</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9f22011e31eaf54170afe80d301665ac</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a7b0414d7c022b8a1f606bace4c8192cf</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac4c30c8eeb5c485f9676410745f1d9d2</anchor>
      <arglist>(const std::string &amp;name, SatParameters_MaxSatStratificationAlgorithm *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_SearchBranching_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9018824bcc1b169f32af87ad4faf7561</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SatParameters_SearchBranching_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a05587e288b302e572a8e80b100505a21</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>SatParameters_SearchBranching_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab59fe9a81546232a6951f9c673c02e8a</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_SearchBranching_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ae566d186f92afaced5ffb7ebae02d474</anchor>
      <arglist>(const std::string &amp;name, SatParameters_SearchBranching *value)</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_VariableOrder &gt;</name>
      <anchorfile>sat__parameters_8pb_8h.html</anchorfile>
      <anchor>a8885ce51db5f4e62425ed9d5c09ed568</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_Polarity &gt;</name>
      <anchorfile>sat__parameters_8pb_8h.html</anchorfile>
      <anchor>ad097df0060c8654c1b6cfc1285bc4969</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_ConflictMinimizationAlgorithm &gt;</name>
      <anchorfile>sat__parameters_8pb_8h.html</anchorfile>
      <anchor>aad7abfd3f639a7d1f9c112b4e4e7d4f0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_BinaryMinizationAlgorithm &gt;</name>
      <anchorfile>sat__parameters_8pb_8h.html</anchorfile>
      <anchor>a8b09b973b5ccbe5b9cbbcde8b824a2fb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_ClauseProtection &gt;</name>
      <anchorfile>sat__parameters_8pb_8h.html</anchorfile>
      <anchor>a611a7d92c4add7f72478e8fb80b789c2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_ClauseOrdering &gt;</name>
      <anchorfile>sat__parameters_8pb_8h.html</anchorfile>
      <anchor>a59199dd6f81e1980528a711e643b9905</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_RestartAlgorithm &gt;</name>
      <anchorfile>sat__parameters_8pb_8h.html</anchorfile>
      <anchor>a56c312c25b31438ad9cd30de76ecf4e8</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_MaxSatAssumptionOrder &gt;</name>
      <anchorfile>sat__parameters_8pb_8h.html</anchorfile>
      <anchor>a52f8336c6767ff5a1b41074ec7a991c0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_MaxSatStratificationAlgorithm &gt;</name>
      <anchorfile>sat__parameters_8pb_8h.html</anchorfile>
      <anchor>aeac2e6fcf0da93835e0cf3171d059b1c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_SearchBranching &gt;</name>
      <anchorfile>sat__parameters_8pb_8h.html</anchorfile>
      <anchor>ae9c1c46327d6db147835ccc4a748fdd4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable">
      <type>const ::PROTOBUF_NAMESPACE_ID::internal::DescriptorTable</type>
      <name>descriptor_table_ortools_2fsat_2fsat_5fparameters_2eproto</name>
      <anchorfile>sat__parameters_8pb_8h.html</anchorfile>
      <anchor>a34114c8909693aee3ef96a477490f1fc</anchor>
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
      <type>constexpr SatParameters_VariableOrder</type>
      <name>SatParameters_VariableOrder_VariableOrder_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2a39eab5a6aadab97bb23a7fb39af600</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_VariableOrder</type>
      <name>SatParameters_VariableOrder_VariableOrder_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a094b77c6089ed1097550980f9ffb764f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>SatParameters_VariableOrder_VariableOrder_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3232d0c544cf356f09b6f8d1b67269e3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_Polarity</type>
      <name>SatParameters_Polarity_Polarity_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>afbfa21e2ce75113388357f29f610342c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_Polarity</type>
      <name>SatParameters_Polarity_Polarity_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a72fe8e22daeacc4a74374d4c34bc09f4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>SatParameters_Polarity_Polarity_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a84b9e2a32889c7bc5476029d4107d736</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_ConflictMinimizationAlgorithm</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_ConflictMinimizationAlgorithm_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ae3d1dd4a33df05f7da9a3ea6c4932c0a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_ConflictMinimizationAlgorithm</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_ConflictMinimizationAlgorithm_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2bfd2dd07fc93d2ebcf90df9982b173f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_ConflictMinimizationAlgorithm_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8a1f2ce9ceb6c6e6ea95e8413c5f304c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_BinaryMinizationAlgorithm</type>
      <name>SatParameters_BinaryMinizationAlgorithm_BinaryMinizationAlgorithm_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab260b9d1bc3bedcc3ad29d6b2fd831d4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_BinaryMinizationAlgorithm</type>
      <name>SatParameters_BinaryMinizationAlgorithm_BinaryMinizationAlgorithm_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a92db718bcc5d276ccf747bde81c78a10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>SatParameters_BinaryMinizationAlgorithm_BinaryMinizationAlgorithm_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a97ac406a44712bd2893b29957f2528d5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_ClauseProtection</type>
      <name>SatParameters_ClauseProtection_ClauseProtection_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aef4cd5f95bfffe8b384372e1cba49049</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_ClauseProtection</type>
      <name>SatParameters_ClauseProtection_ClauseProtection_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>abf9faaf009e6527846e0ff336797f3a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>SatParameters_ClauseProtection_ClauseProtection_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a299e745a341d3282f1f57f930c9d56e1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_ClauseOrdering</type>
      <name>SatParameters_ClauseOrdering_ClauseOrdering_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a6e554645f4d0f9989e1f3d69c1528eea</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_ClauseOrdering</type>
      <name>SatParameters_ClauseOrdering_ClauseOrdering_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>acc0499f1b3c9772bc081ca484c6aa680</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>SatParameters_ClauseOrdering_ClauseOrdering_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aee2d784d894a30c420456d0b389b7970</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_RestartAlgorithm</type>
      <name>SatParameters_RestartAlgorithm_RestartAlgorithm_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a87bcdd92d224942666c7be6e2f936ab0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_RestartAlgorithm</type>
      <name>SatParameters_RestartAlgorithm_RestartAlgorithm_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a955126bc9840983ce5d4faa8d82f1669</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>SatParameters_RestartAlgorithm_RestartAlgorithm_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ae791277565602a13d6e3c8e4ff0e28b9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_MaxSatAssumptionOrder</type>
      <name>SatParameters_MaxSatAssumptionOrder_MaxSatAssumptionOrder_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aedb4be4a6a9caaf8d9161888934ad2d2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_MaxSatAssumptionOrder</type>
      <name>SatParameters_MaxSatAssumptionOrder_MaxSatAssumptionOrder_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ae198f9232534912ddf238f7be789f4aa</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>SatParameters_MaxSatAssumptionOrder_MaxSatAssumptionOrder_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a45e86ed8cbe846e59c55298161086446</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_MaxSatStratificationAlgorithm</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_MaxSatStratificationAlgorithm_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a5df42a6b5c40d46ea317abd561b7ea0b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_MaxSatStratificationAlgorithm</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_MaxSatStratificationAlgorithm_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8665ee9afc158ac57d842bcef9eccc59</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_MaxSatStratificationAlgorithm_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a673309e5337b624e75e496fe33494135</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_SearchBranching</type>
      <name>SatParameters_SearchBranching_SearchBranching_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab90d62c554b3478c3271c929cf81cb59</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_SearchBranching</type>
      <name>SatParameters_SearchBranching_SearchBranching_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac5449564c89e6ffab546725d1d49422a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>SatParameters_SearchBranching_SearchBranching_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3de01c1278d9f16ff4ff5cd72c0233da</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>sorted_interval_list.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/util/</path>
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
      <type></type>
      <name>AllDifferentConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a18a193d68b6d36129baa39ba8305e7bb</anchor>
      <arglist>(AllDifferentConstraintProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>AllDifferentConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>ad4bf1bfe0a0aa3f19e6b5dc7159adb30</anchor>
      <arglist>(const AllDifferentConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>AllDifferentConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>ad0d93227329a105dce1f5d886d4eece4</anchor>
      <arglist>(AllDifferentConstraintProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>a7594df5e05f4e00ef90295124f804b33</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>afc0eff892d4d3c91e2a8896cdd3f1c6a</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a36c0f6c68932ec390c6056553088d4cb</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>a70cc568bda521cb236b0de1105d6cbea</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a7a8089df01c8dd6e21d200235e19f6a9</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a9ba1716fdd6c673cdf8a272f36bc371c</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a0064fce1beae7a9a46176c1050ac5fc3</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a4c8b417d5bbabcbb0a7b9c612e238296</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a5432d5ef70e0542e82bf1dcd37835764</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a330d5a032e2edff2a61e7d0df0ad37e2</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>ac0f6689e29e66f4c442b3acb65f4e5d3</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a148d985a451711d4772c2788a872d47b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a4b67968e25b11b09e015b29e98728737</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a787dab3160566a9e6ef32aea1621ed12</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a530a7f2f84abfcc5c429c4bbb23f6ce1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>aec57737fc9e8cd9fb5741c1334d2f8b8</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>AutomatonConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a0a40b8d24a1c6be0de1b58258b88b1fa</anchor>
      <arglist>(AutomatonConstraintProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>AutomatonConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>acb72109275f0bbd30408de1bcf0eeacc</anchor>
      <arglist>(const AutomatonConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>AutomatonConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a9dbb826a67aee7033a9a40b8b39c04e5</anchor>
      <arglist>(AutomatonConstraintProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>a4356742955a42402b5109f9417046b8d</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a1f713b3f6e3703dce9404ac385815018</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ad7aec0f8284ef631528616c94b2e4b07</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>acb748e7519e9f4613a67fc964e07c37a</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a0f0c772e1735d9cb1bc2984b21ccf5e0</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ad4267a1dbbfd2fb2721de3bf7c86b3ca</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ad3328ed391a2d36ad716d60d910bcdb3</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>aef9565b5cd3343f0979ef107007dc826</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>final_states</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a67ad078c59a4ffd9b328557fea9d6407</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_final_states</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>acc60db648ea6ae4033b5c1d02305f81c</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_final_states</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a790d6563d36a290e884763caa6826171</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; &amp;</type>
      <name>final_states</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a42b381ffc45ae39bc8b81e6f426e9ca0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; *</type>
      <name>mutable_final_states</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a98e9b061b686ff37d0176b32dbba2e7f</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>transition_tail</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ace417bfcddc37ae122eb33d3976061ab</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_transition_tail</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a212d84ad00baa0488040c8886f7de073</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_transition_tail</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>afd36bb5e043f609441c6127de2e38fa7</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; &amp;</type>
      <name>transition_tail</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a8b3dcf412fb8e9dcd6e1f6206e68dbe0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; *</type>
      <name>mutable_transition_tail</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>af1ae6ae59444f831fd45d87e273195b3</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>transition_head</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a007eaad0566ac0542ce3362412d5b6e2</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_transition_head</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a7f592e13c29c9161dbe1de786c8f419b</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_transition_head</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ac330d77451cf4c74935c49ce16a3db63</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; &amp;</type>
      <name>transition_head</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a1091a8b57f3c4321b1fbbcb7d22ac368</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; *</type>
      <name>mutable_transition_head</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a3ab516231bcfe85a06f710828200e232</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>transition_label</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a824e784647eee6bfa2eca744baa88a96</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_transition_label</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a496dfc4e7fb4012c9bb442d6729e878a</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_transition_label</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>aad26e6bdba5475af14c5d766c3db6f36</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; &amp;</type>
      <name>transition_label</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a18e3780ff768a243c90d8bda85a6f1e9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; *</type>
      <name>mutable_transition_label</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a4b926673e1dacfcb8cb0e1f0644a36b0</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a7a7eb946cd2899ad44a9d07ee8278247</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ac263624b35636be7f4b3aba11f65aa28</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>af1e5140fb15d4ef95372802659250caf</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>acfb78f1e1215f1f1c1952027970ba6b5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a060568cd86774ddae044467d7ac70765</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>starting_state</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a7d9ccbb067ca5e444b9d47d81128ca21</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_starting_state</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a5db4d85bb9722f8f12c299914c561aac</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a02dacdf8e3df498488a50787dc1fd5be</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a786f11ebfbf1d56f43dbb2fe59c714ba</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ab2338d0ad7147dc2607ddd0f54e29146</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>BoolArgumentProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a0b8b0e911d72034c8e9079e61d44bf99</anchor>
      <arglist>(BoolArgumentProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>BoolArgumentProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a9bb2617efbb9575da8fc1d4cf01af39f</anchor>
      <arglist>(const BoolArgumentProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>BoolArgumentProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a0ab889fca98935fb0d33c0533676ce29</anchor>
      <arglist>(BoolArgumentProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>a1fd57fc737d9fd8ad9e5c2cd1e4a1b9a</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a8ffc74b28c02faca6b1fb511495eb74f</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a718f32ca9acff7fa22d4a521ad378fdb</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>a374bab814d71cc07ddb92859755ee9ae</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>abfae199f4bfa98a7a528833826f6d863</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a8309b2ec36ae6ec6f48858f5c2474d6d</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>afe0fff5867a98c14d6d29ba4720071ce</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>aac53884d856be05b5325ea8e5ab6f4b3</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a183f41aa3f8363fde734d052e549808b</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>acae81ec12bb5bd770d2eec51ef23d3f5</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>abd801eb8d8cb2a1610847f5c300f97f0</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>ae81e28c7552a8a0081427a597a5588fc</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a042502ee0b013a0ee642d5d3a971b2aa</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a9442a39d196c57fefa5a2bc430c28238</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a6df66478cdb7080394627f159184ccba</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a90c889b8fd2606dd3ef4ea903812e023</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>CircuitConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>acfc301e53387f6f6648d4c00219ccfbc</anchor>
      <arglist>(CircuitConstraintProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>CircuitConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>ac2b3d1c86cae0843cb1b90ad512a485a</anchor>
      <arglist>(const CircuitConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>CircuitConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>adca1b367f0dccf2e839d531961d47d8e</anchor>
      <arglist>(CircuitConstraintProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>ab4022fc492de205ba28ed8c9a09c8a10</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>af12d39200e7147020df8996e33d3e7f3</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a3c98c107d751beaea12a7364c16173b8</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>a3508e8df39b373eec09f9d737e760149</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>ad0487137e6d0f0c01feef70628f73809</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>aa5222e72b649e41786ada08c55e1a7fb</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a90aa18f88888ace0d623de979f7c398d</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a52952736062a3e3b3f2da58483a2a936</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a90fe0b6031d071a9976f4e4c9b3c44cf</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>aab571aded258b877e5b2e832fd9aeca1</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a32cfb519b55b8efbe0e8ffaca87ef3f3</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a12ab8126b7c549534c41a833a712a2e0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>ac657f71cd67eb628824b23e62e6cce60</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>aaac0c3fef3bcc091195f5e0abd77ca73</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>afd3ab11d3b69fcaf36c0d14d27d1df36</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>ad51e9b0a82b92275f28514c2e12f4a2e</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>ac883a9543e9401b498857d8fcc3e9536</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a16ecc66bb54205cbdfcf39053bf9ab77</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a17a9803ff6fb7ba1299d7608a7566ef9</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a167c39824d4540fa2022282b8b41960e</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>af211a92848fd2ad38db52af89ab3af53</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a52239d260759f64100e823a10362ba7e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a5d40c8bc35f9d182c7149cf0e817b119</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a0da7d539b61a75ebf543236dc6470940</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a0e82404eb2bf9a7e4c4a5903eaadf075</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a8e252e7ebf0df19a87d3777b89013dd3</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>CircuitCoveringConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>ac48a824427e52ed7eca62aa36ed15892</anchor>
      <arglist>(CircuitCoveringConstraintProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>CircuitCoveringConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a7923ca37bb6e6c8a86928e95ede9eede</anchor>
      <arglist>(const CircuitCoveringConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>CircuitCoveringConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>abbdc1c4f74e49e67d20a8b97d7f2cf3a</anchor>
      <arglist>(CircuitCoveringConstraintProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>ae22b3a2bdb16c7112c3e4bc87d31c99b</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>ae354770522793d2776dd29a968d8c850</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a3fe3f323c3dd1b34b7bef2ec713e9c6d</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>ae34537cf79bed4306c742409d094320f</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>ac5409dd9bcce987820c3fc174bdeb182</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a7d815930be735db6bafb18c2cac3e3c6</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a634b2023dc3d99e11a8bdc314cc6e3da</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>aba068503a8b7f3a19ffadeb93edfff42</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>nexts</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a6482685dc018f38a306cee7841b4d000</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_nexts</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a5aebe828c1ba441dc794430e9b52da19</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_nexts</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a800d633954ccafd43aa0bf66ec0e6377</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>nexts</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a6500a61dcdcb6242ac8712bc03868a09</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_nexts</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>ad06bddfaf45a1cb4a01bda9a0433d6c2</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>distinguished_nodes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a2a7431afa9135e0663127c9ab38cd8f8</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_distinguished_nodes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>ad9e12d507f22b04054aac4e3a6ea1e32</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_distinguished_nodes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a3783023f282cc73ccb3f201b38b76996</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; &amp;</type>
      <name>distinguished_nodes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a25872c9e5967d1c8c817be4f032616c8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; *</type>
      <name>mutable_distinguished_nodes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a33d635b59879cb017492eb1b2c6e26b4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a1e8d7ec791b11672fb43e828eea1af86</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>aa65aa58eeb416adac8cfd33bc7a1f23f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a3ed20d43a18d543f31cf3bfda5ce4fb2</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>ConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a1733b9cff441428641c1266ddbedac53</anchor>
      <arglist>(ConstraintProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>ConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a3a8b3abb61dc03fc158995e8a642a52c</anchor>
      <arglist>(const ConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>ConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a08ff9597aa845ded8b3da415ba7398df</anchor>
      <arglist>(ConstraintProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>ade399ad3b6845e16e854f66a7a2140fd</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aaabb103f3b6d54629795d13a55ebc1b7</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a498b891140896abdce15fad3fce9457f</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>af0994e9917bbe6552f6e1a4e6764c190</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a32aba4cb600b6493db4f74ff090d9461</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>af2af3c116e17f889970cd12c6109649c</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aef2bfeeda4c457d5b815191a78613004</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a8fb5d37a54083a5bff55e272fd122290</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>enforcement_literal</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aa982241c6cf65af57c5fa6330f0a7e78</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_enforcement_literal</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>abae83fc42c8c406f0e4689f7f32b929e</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_enforcement_literal</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a6eaa42ad925fb130b9de91e1faae8cd0</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>enforcement_literal</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a7955ed63902d5b67f8d82f7cc8a78839</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_enforcement_literal</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a69bdbbc236cbbcb74f110367263c9b2f</anchor>
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
      <type>const std::string &amp;</type>
      <name>name</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>acd77e8d0a4026c999d04a94387775282</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>af4088f5091bed104b22b6ccbd398abe7</anchor>
      <arglist>(const std::string &amp;value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ae5673f9ecb2fa3d87df0c8337599c7d0</anchor>
      <arglist>(std::string &amp;&amp;value)</arglist>
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
      <type>std::string *</type>
      <name>mutable_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>adbdd4f5efeab12b810f875b2492a663c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>std::string *</type>
      <name>release_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a042ffd63999a1573d23d2af6b3d28e8f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ac7a8a6601a6a9d39c1e34408a5cd0d82</anchor>
      <arglist>(std::string *name)</arglist>
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
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a28616e1523e83ca9f573ac0b58753c1d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ac8e6fe00953a8d2deaa2671a89516cc5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>afb99c8df57f687d4ea4ebbb12b2edcf0</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>CpModelProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>ab9c606f40d2570f8e807e41817f15a88</anchor>
      <arglist>(CpModelProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>CpModelProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a7006db70a302c79981b9660bbe246958</anchor>
      <arglist>(const CpModelProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>CpModelProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a899635e54102152ed80713602bb32b51</anchor>
      <arglist>(CpModelProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>a6d24e851d9cf29eb47184d2ac3b35cfc</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a7f2c39efda80964dd04f04a107de9ebc</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a415cc64f88bd4a001458baf0a0e5cc88</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>a4468d35b497d88141c9a924207031a6c</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>aa01dae17f9aee6b68369b6927de07c2c</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a65c43a44785d8365ddf946ff8a5e3a4c</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a83c440eb955944077880bf5eb881c763</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a94e19c4ffd38b953a0e7a769ac574295</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedPtrField&lt; ::operations_research::sat::IntegerVariableProto &gt; *</type>
      <name>mutable_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a2f72dc6e045bd107eccd6fb96c68a0dc</anchor>
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
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedPtrField&lt; ::operations_research::sat::IntegerVariableProto &gt; &amp;</type>
      <name>variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a1f922d0450bfa7e0735e1a3f8ffbbbfb</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedPtrField&lt; ::operations_research::sat::ConstraintProto &gt; *</type>
      <name>mutable_constraints</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a1954918f154563af2c8da34952fa76c2</anchor>
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
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedPtrField&lt; ::operations_research::sat::ConstraintProto &gt; &amp;</type>
      <name>constraints</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a40f360480270741c820afbee9084fc19</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedPtrField&lt; ::operations_research::sat::DecisionStrategyProto &gt; *</type>
      <name>mutable_search_strategy</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a860ed5a553a3c567df3b5757897a164f</anchor>
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
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedPtrField&lt; ::operations_research::sat::DecisionStrategyProto &gt; &amp;</type>
      <name>search_strategy</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a03c38afccb36b384306abe57b4098c2e</anchor>
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
      <type>const std::string &amp;</type>
      <name>name</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a9144500864f84719a9cf45c0977f7c13</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>abb2e2b370fdb73da1e261d97a9554e68</anchor>
      <arglist>(const std::string &amp;value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a0a6526e58404084476db463c8ed5d381</anchor>
      <arglist>(std::string &amp;&amp;value)</arglist>
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
      <type>std::string *</type>
      <name>mutable_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>ad57481a60fda7d4d85bad549b7ce97ed</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>std::string *</type>
      <name>release_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>af43f89c8f28f6162f97c906bf51925aa</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a7e96e1d5d7ef8c0a8204d86b7efc4765</anchor>
      <arglist>(std::string *name)</arglist>
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
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a731294524333d8e5d48435237348b339</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>aae27cb4dd5c4f7d3da50d7ad82a52453</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a7e44efb165d0981613a6ea8dcd412487</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>CpObjectiveProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a7490b3401f636ce10a4360aa118c65e6</anchor>
      <arglist>(CpObjectiveProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>CpObjectiveProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a94eb71df33b1b12bd25c19840e09ec61</anchor>
      <arglist>(const CpObjectiveProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>CpObjectiveProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>ada61bca96bb58de68bc84ea3f0919c3b</anchor>
      <arglist>(CpObjectiveProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>a623f196d47f6f4f5fbcf74538150093c</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>ae73ae521d3f073999bad9b9e6881ac6b</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>af8d192d03f6eca285ae95969ab72ad3e</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>a627cd3f163c4d34934916edcad03eef9</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>acfb80e1d561de2349e4fa2226c434d02</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>ac76699b8aab5654386665b87acaf4096</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a36064aad65cc24fed204f87490770ec3</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a99df0571c28bb530c03f3016d2635dac</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>ae244af0784c53ee44e97027e7a3fa61d</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a5b42ca233984edcf66ec6646310388be</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>aee4319427495cff63d7c7b117e18e87f</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a1f7458f5587dcf49d81556f40217dc85</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>abc3b9fef9bf2fb4902a4e3115e472968</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a92f697a0eb43c20a80c114d49548f647</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a9dd672e91dc5ba60b0f69d1abfa3a5bd</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>ab0eda9c9918d8535b90c39f49780ab29</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; &amp;</type>
      <name>coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>afac741fb4e2327f905402fcef2143c73</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; *</type>
      <name>mutable_coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a9296477196a9c7d4c36cfef50258ce83</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>acf4d56a13c0b8b92d94b76f86a9281b0</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a7e3fd4ac35de4616137e11abb80a4712</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a918ccbfd3a412280436939baf301c948</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; &amp;</type>
      <name>domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a44025c3ad0cc1fcf9021dbbbf44f0443</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; *</type>
      <name>mutable_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a8b1031d54d4bee6bd7db5424afab9f84</anchor>
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
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a1a4e2cdec62483f3f77412a73b316ca9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>aac51ebe65db3532e265b4ff9de79ab6f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a37544c0c84c2e1f16bf979e0b96f1b30</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>CpSolverResponse</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ac7681b7f64a9c45e91dcfbd5b8936077</anchor>
      <arglist>(CpSolverResponse &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>CpSolverResponse &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a2440c897e10a4669c114233b20c83572</anchor>
      <arglist>(const CpSolverResponse &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>CpSolverResponse &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a72081bb486a244e4517523eaa033e4d0</anchor>
      <arglist>(CpSolverResponse &amp;&amp;from) noexcept</arglist>
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
      <anchor>ae83455e74524f42725ba364fde23bc15</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a4696230468ba71b17ad03aecc1f1bf58</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>aa75a238f9aacbe1027451afafe3d34e0</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>a05c108403d00c66985fea5e0ebcd5d48</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a24a0bdebda34e5cef46e92f3f69a08e3</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a4096763c3527606d8093a576a2876aa4</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>aed6a825b81a8bf2fbbd2f16f23d48491</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a2f595ed4ce158d420594ecd4233dbf3b</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a81b237b96a7287285fcd8f2fc3fe20f1</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a08b87d620818db8da6ca5f7889e70b86</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a2ca28ff0bfe53ba270d70b061e69fd66</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; &amp;</type>
      <name>solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a2ec998043eeb90ac6f037b39d84cf275</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; *</type>
      <name>mutable_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a1ab19f8341ecae38f4de63b9212bfd21</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>solution_lower_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a6aa1509a4023b302c543ac040aeec1e0</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_solution_lower_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a40f627b8a75f42b235f96962c212b9e8</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_solution_lower_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a691cf62ab6d85232d2e2c1b8e9ba2bae</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; &amp;</type>
      <name>solution_lower_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a403e61710bd8e825c11b416a2ee4fc26</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; *</type>
      <name>mutable_solution_lower_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ad8f3c371c015ce611b14c6907dccddf9</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>solution_upper_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ab4fb3c7e269729cb0686c0c91838c761</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_solution_upper_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a8d6b6dd41e61b849c501285e557310c8</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_solution_upper_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a46d365039e48fe8227449ef2e667cacc</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; &amp;</type>
      <name>solution_upper_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ad601302b8b114c09f99f53e11f177985</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; *</type>
      <name>mutable_solution_upper_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>af6391067e60fcfa142eaca2e62be7290</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedPtrField&lt; ::operations_research::sat::IntegerVariableProto &gt; *</type>
      <name>mutable_tightened_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>adbdeeb6d8e5aaf22b373fb3f9889bdb9</anchor>
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
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedPtrField&lt; ::operations_research::sat::IntegerVariableProto &gt; &amp;</type>
      <name>tightened_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a4f9764f1613690eddd9233c8088f584a</anchor>
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
      <type>const std::string &amp;</type>
      <name>solution_info</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a1c1afddd00dfb0e8ab0710e9aaef263d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_solution_info</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>afd9b1c08048de154eeb8249ccdce83c4</anchor>
      <arglist>(const std::string &amp;value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_solution_info</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a858337b9c1074b25c515aac6fd5e187a</anchor>
      <arglist>(std::string &amp;&amp;value)</arglist>
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
      <type>std::string *</type>
      <name>mutable_solution_info</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a7007d548e08343070631d76e8608150c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>std::string *</type>
      <name>release_solution_info</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>aac94fa47e35567ed306c239b87d4b542</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_solution_info</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a6f68c0ff776bf2c95cf87579b0ce1f65</anchor>
      <arglist>(std::string *solution_info)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>num_booleans</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>affdd4a4b39a31b1e858f0df0cee8d16b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_num_booleans</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a2407cc21f500b63eaa0c30eb25d5febf</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_num_conflicts</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a818361f6305c54210b3e41051ed822be</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>num_conflicts</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a1da9d5094d01730150b2e3c79ea05a0d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_num_conflicts</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a2fa8b163c4d4bf0f8c3baba38b9a4052</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_num_branches</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>afb45b3e52697edae151112d72d357052</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>num_branches</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a1c29eb25ee1ab49024bceb4cb826b2b6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_num_branches</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a70496a76d73507fff2f6ecd7e12ed435</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_num_binary_propagations</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ae674cc8d35deb0b290dbefc52be06026</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>num_binary_propagations</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>abf4e77e7db38b31a42195a502c8cd968</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_num_binary_propagations</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ada2994e545f5a6c595afbc423b9a002f</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_num_integer_propagations</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a22ab55fb4c3769bb5d9b30830c8cb2b1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>num_integer_propagations</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a05af523bc68504b5266303f3107cbb25</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_num_integer_propagations</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a1cef224a8d98c9b805f4d25d03c0ae3e</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
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
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ac312585a1164a1e715dea6ae6f0bc7fa</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>adb8a8d1df0a96c81d156816cbb497845</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a0a355945018a1a750c9be88c01fc8e3c</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>CumulativeConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>aebf3fdb22167124baee3d7e4e5edde80</anchor>
      <arglist>(CumulativeConstraintProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>CumulativeConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a83c1d1b1cb5722859bcaaea1887c2f22</anchor>
      <arglist>(const CumulativeConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>CumulativeConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a5ec69399289ca49d80c90917c802e684</anchor>
      <arglist>(CumulativeConstraintProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>a25b603c875d3b30e212ff01e5c5bc081</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a10ace762c31bc432718efd67b1132e93</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a836d2ae669b00e046f93db946b40639e</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>a0d281361afe4058a78e4e39a66597f4c</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>ac9b8b49732256fde4da5f18f28e88d37</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>aa6082291d3a4e6f9666c6030bd49e0ba</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>ac6a5586e329674017f92c35e6be5e2f8</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a6f5cfd0cbe176a833875695642750d7b</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>ae1b25f2d567329cb7cf023c4a004ccf8</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a8183404624cdc5d7be8b0c358e9480d3</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>ad65122aed1fb594475283526056f0d3d</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a95af476d3773af5d5ac0f5616f92d88f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>aad40f69dfcc726a3ff788ed5a740a0b5</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a1f88db1294ed3f20303390d378e174f0</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a54f36bdd67eb0eca09934c34a9013418</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>add331758b0f8b5cbde92562ee6a46092</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a977f17e2b11b2b595461e44d35e95d41</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>ac4050ff2448ac1fc66bdb73e1adb168a</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>capacity</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a4405349fc3862e105c70de9d9707a1d0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_capacity</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a16aa71cc0ff53fc405dfee4a8d760839</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>af982779b245e6f21b3483f2818d9555d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a7566605c5f8bfe434ce6dbc1f4438606</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a3d76d15732f6253abebd2b1b677a5790</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>DecisionStrategyProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>ae579ff912d8a69c27e673b2daff24a6d</anchor>
      <arglist>(DecisionStrategyProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>DecisionStrategyProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a76e3c16a78d21b34412985b57171ac38</anchor>
      <arglist>(const DecisionStrategyProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>DecisionStrategyProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a5348d9898186d9cda41b8106cfe7f2e3</anchor>
      <arglist>(DecisionStrategyProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>a3e0a19798ee7a04c74e731d9db2d1a74</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a3476bcbfc6b15276741e31351c712373</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a55cc6e2c3ac51349f1111a5cc8c9a5aa</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>a079a253edbca983a7efa1bb3adc8dac2</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>adfe697b70d598477b2153ca82194acaf</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a382ae103b05ecf32151f109f4610f2ae</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a94e44281175e85257bdc857f9eb69524</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a98cdd05180906fc25ce3d99025d6d27c</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a69699f10c3116b4469b986dd6a3977f2</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a5e73c6660bb8db7bea1962afdd60c056</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a3b41ad82b93b7687d5acc4a20b189feb</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a3a23bc5a00e15237112b94c0b5f0eeb9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>af2b5b018600397d1661d9b41305ca319</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedPtrField&lt; ::operations_research::sat::DecisionStrategyProto_AffineTransformation &gt; *</type>
      <name>mutable_transformations</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a7b271fd0ed933997e5f2c685887cf899</anchor>
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
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedPtrField&lt; ::operations_research::sat::DecisionStrategyProto_AffineTransformation &gt; &amp;</type>
      <name>transformations</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a57c6f172bea7811085f2f302cb157593</anchor>
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
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a0b0d9d77806b6143867f4b255d815157</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>abb67e3297bcf4386a109e11543690a00</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a6f8036c47ddd7427185302f9552893e1</anchor>
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
      <type>static const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>VariableSelectionStrategy_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a1f6a9e268798a6043933b4dcc0bfbd7e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const std::string &amp;</type>
      <name>VariableSelectionStrategy_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a34edcda60aeda83651ef9dd64375b938</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>VariableSelectionStrategy_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>aa139a4e1e222b266b27ebc8b5555e61b</anchor>
      <arglist>(const std::string &amp;name, VariableSelectionStrategy *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>DomainReductionStrategy_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a3272dfe841f631b8498e4415bdee7370</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>DomainReductionStrategy_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a13d7d19fe15bde72e9e6f3bb0840a0fb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const std::string &amp;</type>
      <name>DomainReductionStrategy_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>ae6edb8523d36af2e66d9f0f352177195</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>DomainReductionStrategy_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a4d2fb297d926886c9ecb8bc512b05272</anchor>
      <arglist>(const std::string &amp;name, DomainReductionStrategy *value)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>af6aba7f7dbe7d04ac19fc9d50daa2ae5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr VariableSelectionStrategy</type>
      <name>CHOOSE_FIRST</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a349a25beb40192bf26ef4e84f7888d0e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr VariableSelectionStrategy</type>
      <name>CHOOSE_LOWEST_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a695854f176ddd1021f1da5d4a095db57</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr VariableSelectionStrategy</type>
      <name>CHOOSE_HIGHEST_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a2afe83f647784aded7bf8a58f7fbb244</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr VariableSelectionStrategy</type>
      <name>CHOOSE_MIN_DOMAIN_SIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>ac37ebeff543e66aaeed7322c988d9672</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr VariableSelectionStrategy</type>
      <name>CHOOSE_MAX_DOMAIN_SIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a9ba8aeb78f76818517a5ac036124f012</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr VariableSelectionStrategy</type>
      <name>VariableSelectionStrategy_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>addec6e034241e295020d62127a73de7d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr VariableSelectionStrategy</type>
      <name>VariableSelectionStrategy_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a5cdaa984271e380016c7ce2b349f25e5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>VariableSelectionStrategy_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>abfd92c1e695220c76a7d47d5fba03e26</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr DomainReductionStrategy</type>
      <name>SELECT_MIN_VALUE</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a227a045c0771aa000bab4d00832d2c0d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr DomainReductionStrategy</type>
      <name>SELECT_MAX_VALUE</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a00cb1a254d5006a3981586dd8d357509</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr DomainReductionStrategy</type>
      <name>SELECT_LOWER_HALF</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>ac6433780c7703c35425a04f09388fb8b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr DomainReductionStrategy</type>
      <name>SELECT_UPPER_HALF</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a5ac00624c5344965ff9ffbc6c0ee1439</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr DomainReductionStrategy</type>
      <name>DomainReductionStrategy_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a394fe79c6b38f60ad8f59588207ac782</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr DomainReductionStrategy</type>
      <name>DomainReductionStrategy_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>aa7440c60315b99dc77ff31ba370ae5f0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>DomainReductionStrategy_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>aa392c4ff969726a55293bba902601f9d</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>DecisionStrategyProto_AffineTransformation</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a925aa5ec3eafd63623d22529bc514adf</anchor>
      <arglist>(DecisionStrategyProto_AffineTransformation &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>DecisionStrategyProto_AffineTransformation &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>aea5d25cccdfdf1d280f98e086aad7fad</anchor>
      <arglist>(const DecisionStrategyProto_AffineTransformation &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>DecisionStrategyProto_AffineTransformation &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a5e9c6236c89957fccdae6600bf82add9</anchor>
      <arglist>(DecisionStrategyProto_AffineTransformation &amp;&amp;from) noexcept</arglist>
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
      <anchor>a72aaad9045bde09c18da39448dc8dfe3</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a1c510e7b3f83b5f731fad55dd6f0d353</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a406f0e36e2de78c8f0451eb7ca79c751</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>aebe783eaf7423a7f906a151ee41ce6cb</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a984576bdbd7f7fe28fe84f1822fdfe14</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a2fa668d466ffba03961baae9c0f0e772</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a9dd505f4987383d0a6e07b4062c7b7ea</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>ac2eb36ea709d39f0686fc19f1d143f23</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>offset</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a23fb57763d5d459cb99eb65c37534906</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_offset</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a920dac42548ff6627c6c587677bfd2d3</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_positive_coeff</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>ad66affdb829c9b143457e2226f26a587</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>positive_coeff</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a6fb9bd86a210dcee0e7b4d7c42062c26</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_positive_coeff</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a510f2f33ba40f6aa2d87e4680ccf7daa</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_var</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>abc55c23a2546a5a045fcce0ea702e9a9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>var</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a3c7e1ead14616068c90b19d2364bb9e0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_var</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a70dd21749e61b27b15ba01bc86798464</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>aab8cc83289f887dc2bc741c72f0ee49b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a2aa027e6cde24f8328696c2e37056609</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a1ae9f61f0479f7d609ae57eb44995709</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>ElementConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a5e449fbd6146b99c58c6fb64552ea463</anchor>
      <arglist>(ElementConstraintProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>ElementConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>aa2ca4dac3acc0a8ef884dff558557a29</anchor>
      <arglist>(const ElementConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>ElementConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a902db34604b4c6b9a8521ead22d93a22</anchor>
      <arglist>(ElementConstraintProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>a31130afd20e0b5a2a30d66cacd8714bf</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a94dfb457277421a23b9818dcd427d961</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a4296b4f005b32c788ad52ee379a0346a</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>aa923ef352424ab190d870dbf0e9d5d44</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a3cb089872b5eee4236456cd7cd862644</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>ab92f6bd75c52150d0c8cf9b08d7f7554</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a56d8fc24f19c4d6d8e6a0dc99284c5e4</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a454726de5fdccf5bff72c6b8d88414da</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a9e487e77a5f7e3c425c74946f89c28ea</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a0d712102e6d9fa860423518f02d48313</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a72f91dec4a8d196afc6fcbd353ef2f4f</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a8c899c40c4ba35acc5090a3d6a72d8e1</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a766a5a3e579e773d758f9ddfbdf8999b</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>index</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a478995f3850f744dee5803ce00ee3f9b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_index</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a4ad52f48b6b05b910bddab247ff96ddb</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_target</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a6a7c4c7bf8c071597ed13c253233fee4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>target</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>aa6e89e07811cb54a5d60df52b14d4dba</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_target</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>af07859512cdaf9b76078c80678f4e92d</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>af122983c8081273a0a85c715cab2c67c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a5f08f1f03641c492df9d73e665ade434</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a339ff3d44b979528b15bdc34e4c79945</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>IntegerArgumentProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>aa277f2cd42d8ba1ced09c48f112753da</anchor>
      <arglist>(IntegerArgumentProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>IntegerArgumentProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a229cb29935b0f965cb141e4bb8205c8d</anchor>
      <arglist>(const IntegerArgumentProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>IntegerArgumentProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>ad9425aa6488cf6aed9e273667933bea2</anchor>
      <arglist>(IntegerArgumentProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>a02b806ac3fa096c5f4ae1fc2959b5deb</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>ad9d37d0be15b71a4282961be41470d8c</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a9a74ecdd5189ed7d2868942048dce4ed</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>a7438e0db688137c83c4648ba8bd76362</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a266a3227f7d0ff631a79a868320d1ef6</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a7c4349c5e1ee673f0319ecee31f6450a</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a5dc3a40dc56da6219825d385d3fef126</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>ab969fb61af158e5ccf58f58da457ee3a</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>abfa94bd5c8d07e4a18ec5d9c80e9a945</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a8e6044bcefc0ef4e80b41858daf14b8a</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a9bd714e7789291ba5928a80e2f82b45b</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a9a07bb9d0a7da7431a175dac0bdffa36</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a743f40f110cb58c191fa16361951ce43</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>target</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a167d610c9e825a46463cadaeeea807c1</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_target</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a2f627522063b3fc4116edd3f1ec36fe9</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a27dfe37ed4af00ad7798bab6d3241d10</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a1fc06ab2885fe2ac1ff3f5c71a61cdd9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>ab0e753e63f026d3a7fd41a5cec57b7e9</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>IntegerVariableProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a97cf9dda5ca2a67c95c665a22185e150</anchor>
      <arglist>(IntegerVariableProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>IntegerVariableProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>ab076d7e334e142ce3357cedc15798eaf</anchor>
      <arglist>(const IntegerVariableProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>IntegerVariableProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a2fb96970474d540b2e0e1942b79e8766</anchor>
      <arglist>(IntegerVariableProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>a45e0bde7d1189d592ee2f890ea20178c</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a7291eeab3fc5282103ab806bf6b31b3f</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>abaca913e93fa258ba010bc29ac3b0076</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>a65607b84fac1b80a467ce03e54db1886</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>af36e80a50d7fc906d7dba87b3da7f3ad</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>af703937f06aeed5b6b757611c405b7e5</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a45443494264347c5930f0b39c86dbdc0</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a49b79c87a609d425f39373fab2e3c950</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a35f427cbde882e9c925ddabc01f77b62</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a15c19cc15e78288baabb0c1c559753c0</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a698e9f94617bff66cb6645fc7d55d9b2</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; &amp;</type>
      <name>domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a3e0195e30316ee0983f1924cad035a8d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; *</type>
      <name>mutable_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a7cb7586fa6679cdd9a4fa2e2e2757476</anchor>
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
      <type>const std::string &amp;</type>
      <name>name</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a5ae2400b8a1bbf76d789a2dfd6dcfcee</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a72d58a435115ebf6858a7c6714e5dd03</anchor>
      <arglist>(const std::string &amp;value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a976e20695b8ed47ddf197fbee463131d</anchor>
      <arglist>(std::string &amp;&amp;value)</arglist>
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
      <type>std::string *</type>
      <name>mutable_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a5e5e4fd6b4fbf6677cbc2005166ce610</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>std::string *</type>
      <name>release_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>ac78972e516dc09a05ff3e418f19cc9bf</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>ac8cc2c02b64609b9b3532a46d934ed6a</anchor>
      <arglist>(std::string *name)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>ad6907986dd7f8217d394800b5a4abf86</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a65627ed13423b910be3f8bca46beb232</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a345c1422dbd9e02225043b312542bda6</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>IntervalConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>aef1d22d8936e6d378900906d0575dce3</anchor>
      <arglist>(IntervalConstraintProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>IntervalConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a512114cdcc8ed5ad5b2c92c06feacca8</anchor>
      <arglist>(const IntervalConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>IntervalConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>ade249f2b19645786bf013a489e3e6f64</anchor>
      <arglist>(IntervalConstraintProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>aaa3e5d84beb1dec61fc757875e46ae2d</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>ac3dd68811cc04399d5dcb4255abcb2ae</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a412988cb98ea2deef607d579a5482840</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>ac7485b26107450ab45c2a099f3a8c85d</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a7f1c962a830a640c29addf211f8598d2</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>ab923ab8344f3ff8011f1e93d6079807a</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>aa454d8b5e115eae06da9654f2e21fff7</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>adcaee36be878423d8d1d44491c6091b0</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>start</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>acafb802850f5330ebafa68e6e3de989b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_start</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a13a46a0cd59eda5142b7215ded99cf86</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_end</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a78d30b34b538515e18369d5e0a1d268a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>end</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a2316ac0c647646d6d139063b938a8a39</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_end</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a8ea8c19af7450c3c33256f701424aa5c</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>ab47d61370dcce69cc0cbeb1609410165</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>size</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a56a4bcd79422222e8bb98517a908b87d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a3e1a497efcbdd8180427fbf3788797da</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a6a393ef1c7b51f2e0007e2d6b52c56da</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>ad466d99190fc673292ef6cf31084b520</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>ae4652b8283c432b4198fb989d6b30397</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>InverseConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>adbc6f50b0bf5f310f492b4d1eb58327d</anchor>
      <arglist>(InverseConstraintProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>InverseConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a3c836696a5468e2fae84c8e227997719</anchor>
      <arglist>(const InverseConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>InverseConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a5f8d2b3653eda435bc41feed2d85b289</anchor>
      <arglist>(InverseConstraintProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>a0f723d654e9c1b78eeaac15a234e2430</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a88e0a46fbb9f1b6c2c68ee27274ffd26</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>af40707332cdc4685d0e9abcf6584ff80</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>ac4347d3eac7f361ddc3bd829b4a8d411</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a6d02e97f77c629c3e7c8a451f9c8233e</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a849f18a967e370a015fef5896fb23e8b</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>ad8afc40bf050a234c043de0ca8b286a3</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a703a4336ebbef8a3ccdd9abd0c3a6aa6</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>f_direct</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a4ddf6022d3decd2abfb070c5d5f96d12</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_f_direct</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>adae2df639ce277ef0f08824f6e9deea8</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_f_direct</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a47bb1622b00eaa858364faaa23c399ba</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>f_direct</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a470aa43a28c6b1876a0b3723ffb32f2e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_f_direct</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a5dd6ae94e0634fd01255ff6c2119070c</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>f_inverse</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a44450d00fc6338488e868881c4bba97b</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_f_inverse</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a559035c7333f7105143f5857a0dd51ff</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_f_inverse</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a79e53bec72580aaf63996a7ae0519740</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>f_inverse</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a58405504d22e85ddc7228cd87432d1f5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_f_inverse</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>ad38fa48efd8ef89d4a3d1d8f42c50c37</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a5dc73f41a9d9d1506020199a6a4e0ebe</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a4638a65936886b722f0eb8f73327afe4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>ad4e2d512c6c9977b232912d354d82feb</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
    <name>is_proto_enum&lt; ::operations_research::sat::CpSolverStatus &gt;</name>
    <filename>structis__proto__enum_3_01_1_1operations__research_1_1sat_1_1CpSolverStatus_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>is_proto_enum&lt; ::operations_research::sat::DecisionStrategyProto_DomainReductionStrategy &gt;</name>
    <filename>structis__proto__enum_3_01_1_1operations__research_1_1sat_1_1DecisionStrategyProto__DomainReductionStrategy_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>is_proto_enum&lt; ::operations_research::sat::DecisionStrategyProto_VariableSelectionStrategy &gt;</name>
    <filename>structis__proto__enum_3_01_1_1operations__research_1_1sat_1_1DecisionStrategyProto__VariableSelectionStrategy_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>is_proto_enum&lt; ::operations_research::sat::SatParameters_BinaryMinizationAlgorithm &gt;</name>
    <filename>structis__proto__enum_3_01_1_1operations__research_1_1sat_1_1SatParameters__BinaryMinizationAlgorithm_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>is_proto_enum&lt; ::operations_research::sat::SatParameters_ClauseOrdering &gt;</name>
    <filename>structis__proto__enum_3_01_1_1operations__research_1_1sat_1_1SatParameters__ClauseOrdering_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>is_proto_enum&lt; ::operations_research::sat::SatParameters_ClauseProtection &gt;</name>
    <filename>structis__proto__enum_3_01_1_1operations__research_1_1sat_1_1SatParameters__ClauseProtection_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>is_proto_enum&lt; ::operations_research::sat::SatParameters_ConflictMinimizationAlgorithm &gt;</name>
    <filename>structis__proto__enum_3_01_1_1operations__research_1_1sat_1_1SatParameters__ConflictMinimizationAlgorithm_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>is_proto_enum&lt; ::operations_research::sat::SatParameters_MaxSatAssumptionOrder &gt;</name>
    <filename>structis__proto__enum_3_01_1_1operations__research_1_1sat_1_1SatParameters__MaxSatAssumptionOrder_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>is_proto_enum&lt; ::operations_research::sat::SatParameters_MaxSatStratificationAlgorithm &gt;</name>
    <filename>structis__proto__enum_3_01_1_1operations__research_1_1sat_1_1SatParameters__MaxSatStratificationAlgorithm_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>is_proto_enum&lt; ::operations_research::sat::SatParameters_Polarity &gt;</name>
    <filename>structis__proto__enum_3_01_1_1operations__research_1_1sat_1_1SatParameters__Polarity_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>is_proto_enum&lt; ::operations_research::sat::SatParameters_RestartAlgorithm &gt;</name>
    <filename>structis__proto__enum_3_01_1_1operations__research_1_1sat_1_1SatParameters__RestartAlgorithm_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>is_proto_enum&lt; ::operations_research::sat::SatParameters_SearchBranching &gt;</name>
    <filename>structis__proto__enum_3_01_1_1operations__research_1_1sat_1_1SatParameters__SearchBranching_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>is_proto_enum&lt; ::operations_research::sat::SatParameters_VariableOrder &gt;</name>
    <filename>structis__proto__enum_3_01_1_1operations__research_1_1sat_1_1SatParameters__VariableOrder_01_4.html</filename>
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
      <type></type>
      <name>LinearConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a15fc73c1d9e1169210a1d05693ba4c41</anchor>
      <arglist>(LinearConstraintProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>LinearConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a9c8f76e5cbd2626626c02fc2cc95ee93</anchor>
      <arglist>(const LinearConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>LinearConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>acae4f97d26631280037013fe3fdec1e3</anchor>
      <arglist>(LinearConstraintProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>ab02c28153dfbcc2a736c6218bc2cbb1a</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a8cd76fa45dde35a0daabd8f6089c7ba1</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>ab17ed4c2b7dbf148c167ecf82c947526</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>a44347be73701ae82d560097e2377e390</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>af86067fa54e366670d53c8447586503a</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>adcdc620bced54254827fb1cb505e534a</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a0d290df52c40d482c2d0f9aa84761980</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a4906a0a11ce035fc6df718679b97dc96</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a0ce21bd21de20bc62dd99ecfe02f8158</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>ae9221fcd2a7d9cce65b381730982ca3b</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a12c5d8b0f5dd8dd53e5461b8629cc495</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>aa9b730b96040a7cd9abad90175e333b8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>ad47809548a0fc2f1506d80cf8099af78</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a08646e4e945c4ee40fe030b04f362106</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>aee153669434b8548134f251653cb6b06</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>ac0e448a494ebbac4b27edda749339d8b</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; &amp;</type>
      <name>coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a5af044811d7b24753d7dc1457c5d55e1</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; *</type>
      <name>mutable_coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a058673bef6c44167b967b1761d734b4c</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a44e43e6327c74bb905e838e836d402c5</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a35e5a351d91db4c7d6e6eda89247f0b8</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>afeb16a09eb1c4fb0b316a6de573e670e</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; &amp;</type>
      <name>domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a5a1df99e46f4233e2755f4b81969e746</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; *</type>
      <name>mutable_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>aafbcb5580d414f14797a872cfbd40aaa</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>af0d82db2f522bf193f09ce363bf53d1f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a2426daac40a3292cfe33e2b15a43797a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a567badf31ad724104c9da4dff24c5cbb</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>NoOverlap2DConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a1172f2d2dcf7c5082169722f7f3d917d</anchor>
      <arglist>(NoOverlap2DConstraintProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>NoOverlap2DConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>ad88c52d26f57a52b144b97ba00b6a2a5</anchor>
      <arglist>(const NoOverlap2DConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>NoOverlap2DConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a8428b57a7644055596bcb132ab837fcb</anchor>
      <arglist>(NoOverlap2DConstraintProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>a33b1a031bfa21680d4f037c27eef5dd4</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>ab82522bd0a37047d544e54b1a26e5994</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a3e386275789caa5eba7f011aa74976ae</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>af373d7d753b0f71b1c1d04bafa6e16c2</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a41e3c5628aa75ec7d95bc23c28319d47</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>ad2831450cebb5d1a65ef3ea3b8b6836a</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a465b73ed4018e8283a261711fa8e580b</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a65e560f9c955fc057a71cadadab0d806</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>x_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a5dae90d1fb1e966510f477b3fd7de296</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_x_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a8c124aca59c7cb0b24d40dd4b5066a2a</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_x_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a8b07656d51866fac64b48917a3fe6ea3</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>x_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a6e818c0043d494831cad126fcabd7b69</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_x_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a8a210c928ed983a59f823f744d885124</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>y_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>aa3fe2b2f87bb89e2d4e6a47d1f3d1656</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_y_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a8d09d1c57a8ad4982865bae04a6feb85</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_y_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>ad3c4dadc08c4f36aaba943dbd0c1aaa0</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>y_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a5a97c78e3bd7352e36ed611a9290e5d5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_y_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a006765bec18088869bdb311ec852bac8</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>ab71f23f26bf1410097f25d90e472d79a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>aa74c21c7222c834e14ee16d1df46a070</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a2826f84854ea19a60fe4b0fb3aac2bae</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>NoOverlapConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a2889e8663f080b95cae731029fe6e957</anchor>
      <arglist>(NoOverlapConstraintProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>NoOverlapConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a933306ebe007ad7a65f1de3a7573f65c</anchor>
      <arglist>(const NoOverlapConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>NoOverlapConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>ae5a2559d2b1f4a865c62fde8a5b299ff</anchor>
      <arglist>(NoOverlapConstraintProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>ac2c8458cbcb020ee4b68d610d3c90bb4</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a262ed13c3d642bb2ca6d32efe8670136</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>abfba7b3ba2db6f9204d6a8dfb18b52b9</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>a519dc81f785eb97c10c23b11bc90e066</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>abf150aab6fbc9a4c3d0581cc6bbf0aa0</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a76b4e6dccbbd4096a2ebcae75e80021a</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a50e6bcc2746b0bf6b477768041e75433</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>acbb8e91398a505fe3495c1b5298d8019</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a53f7740469a9bd8e4def0068758bcba3</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>ac866d9a7c1e5b39289555c446b907373</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a6a7c16ad4ed281e3f5896996930dda8e</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a482f1afd17f13ff34cce808525929602</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a3df32f2e5e52f5f1c3fc709a8feb7cc8</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>aef06a673d2828624378c53dff605c2b8</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>af7ed26fd574543f39a561cca4947ef11</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a6a19f196e264fb3bcf70ab07d36ccc72</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>PartialVariableAssignment</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ac276d5c490d73db515702b7ae3c78915</anchor>
      <arglist>(PartialVariableAssignment &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>PartialVariableAssignment &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a3822c2fde39cab2adc595da8c1b2f45f</anchor>
      <arglist>(const PartialVariableAssignment &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PartialVariableAssignment &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>aac4a3ad13ee73501620e8b201e37aa2b</anchor>
      <arglist>(PartialVariableAssignment &amp;&amp;from) noexcept</arglist>
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
      <anchor>a9db8f018d5e17b6913edeaaa11aa3651</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ad24eeef9a2bbdd2c56aef7594244a167</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ae69add04f731189b71aae1ff79805818</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>a2ca02674d731864b8475070449c6b109</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a724b86514961292ba1de857fc58ddc90</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>afe7e8791f629227bce705edb19ccfbfc</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ad7cdd1c7cf1a05dc5600ec22f8b284c5</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a578fba150c0d366d654465372252e40f</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>aefeab774151ffaab3864e21b05524858</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>aabef22c98f580a722bf7b9e735ba825e</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a877b1bb708c277a2f07682b19e6dcaf8</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a5a463f13f17e6b7fc1944bdfb2dd82f1</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ad1ea8eae5e928933e7adcaf9349d9ca1</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>values</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a2bced585d102e5c0b44309a0f2ccc8a1</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_values</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>af0579efaa4bfe818dde97c4b4235f226</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_values</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a27361866f00c323c97be322598cdc296</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; &amp;</type>
      <name>values</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a55775f34f56545f00e86a97d94f4dcf4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; *</type>
      <name>mutable_values</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ad84d50c5e7253aae616daeab2aefbad7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>acce6ffc24d51acaa921703b658d4c992</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a570d7c2c03404d4fbcdb613e5ea006bd</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>affede24a65ae67db73bedb4a0e2c49f6</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>ReservoirConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a6df939b382b4530e028d61f1794e2294</anchor>
      <arglist>(ReservoirConstraintProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>ReservoirConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>ad0daab530049b740b2c4ad4dc71813ae</anchor>
      <arglist>(const ReservoirConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>ReservoirConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>ab9737f3bd861c599d59f5e7693e3e234</anchor>
      <arglist>(ReservoirConstraintProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>a51095605c42eaed517c8762be13775d4</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a78bdd3321468e1dde4051fc14884c3f4</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>ab93c2ed02d24fd18e18ba32460d2acf2</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>a186bafd07b4845897f2d92cade39ed68</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>ad446d653bc58747f5109196c90ecdd26</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a5b48930d1d32d8612f47e172c78203a2</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>aa65d88f2677784e0a8839a0f638f3361</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a9adc655050f472034b718de32c5b5394</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>times</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>aa27ee24b4ac91d3aad9297dd9bfd66ca</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_times</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a46cc6d4f1190618ba68a80b2c938003c</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_times</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a4e15fe7bf37df4b8d5df18136cc7fafa</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>times</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a59c27374cb44cf28c6853bf7c3a95a31</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_times</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a22a066e3d0183fe827ab396a2de52b86</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>abffb414670e87056ff70283523ca9111</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>abbc908767462fffa62f2d48c57174c66</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a911c084c988307f3e74235c2f7817523</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; &amp;</type>
      <name>demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>afb4103c6998cca65e55fbf048db83a95</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; *</type>
      <name>mutable_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a2b4f7fa585378a6226d2fe7c67b332b6</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>actives</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a3cdcbaf49de8bbe2f7ca22c08d5b2c35</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_actives</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a69d0c73914c880cf30cd733844bd605a</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_actives</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>ab2c584372bed72149e7fe8d4e105b419</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>actives</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a46b950b46006a063c47ce4ffc7c531ea</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_actives</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a84ee32336ae9d85a393a9de83a642975</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>min_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>aeb9256ec5e8a0f654698ed4420302325</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_min_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a1294a12fc1c7ff217267a316123d4297</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_max_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>af4ebdf4db00d24477b32c8dbc8d6f0be</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>max_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>acdfb8d377e07d0554c00ddfff0c53c06</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_max_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>aeebd5e4f572235cffc852f31428bd9b2</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a85c0548e54d98ad3667acb14c3d88f0c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a437fca83fc79d4f8a6ab3a9027c2fd1c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a5aa47f5ac101589cb8b027960069234f</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>RoutesConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a17a7441bc6ec3ee4918d6861b42fcc5f</anchor>
      <arglist>(RoutesConstraintProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>RoutesConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a07107e6e4490559714e67f598f5dc6e3</anchor>
      <arglist>(const RoutesConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>RoutesConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a9f7e32277cf1647b478a47a49ba560b6</anchor>
      <arglist>(RoutesConstraintProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>a9eeae97f0c3ae1362db856925aee138a</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>addeed518e6913a72e9f4a44b92ef4fa1</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>aaafd9c1d50cc8e6ebaa371ab3779dbd4</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>a1aa5997c9c0cf173051d68213de9c94d</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>aa53aac2a71c4e604599f2671486087ba</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a3656ecf5dd1667b4fbb6399f4ec465a1</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>aa649b5d02b12d1644fa24838c6e7eb05</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a4cacb09f2c4fcad6a24ad36e8fb089f4</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a78f1bb9d85c382195b11f89118fcfa2f</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a5b3524e43e8bee1f5623d797d9a49b75</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a00667eebf43010f5c6547172341768c5</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>aa4eae0065b2bdbd41ccfda1e96f94af5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a14d02441b4a2cb30e716edf1b5d69ae1</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>af212f423c73e244a0366d971323ababf</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a80dfab194c4475eed3a8b26c121a7814</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a04519e764f71fdd3f4b9cbb826139fa5</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>ac10863b9e8d5fc66555471a831faaf30</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>ad149dc83ae58efa5fb69cb0280265575</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a370b960f5cc66a1f1ef1ebba450959e3</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>ab99b69348cacde480ec731ffbae53542</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a98bae32f517be73e678363dc24139ef3</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>af63853ae7f40d06e032ab7faec545482</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a1d5371d312c6e4bb6d5824b39f5d8c64</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>ac1b1c6feca6541fb0d9530d2ecce98a3</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a30b67bcaff0fe4224b3410a18cc41150</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a72fc409ecae763062f258c0bb0e6bf4b</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>afe63352a9495caea002ad883d338bdbf</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a498c73699bcc815d2c720cbd9a3cc5ab</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>capacity</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a21a223f37da2ea5587223937baa07668</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_capacity</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>add7c48e251f27d50481c9097deba2c23</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a2a16933622336a5c40530cc26c97d28c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a3d11284ccc6e48dc6c01d522d2806d1f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a38a9b3053f4e5a30a652a16085ff051b</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>SatParameters</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a10514519099f08853b63cff3cae4fa99</anchor>
      <arglist>(SatParameters &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>SatParameters &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0967633b9de4be2869282456e56c5064</anchor>
      <arglist>(const SatParameters &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>SatParameters &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac191d1ed74bfcdf68191b443c9a5ce46</anchor>
      <arglist>(SatParameters &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a202b9e0c424bcc7ab89ee33899e975ad</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a91040e4fa7123a8f3a6ba487d33cd182</anchor>
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
      <anchor>adc4b099138bae9fc352a6d396a1f919f</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9ee0315962908cb92596cf2ff93af546</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0625ede0beccfe6712965f5b2d1e4de7</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>a48ed39e4fc54c6accd788bab6e72d004</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>adf5d17a9f59c46cf95e49852f103e9d1</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a925d9b18fc13a36011ddc6ed0f282a8a</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac73a18b3427663e2549d5e21c9b909e3</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a67f371cf3efe83f09014dc4ec91626ea</anchor>
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
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; int &gt; &amp;</type>
      <name>restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a192691891f170ce78b84bd290fd0d0f9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; int &gt; *</type>
      <name>mutable_restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5b924328ee39f0d7c91aa824a85ba817</anchor>
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
      <type>const std::string &amp;</type>
      <name>default_restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a545369a07562208a80985583b7c52a08</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_default_restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a93742f7c29f8032cb2cb34d927dcb8c2</anchor>
      <arglist>(const std::string &amp;value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_default_restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7c0c381590c11e84a9fe175644f7f366</anchor>
      <arglist>(std::string &amp;&amp;value)</arglist>
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
      <type>std::string *</type>
      <name>mutable_default_restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aeca983c469ec5a7c9a155040bf790fe4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>std::string *</type>
      <name>release_default_restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad52b0ed823c5c768b125c9568bf5a379</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_default_restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9d62115a885cc0f8ded411034f783fff</anchor>
      <arglist>(std::string *default_restart_algorithms)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>num_conflicts_before_strategy_changes</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a463fb8380d1e39902a299eec0339b3cc</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_num_conflicts_before_strategy_changes</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a30aa37174358221915a82f9f45949693</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>search_randomization_tolerance</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3067a81a730e9e008b0b70940f46a6f9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_search_randomization_tolerance</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa5e2b14b6ed7d6417cab405d45937105</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
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
      <name>has_use_lns_only</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab918690648f23e16d9563d91729f8272</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_use_lns_only</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a70b29a14aa120c08408d4813cbf85ead</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>use_lns_only</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a89f910dd64cf6b396536b23af6dc80c4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_use_lns_only</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae6bd942c9ef74b3c358c00680287a957</anchor>
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
      <name>has_interleave_search</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9d5e04b07ceaec12a8c48a80f89f417c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_interleave_search</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aaf9970f10bfe2567d0e628bcde626f6b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>interleave_search</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>afa21f026097edab6dd0baeaa0781c22c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_interleave_search</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9b5f04092a7b22fefcc89069bff64d41</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_deterministic_parallel_search</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a82a0be33fb5fe07767d28a164f505712</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_deterministic_parallel_search</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af6c8828f6fb61da89aabf7cfbd4d77e5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>deterministic_parallel_search</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aeab34cf2834d41f4a725426af690d2df</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_deterministic_parallel_search</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a25e6bf68229c471e23bbb9337d01a8f7</anchor>
      <arglist>(bool value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>mip_max_activity_exponent</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>abe147ab35c9e2963f93f8d9747300efd</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_mip_max_activity_exponent</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a71030a0f1c74fd0854e2974619204ff4</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>clause_cleanup_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af80e747965e275a56a11d58c1840f0ef</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_clause_cleanup_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0091dd987c6322441eaa8c648f8fb895</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>clause_cleanup_target</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>afcf6077aa3e47e15c38b6fd6784b0da9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_clause_cleanup_target</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a77b09ed710299822b2da8051c06711c5</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>glucose_decay_increment_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a960f7a4b929147546a018775cf287fe8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_glucose_decay_increment_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a047672292c03c458c5aed164b6df737e</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>restart_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad757bb66293d5b9af7c3009abe787cb9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_restart_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7f8923d1b9ef000d38535ed807defe86</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>random_seed</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1578caef67fa09927ec6cf3283d528e6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_random_seed</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab85ded0257cd47919c08c972638687d2</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>max_number_of_conflicts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>afc68a4fe7fdfa4a494172292d3ff7df9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_max_number_of_conflicts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a30818b8ba29e4eb46ddecdbca3337365</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>max_memory_in_mb</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1dd90c9543a33590a5a9e373e4e8300d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_max_memory_in_mb</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a23d5a879c7f6a07f3ef2be2f817c9a6a</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>pb_cleanup_increment</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a859219ce3b976de7f7d29fe3603b7a29</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_pb_cleanup_increment</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a83188a72a5137065ef6e503f4712992a</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>presolve_bve_threshold</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab02aebe26c05c6299af2d0d69b645203</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_presolve_bve_threshold</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af6ba272e4b819616857bde6b9fcf2174</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>presolve_bve_clause_weight</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a55c005e3857895529d6228acb6bc3588</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_presolve_bve_clause_weight</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a19582a1612b7dabbd59a41ec4de8f2ed</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>clause_cleanup_lbd_bound</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a24a484ac7f306852aa26f114decf393d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_clause_cleanup_lbd_bound</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a92ce4d479408909b62cd3972be827721</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>restart_running_window_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a806b9b864116342978994d34fa09fedd</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_restart_running_window_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a898a9ce9a49525d01f0bab548f77f959</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>blocking_restart_window_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a35ea3ba196b62c3c301edb821b5926d7</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_blocking_restart_window_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a57a5c3a1afb9b969926079880a796e21</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>presolve_bva_threshold</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1f2c4e2d9cf303fd9481f849239f3697</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_presolve_bva_threshold</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0e4d4626e9f7711332bf53312a987528</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>linearization_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af0a98b63bfb6d3d974d1235625812f45</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_linearization_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a479d7f837928fb1d3109cda96a5ace3b</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>max_num_cuts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6b9c4341d3433adb5a02fcdf469aaa65</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_max_num_cuts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a764d44a1fbb118e3b17f69f4774c7219</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>minimize_with_propagation_restart_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>adb9da8fa990a6f8202a4fdf155f57158</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_minimize_with_propagation_restart_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a95a1271513447c72cfe99ca774a701f7</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>minimize_with_propagation_num_decisions</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af8bdbd4c57e397c79abfba8357b18010</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_minimize_with_propagation_num_decisions</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a23ab915a47f8bef020c2c6ca845101c2</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>binary_search_num_conflicts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a22598b41445a495208a74941bac0cf76</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_binary_search_num_conflicts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>afb858b77f8166fddb4281aa8be7ec3fe</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>num_search_workers</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac85bba5787bf4ff93bab8fcc212eeabb</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_num_search_workers</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad8006b08158a9f7a415b6525b1b9601e</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>boolean_encoding_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a20b11d0a1b3e895e99e9d22bc2f7646d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_boolean_encoding_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae6f55b803fc56bf6eb980a2e2d5cb654</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>cp_model_probing_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>afaafc2bd7e49e342deb5c9f7326e0977</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_cp_model_probing_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a29fd961109a86f05f1566cfa7990ebe8</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <name>has_catch_sigint_signal</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aeade8433857391c428a5fe2482bd534e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_catch_sigint_signal</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a27a4a184b204a4c248b6411e8da3b8f7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>catch_sigint_signal</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4ee5ea9c6e259a325c6f58003eb2b36e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_catch_sigint_signal</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a15b841f95072176fb4996916efb06cda</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>max_integer_rounding_scaling</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3aee2d5ca527b9b127fea3bcdc4d7fc7</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_max_integer_rounding_scaling</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a62c7db88ac0975d6ccb2adf297cdc175</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>max_inactive_count</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a251a8349e328b59486aef4d46e5544d7</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_max_inactive_count</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac08f42beb7c0dc2bcc619f45f117282d</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>constraint_removal_batch_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a27d70a01c7fd8403d1b5d0a587c9daef</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_constraint_removal_batch_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa5ff14517c248adc111bb24ad3c702b7</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>pseudo_cost_reliability_threshold</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a30e29a94a6530420599dfa28c4fc5214</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_pseudo_cost_reliability_threshold</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab35469e7f8aceb9cd0910536f174fdff</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
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
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a72e7bb63d4b6363fff35b1aa0021975e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a156612be30633dab86669dc0d46c0e6b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a812b5213e915ef4cdcdf1952347d2980</anchor>
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
      <type>static const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>VariableOrder_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a699a4ab94c8606abc4dc210f6fdbca96</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const std::string &amp;</type>
      <name>VariableOrder_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a20498d3f0d5902b7aa42e4277969cebe</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>VariableOrder_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7ebbe6be94f3b6c0028d565faa7b7cba</anchor>
      <arglist>(const std::string &amp;name, VariableOrder *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>Polarity_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1143efde7d013f8220838ea2ae71c887</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>Polarity_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1982e2981258e72b22ef2f3495107311</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const std::string &amp;</type>
      <name>Polarity_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8768e99c21345c33f514a334be34f0c8</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>Polarity_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a765d9e64f5b38d1a586c4aeda871eec4</anchor>
      <arglist>(const std::string &amp;name, Polarity *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>ConflictMinimizationAlgorithm_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3b399cf7366676017073c0f36773138c</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>ConflictMinimizationAlgorithm_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7bee08f50ce6a68ac894ed898c408643</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const std::string &amp;</type>
      <name>ConflictMinimizationAlgorithm_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae096c22b537af54751598904cd4f8a48</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>ConflictMinimizationAlgorithm_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3c0638c78551bf99edebcf29f8ae5383</anchor>
      <arglist>(const std::string &amp;name, ConflictMinimizationAlgorithm *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>BinaryMinizationAlgorithm_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a99a3bc35f1ee438d5034f173476c2232</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>BinaryMinizationAlgorithm_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af2796dfa9022846c716f43e105ced36b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const std::string &amp;</type>
      <name>BinaryMinizationAlgorithm_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a64303bd96e74de80127201d244a176d7</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>BinaryMinizationAlgorithm_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a845426838511816af9aca93a3c06d627</anchor>
      <arglist>(const std::string &amp;name, BinaryMinizationAlgorithm *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>ClauseProtection_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad2d5ffae53df56c4630ffc343851142c</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>ClauseProtection_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6636cd7c449803277bc75421452af67f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const std::string &amp;</type>
      <name>ClauseProtection_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab634a6ce39d4479a246ab7570d19746b</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>ClauseProtection_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6bb81ad185b7e9ab599ed02bf6a15c3e</anchor>
      <arglist>(const std::string &amp;name, ClauseProtection *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>ClauseOrdering_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab95be218b14b8d6d9bbf28a09f545a75</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>ClauseOrdering_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8a256dd17b3c0aacce7eed235c2001d6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const std::string &amp;</type>
      <name>ClauseOrdering_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2e7cd1ecd09a09e51e9d92b32881f6d3</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>ClauseOrdering_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8794003fa170bf864c9bf28376c2ca61</anchor>
      <arglist>(const std::string &amp;name, ClauseOrdering *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>RestartAlgorithm_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6bc12f6092ceecd9403233f081e9ebcd</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>RestartAlgorithm_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af5c36f310f322203917bc2b327fbeaed</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const std::string &amp;</type>
      <name>RestartAlgorithm_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7af1827e1598b546fd616885f8f50418</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>RestartAlgorithm_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a15247bf1aef4237a4bd4275c9a48a130</anchor>
      <arglist>(const std::string &amp;name, RestartAlgorithm *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>MaxSatAssumptionOrder_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2ae770ff4ce1ef6d5c32cad224cd8fa0</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>MaxSatAssumptionOrder_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a80711a2c09fe1c8d062c0a68f79b74f7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const std::string &amp;</type>
      <name>MaxSatAssumptionOrder_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aacaf58a1a74d8c72edd5a6902ed0f8be</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>MaxSatAssumptionOrder_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8210929b912e6b8c5fd22777ff5ec9f9</anchor>
      <arglist>(const std::string &amp;name, MaxSatAssumptionOrder *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>MaxSatStratificationAlgorithm_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a266e70e69fc4191560e696d697d1f39b</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>MaxSatStratificationAlgorithm_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>abdee1b886cf03364e8e14627534f307a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const std::string &amp;</type>
      <name>MaxSatStratificationAlgorithm_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aeeb3819a43afadbbba637a5529469f99</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>MaxSatStratificationAlgorithm_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aee56a43cd5e54cbad9300b097f7cb9ee</anchor>
      <arglist>(const std::string &amp;name, MaxSatStratificationAlgorithm *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>SearchBranching_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae849deb5401a33e85b973ef423548f83</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SearchBranching_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a114dbeff5922ad917bb88b2590e9b17b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const std::string &amp;</type>
      <name>SearchBranching_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9fd1df263e5922b78cd5f8b3ff939fe0</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>SearchBranching_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa5def0bf886a4ebe5660d33ba38703fb</anchor>
      <arglist>(const std::string &amp;name, SearchBranching *value)</arglist>
    </member>
    <member kind="variable">
      <type>static ::PROTOBUF_NAMESPACE_ID::internal::ExplicitlyConstructed&lt; std::string &gt;</type>
      <name>_i_give_permission_to_break_this_code_default_default_restart_algorithms_</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab8c128b64cb3cb39cae2aedc1ddbaa9c</anchor>
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
      <type>static constexpr VariableOrder</type>
      <name>IN_ORDER</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>adad61cb8c48e7cd47c94823daf419b4e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr VariableOrder</type>
      <name>IN_REVERSE_ORDER</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad426422ff60534d9f59c9e13bd511e39</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr VariableOrder</type>
      <name>IN_RANDOM_ORDER</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac90d0823e400a7d7d305738773e65f2f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr VariableOrder</type>
      <name>VariableOrder_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aacd8bf53f3333b2013f2cc1a7e01ea48</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr VariableOrder</type>
      <name>VariableOrder_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0a6dedca5e472d92b2667839889825c5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>VariableOrder_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af24bba72e9fcce633208fbda37ac27db</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr Polarity</type>
      <name>POLARITY_TRUE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>adf6d4457b4e4e62f4a4281fccceb506b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr Polarity</type>
      <name>POLARITY_FALSE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2155720e133e08f6e32365cfa04852c5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr Polarity</type>
      <name>POLARITY_RANDOM</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae1b31252b9fb880bc92d45ca8014246d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr Polarity</type>
      <name>POLARITY_WEIGHTED_SIGN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9a805c6dca8255c45e37b81283780974</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr Polarity</type>
      <name>POLARITY_REVERSE_WEIGHTED_SIGN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a373210e9e33c7de5fd43fb07cc78ac3e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr Polarity</type>
      <name>Polarity_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af8ac098aed6bf7f8a52531d2f655a2af</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr Polarity</type>
      <name>Polarity_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1b03a101da6d03e3d476b1a79f3c9e4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>Polarity_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8407edc9430519fc8067920fa4601e9a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr ConflictMinimizationAlgorithm</type>
      <name>NONE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a11a99e284516133016d4f08e988e8848</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr ConflictMinimizationAlgorithm</type>
      <name>SIMPLE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9206fede2fc1c6d18c268812c11b4386</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr ConflictMinimizationAlgorithm</type>
      <name>RECURSIVE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>afb3bbcb0e2d56baaa610c1d55d2448a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr ConflictMinimizationAlgorithm</type>
      <name>EXPERIMENTAL</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a30c54411c509ab0dd993c4c4f8a4d62c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr ConflictMinimizationAlgorithm</type>
      <name>ConflictMinimizationAlgorithm_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8841141976250822e9e1ae22cdbca3b8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr ConflictMinimizationAlgorithm</type>
      <name>ConflictMinimizationAlgorithm_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3969b6248e8af7ec02e2655490ef6534</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>ConflictMinimizationAlgorithm_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5838e8ff773f89a1b457398877f854a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr BinaryMinizationAlgorithm</type>
      <name>NO_BINARY_MINIMIZATION</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3811f69fdb8c2299a49ccf927e61f896</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr BinaryMinizationAlgorithm</type>
      <name>BINARY_MINIMIZATION_FIRST</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6d6ab41394f8bdef178abadddecd07b8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr BinaryMinizationAlgorithm</type>
      <name>BINARY_MINIMIZATION_FIRST_WITH_TRANSITIVE_REDUCTION</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aacd46dd1106f6a1f1fb33ed83aa02be2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr BinaryMinizationAlgorithm</type>
      <name>BINARY_MINIMIZATION_WITH_REACHABILITY</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a457b59b4e428e89ad36f6b7a3008ca83</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr BinaryMinizationAlgorithm</type>
      <name>EXPERIMENTAL_BINARY_MINIMIZATION</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a158f6663209bf0ac40cdd1b3065fdf71</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr BinaryMinizationAlgorithm</type>
      <name>BinaryMinizationAlgorithm_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2806f19428752bc08e1c488f9b117853</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr BinaryMinizationAlgorithm</type>
      <name>BinaryMinizationAlgorithm_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1f4c2b8d7b3363d4448700c28be529aa</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>BinaryMinizationAlgorithm_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a84a0c817ce5590f619fa3913112d163a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr ClauseProtection</type>
      <name>PROTECTION_NONE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aaa4dbfcffead4532eeadd626e95e3e0e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr ClauseProtection</type>
      <name>PROTECTION_ALWAYS</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa142b0dac67760773d10b35ab0744752</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr ClauseProtection</type>
      <name>PROTECTION_LBD</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a80adef83e0ba836ac7f3313d176a098b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr ClauseProtection</type>
      <name>ClauseProtection_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae763497b6449cec56f0028075b9d9a89</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr ClauseProtection</type>
      <name>ClauseProtection_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a022072bfb734bca33b3066de63714be6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>ClauseProtection_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a13e89948bbfa5c7ab2cd2c0c826069cd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr ClauseOrdering</type>
      <name>CLAUSE_ACTIVITY</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa5ed59a4b434dc7d905bc77d9105ee0c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr ClauseOrdering</type>
      <name>CLAUSE_LBD</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a283d86d150af1fc8ff2391d3cbcfeeee</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr ClauseOrdering</type>
      <name>ClauseOrdering_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a12597794e5b74f5243ccb7247337b332</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr ClauseOrdering</type>
      <name>ClauseOrdering_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4c74dd07e925009cbef07c563f75d223</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>ClauseOrdering_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>acabe8c5c83ae1a24f385f2766401e6ca</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr RestartAlgorithm</type>
      <name>NO_RESTART</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ada783388ee445131473f52990e0d29e1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr RestartAlgorithm</type>
      <name>LUBY_RESTART</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae7ab3368898d3e0a77ccd09102e645c5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr RestartAlgorithm</type>
      <name>DL_MOVING_AVERAGE_RESTART</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a65730fb52dcf4ce5d66419c7179df542</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr RestartAlgorithm</type>
      <name>LBD_MOVING_AVERAGE_RESTART</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac24828a61e9a4ee5f343dfae89d33843</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr RestartAlgorithm</type>
      <name>FIXED_RESTART</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2672c445112fcc4f5570a37d59783bd5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr RestartAlgorithm</type>
      <name>RestartAlgorithm_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac60247aab0bf6144f990259cdb5d2cbf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr RestartAlgorithm</type>
      <name>RestartAlgorithm_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>accc13eb79a2b42694d69a198ea2faf5c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>RestartAlgorithm_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1112b1ae2f5a33f0d5aded507afff519</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr MaxSatAssumptionOrder</type>
      <name>DEFAULT_ASSUMPTION_ORDER</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1c0cc798810280747de3cb5d8da7ab8f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr MaxSatAssumptionOrder</type>
      <name>ORDER_ASSUMPTION_BY_DEPTH</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a44233b88df8f648291109a21413ec1b7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr MaxSatAssumptionOrder</type>
      <name>ORDER_ASSUMPTION_BY_WEIGHT</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3e24268cd192509216e64c3247d3ee4e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr MaxSatAssumptionOrder</type>
      <name>MaxSatAssumptionOrder_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>abe3018b3726717006b1f62acda6098c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr MaxSatAssumptionOrder</type>
      <name>MaxSatAssumptionOrder_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af59e16b3a3534348df927653593cca37</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>MaxSatAssumptionOrder_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae4454454867fbc6b11dcdeb9a11aae62</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr MaxSatStratificationAlgorithm</type>
      <name>STRATIFICATION_NONE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae3e688d4d8665595388024c804d02313</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr MaxSatStratificationAlgorithm</type>
      <name>STRATIFICATION_DESCENT</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a86479bcb0657b9073728d1396d18a97a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr MaxSatStratificationAlgorithm</type>
      <name>STRATIFICATION_ASCENT</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a98a2d480fd9bb1dabfd53d8e76584d6f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr MaxSatStratificationAlgorithm</type>
      <name>MaxSatStratificationAlgorithm_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4ab09c24a9a649c58af0fa763a7f20e0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr MaxSatStratificationAlgorithm</type>
      <name>MaxSatStratificationAlgorithm_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2353c911319a686f3a7e5a4b2d7d098b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>MaxSatStratificationAlgorithm_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab13e4e1d1ac373223422b55433d07ffd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SearchBranching</type>
      <name>AUTOMATIC_SEARCH</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6ef30a08ad64ae30b432d37ff09aaf6c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SearchBranching</type>
      <name>FIXED_SEARCH</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8d16e29509bb32191869438d7bbfb436</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SearchBranching</type>
      <name>PORTFOLIO_SEARCH</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a35dd900bc32a6a2dce80054f9fe95f6e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SearchBranching</type>
      <name>LP_SEARCH</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae5ae9725c90d386a85ae39493b72b8e9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SearchBranching</type>
      <name>PSEUDO_COST_SEARCH</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9971fc10dee1257f9c8453621dc7799c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SearchBranching</type>
      <name>PORTFOLIO_WITH_QUICK_RESTART_SEARCH</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad6a4d8239cdff7440e1c57078d26ef95</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SearchBranching</type>
      <name>HINT_SEARCH</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>abeb9a1a70307b6547beea29c94be0799</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SearchBranching</type>
      <name>SearchBranching_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa0ed785558da77807e90c35fc6d13227</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SearchBranching</type>
      <name>SearchBranching_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad6ece3fc1ac949e81d11fd3aa0ff5257</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>SearchBranching_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a767188f203e3d604715b4b501cd69a4f</anchor>
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
      <name>kMinOrthogonalityForLpConstraintsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a82c16fd139d1d22bc3e3e128a0b6367e</anchor>
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
      <name>kUseLnsOnlyFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8b7db13a78d8770898ab371ff1f67147</anchor>
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
      <name>kInterleaveSearchFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>adf6332c9a16f2c02534a1d684f879d58</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kDeterministicParallelSearchFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae4e49c261144c7c9bfb9e9fad91371a4</anchor>
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
      <name>kNumSearchWorkersFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a43fcfbbe1a76b248772e258ce94c3dc2</anchor>
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
      <name>kCatchSigintSignalFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a970da03f7dabd7898f2e4abbe218e8f4</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type></type>
      <name>TableConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a2df83a90848ba0eb7f50e32beae2cbb8</anchor>
      <arglist>(TableConstraintProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>TableConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a1c026db493b5064e9ce685013912e67f</anchor>
      <arglist>(const TableConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>TableConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a30c497155674d236b3eaccd249508436</anchor>
      <arglist>(TableConstraintProto &amp;&amp;from) noexcept</arglist>
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
      <anchor>a882fecc12bd2d0d3941b12843b680b84</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a6b129a89827687b57c1f6cf8e7b56bd0</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a35e5f3a4997d475832f12b577648d6f0</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
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
      <anchor>abcd1b2506c494838c9558422de0b7723</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a63b6cfb1dbb3d4192511798010360e93</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a2aec5796d37cf916cfc7993a00b338c7</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>aa4c3d4029ea446b64a0f569058bf7ce6</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a9fcc9b44747889c9138112e77d6f6f2d</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a8913fab975f337a838810df126f01689</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>ad7208a4d07f1bdf32fd13fc09956a9bb</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>ac01216912b17e0583d3115dbc5551d88</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>af6132f010a730c8233ec808ca1f32a69</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a26f3b7f29ec0ec1340ea8f0d8a0b8cde</anchor>
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
      <type>::PROTOBUF_NAMESPACE_ID::int64</type>
      <name>values</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a8d723adf3db45a78f51f57a2126eba68</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_values</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>ab815bc07664ff575f4e693b377f0625a</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_values</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>ad862bbb2d31214c25269ec4fbe15e609</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; &amp;</type>
      <name>values</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>ab120f76b7b57a84566fe588cbbeb5ae0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int64 &gt; *</type>
      <name>mutable_values</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a0369e6125e1556a0dc2b7d7da5363e9d</anchor>
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
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a54d5dff03aeac74c96c134b5bd656378</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a7e9ea2067b30723b219a76fdc36ed58f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a1fd5b9f3387f9bdb9aaece4d6b7489e0</anchor>
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
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
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
      <type>static const ::PROTOBUF_NAMESPACE_ID::internal::ParseTableField entries []</type>
      <name>PROTOBUF_SECTION_VARIABLE</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fcp__5fmodel__2eproto.html</anchorfile>
      <anchor>ab2cd557c4dac3e40f17f1b149170fa59</anchor>
      <arglist>(protodesc_cold)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::internal::AuxillaryParseTableField aux []</type>
      <name>PROTOBUF_SECTION_VARIABLE</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fcp__5fmodel__2eproto.html</anchorfile>
      <anchor>ad24d39651bea2cdf9a4cdcd7e8a83838</anchor>
      <arglist>(protodesc_cold)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::internal::ParseTable schema [24]</type>
      <name>PROTOBUF_SECTION_VARIABLE</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fcp__5fmodel__2eproto.html</anchorfile>
      <anchor>a6b76a418b26fed74383dd10053e84c96</anchor>
      <arglist>(protodesc_cold)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::internal::FieldMetadata</type>
      <name>field_metadata</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fcp__5fmodel__2eproto.html</anchorfile>
      <anchor>ad26edd68cb6615a762cbfe94efad2eb6</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::internal::SerializationTable</type>
      <name>serialization_table</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fcp__5fmodel__2eproto.html</anchorfile>
      <anchor>a22a47948e790d2d2d04a586faaa01e54</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::uint32</type>
      <name>offsets</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fcp__5fmodel__2eproto.html</anchorfile>
      <anchor>ad86f34f978df579b088079980476e75d</anchor>
      <arglist>[]</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>TableStruct_ortools_2fsat_2fsat_5fparameters_2eproto</name>
    <filename>structTableStruct__ortools__2fsat__2fsat__5fparameters__2eproto.html</filename>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::internal::ParseTableField entries []</type>
      <name>PROTOBUF_SECTION_VARIABLE</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fsat__5fparameters__2eproto.html</anchorfile>
      <anchor>af29db2b4097b70d5d728461bcfca9ffd</anchor>
      <arglist>(protodesc_cold)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::internal::AuxillaryParseTableField aux []</type>
      <name>PROTOBUF_SECTION_VARIABLE</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fsat__5fparameters__2eproto.html</anchorfile>
      <anchor>a903f3f288a4d1c6e555c186f02482a14</anchor>
      <arglist>(protodesc_cold)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::internal::ParseTable schema [1]</type>
      <name>PROTOBUF_SECTION_VARIABLE</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fsat__5fparameters__2eproto.html</anchorfile>
      <anchor>a6f40f1a585bbc934178959a315ac0d42</anchor>
      <arglist>(protodesc_cold)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::internal::FieldMetadata</type>
      <name>field_metadata</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fsat__5fparameters__2eproto.html</anchorfile>
      <anchor>a5b79f1673db4a0c63db5fe67fc9d558d</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::internal::SerializationTable</type>
      <name>serialization_table</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fsat__5fparameters__2eproto.html</anchorfile>
      <anchor>afc73598fde64f17f1a22e47097b3a6c9</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::uint32</type>
      <name>offsets</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fsat__5fparameters__2eproto.html</anchorfile>
      <anchor>a4178f61ecf4882088d35687afd6e3540</anchor>
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
      <anchor>ac0ca8810a97bcc1b3d45269a33fd4f0c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_FIRST</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac0ca8810a97bcc1b3d45269a33fd4f0ca5e00b7cd6b433ec6a15ff913d3b2c3f3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_LOWEST_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac0ca8810a97bcc1b3d45269a33fd4f0ca0b1d456b36749d677aa4a201b22ba114</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_HIGHEST_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac0ca8810a97bcc1b3d45269a33fd4f0ca79fc0af04ed454750ecb59dc5a748e88</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_MIN_DOMAIN_SIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac0ca8810a97bcc1b3d45269a33fd4f0ca18e573e60bf8dde6880a6cfb9f697ffc</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_MAX_DOMAIN_SIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac0ca8810a97bcc1b3d45269a33fd4f0ca9bc8cd090f555c04c4fb8ec23838dc30</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_DecisionStrategyProto_VariableSelectionStrategy_INT_MIN_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac0ca8810a97bcc1b3d45269a33fd4f0ca77405cd855df69ed653be2766be0a1af</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_DecisionStrategyProto_VariableSelectionStrategy_INT_MAX_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac0ca8810a97bcc1b3d45269a33fd4f0cadecec94c9d1599ecbdfdab2f7cfcb7aa</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>DecisionStrategyProto_DomainReductionStrategy</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a20ead57ac0739497ea66f0c21b23b529</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_SELECT_MIN_VALUE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a20ead57ac0739497ea66f0c21b23b529a2f416e6e94f971bfbb75ba25e7f7b760</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_SELECT_MAX_VALUE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a20ead57ac0739497ea66f0c21b23b529ac22896facd05595ce84133b3b3043685</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_SELECT_LOWER_HALF</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a20ead57ac0739497ea66f0c21b23b529ab63e61aebddafddd1496d6ab577dab53</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_SELECT_UPPER_HALF</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a20ead57ac0739497ea66f0c21b23b529ac41d0ba8114af7179c253fda16e517ca</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_DecisionStrategyProto_DomainReductionStrategy_INT_MIN_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a20ead57ac0739497ea66f0c21b23b529a82875a7d185a8f87d56cb0fb0f37f72a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_DecisionStrategyProto_DomainReductionStrategy_INT_MAX_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a20ead57ac0739497ea66f0c21b23b529ac1c76a18c1405c9569b8afca29919e48</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>CpSolverStatus</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac14a394f629f2cf1070b84bce2e427ce</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UNKNOWN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac14a394f629f2cf1070b84bce2e427cea693e3d1636a488a456c173453c45cc14</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MODEL_INVALID</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac14a394f629f2cf1070b84bce2e427ceacb3300bde58b85d202f9c211dfabcb49</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>FEASIBLE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac14a394f629f2cf1070b84bce2e427ceae4d551fa942cba479e3090bb8ae40e73</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INFEASIBLE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac14a394f629f2cf1070b84bce2e427cea185c2992ead7a0d90d260164cf10d46f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac14a394f629f2cf1070b84bce2e427cea78e9c6b9f6ac60a9e9c2d25967ed1ad0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CpSolverStatus_INT_MIN_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac14a394f629f2cf1070b84bce2e427cea443f059ef1efc767e19c5724f6c161d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CpSolverStatus_INT_MAX_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac14a394f629f2cf1070b84bce2e427ceae535ad44840a077b35974e3a04530717</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_VariableOrder</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a299d9cbf6a19e8aa8294c01b02d59aa7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_VariableOrder_IN_ORDER</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a299d9cbf6a19e8aa8294c01b02d59aa7a92760d7186df85dfd6c188eae0b9b591</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_VariableOrder_IN_REVERSE_ORDER</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a299d9cbf6a19e8aa8294c01b02d59aa7a941215af97625c63a144520ec7e02bfb</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_VariableOrder_IN_RANDOM_ORDER</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a299d9cbf6a19e8aa8294c01b02d59aa7a8de6cbc54e325b78d800c8354591d726</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_Polarity</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a517d73d1db81fd87470e6bcbe87c633e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_Polarity_POLARITY_TRUE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a517d73d1db81fd87470e6bcbe87c633ea6145ecb76ca29dc07b9acde97866a8ee</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_Polarity_POLARITY_FALSE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a517d73d1db81fd87470e6bcbe87c633ea204c91561099609cdf7b6469e84e9576</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_Polarity_POLARITY_RANDOM</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a517d73d1db81fd87470e6bcbe87c633eafaf662755a533bc2353968b4c4da4d32</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_Polarity_POLARITY_WEIGHTED_SIGN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a517d73d1db81fd87470e6bcbe87c633eaf9a6fbf18fc3445083ca746b1e920ca6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_Polarity_POLARITY_REVERSE_WEIGHTED_SIGN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a517d73d1db81fd87470e6bcbe87c633ea77094f18176663ceea0b80667cf917a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_ConflictMinimizationAlgorithm</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ada813507f9879e596a07b3850f7fc0d5</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ConflictMinimizationAlgorithm_NONE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ada813507f9879e596a07b3850f7fc0d5ae1bd62c48ad8f9a7d242ae916bbe5066</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ConflictMinimizationAlgorithm_SIMPLE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ada813507f9879e596a07b3850f7fc0d5ac1adcdd93b988565644ddc9c3510c96c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ConflictMinimizationAlgorithm_RECURSIVE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ada813507f9879e596a07b3850f7fc0d5acf7f9f878c3e92e4e319c3e4ea926af7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ConflictMinimizationAlgorithm_EXPERIMENTAL</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ada813507f9879e596a07b3850f7fc0d5a52b205df52309c4f050206500297e4e5</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_BinaryMinizationAlgorithm</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a402444328a26710265238ae8fb883496</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_BinaryMinizationAlgorithm_NO_BINARY_MINIMIZATION</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a402444328a26710265238ae8fb883496a5cefb853f31166cc3684d90594d5dde9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_BinaryMinizationAlgorithm_BINARY_MINIMIZATION_FIRST</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a402444328a26710265238ae8fb883496acefb9cb334d97dc99896de7db79a2476</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_BinaryMinizationAlgorithm_BINARY_MINIMIZATION_FIRST_WITH_TRANSITIVE_REDUCTION</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a402444328a26710265238ae8fb883496ac586955ded9c943dee2faf8b5b738dbd</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_BinaryMinizationAlgorithm_BINARY_MINIMIZATION_WITH_REACHABILITY</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a402444328a26710265238ae8fb883496a30c30629b82fa4252c40e28942e35416</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_BinaryMinizationAlgorithm_EXPERIMENTAL_BINARY_MINIMIZATION</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a402444328a26710265238ae8fb883496aeb6a38e1f5f44d7f13c6f8d6325ba069</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_ClauseProtection</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af02bc4bd103928ea008623a1da38a12c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ClauseProtection_PROTECTION_NONE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af02bc4bd103928ea008623a1da38a12ca1739f0f3322dc59ebaa2fb9fa3481d6b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ClauseProtection_PROTECTION_ALWAYS</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af02bc4bd103928ea008623a1da38a12caa7de36c91e9668bd4d3429170a3a915a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ClauseProtection_PROTECTION_LBD</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af02bc4bd103928ea008623a1da38a12ca4ce148354b01f5b1e2da32e7576edaa3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_ClauseOrdering</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab33358f8fe7b8cb7f98c226b3a070e38</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ClauseOrdering_CLAUSE_ACTIVITY</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab33358f8fe7b8cb7f98c226b3a070e38aaab0bb6b57e109185e6a62d5d0271a04</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ClauseOrdering_CLAUSE_LBD</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab33358f8fe7b8cb7f98c226b3a070e38a2dcf758b7ee7431577e2aa80a60b163e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_RestartAlgorithm</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2916a603fb108fbf4133f865d472fc0b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_RestartAlgorithm_NO_RESTART</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2916a603fb108fbf4133f865d472fc0ba698c5900a88697e89f1a9ffa790fd49f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_RestartAlgorithm_LUBY_RESTART</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2916a603fb108fbf4133f865d472fc0ba0fcf1821b877dd61f6cfac37a36a82d8</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_RestartAlgorithm_DL_MOVING_AVERAGE_RESTART</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2916a603fb108fbf4133f865d472fc0ba89e7ee47fc5c826c03f455f082f22c70</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_RestartAlgorithm_LBD_MOVING_AVERAGE_RESTART</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2916a603fb108fbf4133f865d472fc0ba5d2302ed4086b87cadaad18aa5981aed</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_RestartAlgorithm_FIXED_RESTART</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2916a603fb108fbf4133f865d472fc0ba353691b5a40f70fe5d05cc01bdf22536</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_MaxSatAssumptionOrder</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a71cb004d78f7d8c38fcc9cbc225af533</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatAssumptionOrder_DEFAULT_ASSUMPTION_ORDER</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a71cb004d78f7d8c38fcc9cbc225af533ab0500c1196441cd7820da82c2c1baf6f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatAssumptionOrder_ORDER_ASSUMPTION_BY_DEPTH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a71cb004d78f7d8c38fcc9cbc225af533a61bc7845a56fecefcc18795a536d5eb3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatAssumptionOrder_ORDER_ASSUMPTION_BY_WEIGHT</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a71cb004d78f7d8c38fcc9cbc225af533a44da070df5c6e2443fa1c00b6c25893f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_MaxSatStratificationAlgorithm</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a011a7400ac03996a9023db2a9e7df81c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatStratificationAlgorithm_STRATIFICATION_NONE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a011a7400ac03996a9023db2a9e7df81ca5bb7f0a112c4672ea2abec407f7d384c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatStratificationAlgorithm_STRATIFICATION_DESCENT</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a011a7400ac03996a9023db2a9e7df81ca0c67cde78d6314de8d13734d65709b3a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatStratificationAlgorithm_STRATIFICATION_ASCENT</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a011a7400ac03996a9023db2a9e7df81cadf547628eb3421e641512aeb95b31912</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_SearchBranching</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af29968605d0dca7194386c85c3e8269e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_AUTOMATIC_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af29968605d0dca7194386c85c3e8269eac23498a3951b707b682de68c3f2ef4ba</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_FIXED_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af29968605d0dca7194386c85c3e8269ea4b402cda1dee9234ecc9bf3f969dae9c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_PORTFOLIO_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af29968605d0dca7194386c85c3e8269ea79d67aaf6b62f71bbddd9c5177ebedc1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_LP_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af29968605d0dca7194386c85c3e8269eac0ee72ff494861f949253aac50496f42</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_PSEUDO_COST_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af29968605d0dca7194386c85c3e8269ea0959d8f131e2610b97a8830464b2c633</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_PORTFOLIO_WITH_QUICK_RESTART_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af29968605d0dca7194386c85c3e8269ea28a2409f7a5ca2ecd6635da22e4e6667</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_HINT_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af29968605d0dca7194386c85c3e8269ea7254e3257c79f6ccdb408e6f26f0c2e1</anchor>
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
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac8eeb3305c37f40da67f55486402ac78</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>abbc472dcbb3ad76095da9926b37e49f8</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a158d3c3e8612a0cb9be525140c96267f</anchor>
      <arglist>(const std::string &amp;name, DecisionStrategyProto_VariableSelectionStrategy *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af161ecb897e60ce83c87c17d11ae7d91</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a60036e4e1e1d47218d6339e9119805c4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac22a3ab628a918dd90466ba12d6ee0cd</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a6f1fd67f2396dd88544958778b9854bf</anchor>
      <arglist>(const std::string &amp;name, DecisionStrategyProto_DomainReductionStrategy *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CpSolverStatus_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8f7f7995f8e9a03c15cdddf39b675702</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>CpSolverStatus_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad87fa7d63870ba0085a841c2303dad6b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>CpSolverStatus_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aede942101121114490d4f59631bf9292</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CpSolverStatus_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a463a1c6294a89434db5de2a5560685f4</anchor>
      <arglist>(const std::string &amp;name, CpSolverStatus *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_VariableOrder_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a711b59624fbd706f0754647084c665d8</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SatParameters_VariableOrder_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a0a75b439d4e889cf84f7d6f6b5a37a86</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>SatParameters_VariableOrder_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9e40adad4a6a75afceefe43c8c509457</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_VariableOrder_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2b5db4bee652895d2a67171ad96cecb7</anchor>
      <arglist>(const std::string &amp;name, SatParameters_VariableOrder *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_Polarity_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a4585806adf77d6f7a56bd21230a31175</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SatParameters_Polarity_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3a1aa6bdfa59980400e6617e6a206071</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>SatParameters_Polarity_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af6e220cb137fc0462fc253744b8bc3ba</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_Polarity_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa8e76d4d2386cfab3cefb7460f62d95c</anchor>
      <arglist>(const std::string &amp;name, SatParameters_Polarity *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a90d6f173fbfa33e26ff6508013c81ffd</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8ae6f7af0b88d08cd83a4ff1a1108985</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af3ae9c39e1b2cf4733a63fb9e4f958b7</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af1125a74a1efaf1562812c9d9b1ffc00</anchor>
      <arglist>(const std::string &amp;name, SatParameters_ConflictMinimizationAlgorithm *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_BinaryMinizationAlgorithm_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3e37f554c39fbb05faf07674ac550f47</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SatParameters_BinaryMinizationAlgorithm_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8e0457f852d7716dc2d913867100dc8c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>SatParameters_BinaryMinizationAlgorithm_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aea747a1c7b91baf6f1b5486700c31e5f</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_BinaryMinizationAlgorithm_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a7c328aaf533ab0b051f9b4617bd47d43</anchor>
      <arglist>(const std::string &amp;name, SatParameters_BinaryMinizationAlgorithm *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ClauseProtection_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac1aa9d5ea93fbc96a68237c2beda3836</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SatParameters_ClauseProtection_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac92d8d18b4148e00e25b463b42c0ea3b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>SatParameters_ClauseProtection_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ae1e232826064de5442ec15d6a2ff90f2</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ClauseProtection_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a45a55c59398241500c1604ed6736e7e0</anchor>
      <arglist>(const std::string &amp;name, SatParameters_ClauseProtection *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ClauseOrdering_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa6f7c43106217e8a55877110b7d87e7c</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SatParameters_ClauseOrdering_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a6763a151acaebadf9a4be9383e91e1eb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>SatParameters_ClauseOrdering_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a87813e257ba880dc079609db5d7f5da4</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ClauseOrdering_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab38e233912e1d6e80baf8fe3bec043ee</anchor>
      <arglist>(const std::string &amp;name, SatParameters_ClauseOrdering *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_RestartAlgorithm_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab199957e5457d8356687f12d67d1aaac</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SatParameters_RestartAlgorithm_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9d2995934edcfcc59a0da77719fcb11b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>SatParameters_RestartAlgorithm_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a34b396f35aa7c449a39d2b92c3f93744</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_RestartAlgorithm_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af97fc1fcba310fb2c415278cef3df03a</anchor>
      <arglist>(const std::string &amp;name, SatParameters_RestartAlgorithm *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_MaxSatAssumptionOrder_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a4104fcd7cb88b2edc4cbc86e6b331cdf</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SatParameters_MaxSatAssumptionOrder_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa36fba890ac5ad3ce86c9f70b8352bb5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>SatParameters_MaxSatAssumptionOrder_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa49899c1c9df530d20f240b519437c6d</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_MaxSatAssumptionOrder_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac304d2e190884ab7f230876fe1bd1d9f</anchor>
      <arglist>(const std::string &amp;name, SatParameters_MaxSatAssumptionOrder *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a5fcee51ba7784a7c403731301af6e14c</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9f22011e31eaf54170afe80d301665ac</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a7b0414d7c022b8a1f606bace4c8192cf</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac4c30c8eeb5c485f9676410745f1d9d2</anchor>
      <arglist>(const std::string &amp;name, SatParameters_MaxSatStratificationAlgorithm *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_SearchBranching_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9018824bcc1b169f32af87ad4faf7561</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SatParameters_SearchBranching_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a05587e288b302e572a8e80b100505a21</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>SatParameters_SearchBranching_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab59fe9a81546232a6951f9c673c02e8a</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_SearchBranching_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ae566d186f92afaced5ffb7ebae02d474</anchor>
      <arglist>(const std::string &amp;name, SatParameters_SearchBranching *value)</arglist>
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
      <type>constexpr DecisionStrategyProto_VariableSelectionStrategy</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_VariableSelectionStrategy_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3e5fd8dd3f65b3725d38e743b450fe14</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr DecisionStrategyProto_VariableSelectionStrategy</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_VariableSelectionStrategy_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3e888f213753f1e8fac882e0a2394040</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_VariableSelectionStrategy_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a6c4f9d19c7865cdcdc3fa9c1ecfd98e8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr DecisionStrategyProto_DomainReductionStrategy</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_DomainReductionStrategy_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>adeada39a9b25093a4cc1883510e1bb08</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr DecisionStrategyProto_DomainReductionStrategy</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_DomainReductionStrategy_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aec9bb98a52b3d32d47a598fc5eafb671</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_DomainReductionStrategy_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a1742cab1f2a807d32238c453b92bdeb3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr CpSolverStatus</type>
      <name>CpSolverStatus_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a067ce64a3f75c8567b22bf8bbecf2fa5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr CpSolverStatus</type>
      <name>CpSolverStatus_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac52096bfb8221d5724ff16dc4c93647c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>CpSolverStatus_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aeccedf377b000af35b4e9091c1bc2bb8</anchor>
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
      <type>constexpr SatParameters_VariableOrder</type>
      <name>SatParameters_VariableOrder_VariableOrder_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2a39eab5a6aadab97bb23a7fb39af600</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_VariableOrder</type>
      <name>SatParameters_VariableOrder_VariableOrder_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a094b77c6089ed1097550980f9ffb764f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>SatParameters_VariableOrder_VariableOrder_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3232d0c544cf356f09b6f8d1b67269e3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_Polarity</type>
      <name>SatParameters_Polarity_Polarity_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>afbfa21e2ce75113388357f29f610342c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_Polarity</type>
      <name>SatParameters_Polarity_Polarity_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a72fe8e22daeacc4a74374d4c34bc09f4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>SatParameters_Polarity_Polarity_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a84b9e2a32889c7bc5476029d4107d736</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_ConflictMinimizationAlgorithm</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_ConflictMinimizationAlgorithm_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ae3d1dd4a33df05f7da9a3ea6c4932c0a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_ConflictMinimizationAlgorithm</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_ConflictMinimizationAlgorithm_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2bfd2dd07fc93d2ebcf90df9982b173f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_ConflictMinimizationAlgorithm_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8a1f2ce9ceb6c6e6ea95e8413c5f304c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_BinaryMinizationAlgorithm</type>
      <name>SatParameters_BinaryMinizationAlgorithm_BinaryMinizationAlgorithm_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab260b9d1bc3bedcc3ad29d6b2fd831d4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_BinaryMinizationAlgorithm</type>
      <name>SatParameters_BinaryMinizationAlgorithm_BinaryMinizationAlgorithm_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a92db718bcc5d276ccf747bde81c78a10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>SatParameters_BinaryMinizationAlgorithm_BinaryMinizationAlgorithm_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a97ac406a44712bd2893b29957f2528d5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_ClauseProtection</type>
      <name>SatParameters_ClauseProtection_ClauseProtection_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aef4cd5f95bfffe8b384372e1cba49049</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_ClauseProtection</type>
      <name>SatParameters_ClauseProtection_ClauseProtection_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>abf9faaf009e6527846e0ff336797f3a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>SatParameters_ClauseProtection_ClauseProtection_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a299e745a341d3282f1f57f930c9d56e1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_ClauseOrdering</type>
      <name>SatParameters_ClauseOrdering_ClauseOrdering_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a6e554645f4d0f9989e1f3d69c1528eea</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_ClauseOrdering</type>
      <name>SatParameters_ClauseOrdering_ClauseOrdering_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>acc0499f1b3c9772bc081ca484c6aa680</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>SatParameters_ClauseOrdering_ClauseOrdering_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aee2d784d894a30c420456d0b389b7970</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_RestartAlgorithm</type>
      <name>SatParameters_RestartAlgorithm_RestartAlgorithm_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a87bcdd92d224942666c7be6e2f936ab0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_RestartAlgorithm</type>
      <name>SatParameters_RestartAlgorithm_RestartAlgorithm_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a955126bc9840983ce5d4faa8d82f1669</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>SatParameters_RestartAlgorithm_RestartAlgorithm_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ae791277565602a13d6e3c8e4ff0e28b9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_MaxSatAssumptionOrder</type>
      <name>SatParameters_MaxSatAssumptionOrder_MaxSatAssumptionOrder_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aedb4be4a6a9caaf8d9161888934ad2d2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_MaxSatAssumptionOrder</type>
      <name>SatParameters_MaxSatAssumptionOrder_MaxSatAssumptionOrder_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ae198f9232534912ddf238f7be789f4aa</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>SatParameters_MaxSatAssumptionOrder_MaxSatAssumptionOrder_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a45e86ed8cbe846e59c55298161086446</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_MaxSatStratificationAlgorithm</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_MaxSatStratificationAlgorithm_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a5df42a6b5c40d46ea317abd561b7ea0b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_MaxSatStratificationAlgorithm</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_MaxSatStratificationAlgorithm_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8665ee9afc158ac57d842bcef9eccc59</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_MaxSatStratificationAlgorithm_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a673309e5337b624e75e496fe33494135</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_SearchBranching</type>
      <name>SatParameters_SearchBranching_SearchBranching_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab90d62c554b3478c3271c929cf81cb59</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr SatParameters_SearchBranching</type>
      <name>SatParameters_SearchBranching_SearchBranching_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac5449564c89e6ffab546725d1d49422a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>SatParameters_SearchBranching_SearchBranching_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3de01c1278d9f16ff4ff5cd72c0233da</anchor>
      <arglist></arglist>
    </member>
  </compound>
</tagfile>
