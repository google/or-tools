<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile>
  <compound kind="file">
    <name>linear_solver.h</name>
    <path>/Users/lperron/Work/or-tools/ortools/linear_solver/</path>
    <filename>linear__solver_8h</filename>
    <includes id="linear__solver_8pb_8h" name="linear_solver.pb.h" local="yes" imported="no">ortools/linear_solver/linear_solver.pb.h</includes>
  </compound>
  <compound kind="file">
    <name>linear_solver.pb.h</name>
    <path>/Users/lperron/Work/or-tools/ortools/gen/ortools/linear_solver/</path>
    <filename>linear__solver_8pb_8h</filename>
    <class kind="struct">TableStruct_ortools_2flinear_5fsolver_2flinear_5fsolver_2eproto</class>
    <class kind="class">operations_research::MPVariableProto</class>
    <class kind="class">operations_research::MPConstraintProto</class>
    <class kind="class">operations_research::MPGeneralConstraintProto</class>
    <class kind="class">operations_research::MPIndicatorConstraint</class>
    <class kind="class">operations_research::PartialVariableAssignment</class>
    <class kind="class">operations_research::MPModelProto</class>
    <class kind="class">operations_research::OptionalDouble</class>
    <class kind="class">operations_research::MPSolverCommonParameters</class>
    <class kind="class">operations_research::MPModelRequest</class>
    <class kind="class">operations_research::MPSolutionResponse</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::MPSolverCommonParameters_LPAlgorithmValues &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::MPModelRequest_SolverType &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::MPSolverResponseStatus &gt;</class>
    <namespace>operations_research</namespace>
    <namespace>google</namespace>
    <namespace>google::protobuf</namespace>
    <member kind="define">
      <type>#define</type>
      <name>PROTOBUF_INTERNAL_EXPORT_ortools_2flinear_5fsolver_2flinear_5fsolver_2eproto</name>
      <anchorfile>linear__solver_8pb_8h.html</anchorfile>
      <anchor>a2ff46d5dc479b9be7968c15b3f932277</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>MPSolverCommonParameters_LPAlgorithmValues</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab042145a1da0eaafbe215ded57dfe85f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSolverCommonParameters_LPAlgorithmValues_LP_ALGO_UNSPECIFIED</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab042145a1da0eaafbe215ded57dfe85fa2218d316cfcac5a88342c95b188f3fda</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSolverCommonParameters_LPAlgorithmValues_LP_ALGO_DUAL</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab042145a1da0eaafbe215ded57dfe85fa129c4c6d32bf9aed2414939cb02ff99a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSolverCommonParameters_LPAlgorithmValues_LP_ALGO_PRIMAL</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab042145a1da0eaafbe215ded57dfe85fa53de34dc95fb67212e335f19dc210516</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSolverCommonParameters_LPAlgorithmValues_LP_ALGO_BARRIER</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab042145a1da0eaafbe215ded57dfe85fa89ff8ffa01928d5993a1414705eecd15</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>MPModelRequest_SolverType</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_GLOP_LINEAR_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5a6fab373696058c6e9f279de4a8446411</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_CLP_LINEAR_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5a0969851c637668f95c10ddb1ade866a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_GLPK_LINEAR_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5aa32d84461e16e800e3f996d6347a304d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_GUROBI_LINEAR_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5a3af34f198d539e787263f9eded0ce0cd</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_CPLEX_LINEAR_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5a4bdeae4b1af8d2cd4aab225db4fc0407</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_SCIP_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5ac25c4844cbdf1e4d7c7efc11f1f8ebf4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_GLPK_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5af60a0830addaf4cf00bc59459fa6647e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_CBC_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5a0e93bcd472e7a9296ff02058ed60f8d1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_GUROBI_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5ac8beb7f7b026823a6bc2e4e87f546da6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_CPLEX_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5a26762918189367f5e171d0e226084d82</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_BOP_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5a67639f2cd42e1197b5ad69a004c93ba3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_SAT_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5ac0fedb2082db5e7c96da01b4149c318e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_KNAPSACK_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5abe010aed8c1b29c5a0fd9ac262ce791f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>MPSolverResponseStatus</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_OPTIMAL</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56ac95cb5be9e36b31647dd28910ac6cae4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_FEASIBLE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56ac7d90afd0518be8cd6433ecad656a83b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_INFEASIBLE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56a0da2dbf49d011970a770d42141819d0a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_UNBOUNDED</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56ad73de4a0f9908a4c0d11246ecccf32b6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_ABNORMAL</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56ac77789af50586fb2f81915dd1cb790b6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_NOT_SOLVED</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56a667b6a5ed42c91ea81fa67c59cb3badb</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_MODEL_IS_VALID</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56a97ee5aaa7f57f286d4a821dd6e57523f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_UNKNOWN_STATUS</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56a84ea2a63b24de389aac6aa33b1203cd4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_MODEL_INVALID</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56a6ae83516a798f1675e1b4daf0d8ea6b1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_MODEL_INVALID_SOLUTION_HINT</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56a0f9da70b2f2b1304313c3a2a5f4876b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_MODEL_INVALID_SOLVER_PARAMETERS</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56ab90169f8480eca12c963af5ce50d36aa</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_SOLVER_TYPE_UNAVAILABLE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56afa008125099beaab382c42682be6bbf9</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddDescriptors_ortools_2flinear_5fsolver_2flinear_5fsolver_2eproto</name>
      <anchorfile>linear__solver_8pb_8h.html</anchorfile>
      <anchor>a7f35194793812465687f4a16273cf830</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a46426d4330f8798191f318e844342edd</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPGeneralConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPGeneralConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ab2f9904d7bb5495c08a6624901891d05</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPIndicatorConstraint *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPIndicatorConstraint &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a09aec13b641f3ff6f0aaa054d9ffb01a</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPModelProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPModelProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a782374154dcade461d694c642d1a118a</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPModelRequest *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPModelRequest &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a218a8e8e9347626cd702a3a9c5e4276f</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPSolutionResponse *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPSolutionResponse &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a97bcf46029da44aea2acff25eeb8f5e5</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPSolverCommonParameters *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPSolverCommonParameters &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a3123875581158bdc0cef8cc7334204c3</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPVariableProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPVariableProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a5241f254a7dd0adf3d39675538cf578f</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::OptionalDouble *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::OptionalDouble &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a57c05b56ab566a13251384836a0ac42e</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::PartialVariableAssignment *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::PartialVariableAssignment &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a62287180108f55a9b20f78b37944b485</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_IsValid</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab3ee5c7a9f799696432b082fd4835232</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_descriptor</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a976efc8cb83ba6997aa984b3c106da17</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_Name</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a317f48b6b35697bf02ead22157c91c52</anchor>
      <arglist>(MPSolverCommonParameters_LPAlgorithmValues value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_Parse</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ac2b888d39ca1974f8485911aa6434144</anchor>
      <arglist>(const ::std::string &amp;name, MPSolverCommonParameters_LPAlgorithmValues *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPModelRequest_SolverType_IsValid</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ad26c438ab5f1b232d7eced80a2780ca0</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>MPModelRequest_SolverType_descriptor</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>aa3fea38c7df3ab9583e34b82878e255c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>MPModelRequest_SolverType_Name</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ad1d017a27f2b89bb55910d1fceb31c64</anchor>
      <arglist>(MPModelRequest_SolverType value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPModelRequest_SolverType_Parse</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a028ee3de18b0c41c98df4de7f38c3543</anchor>
      <arglist>(const ::std::string &amp;name, MPModelRequest_SolverType *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPSolverResponseStatus_IsValid</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a7a295b0760db498bc4fa9479bb8c2329</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>MPSolverResponseStatus_descriptor</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a472faf18ff58cd6640b7b3bf6336d9b6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>MPSolverResponseStatus_Name</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a1225873debe2bc2cb173d365f06ca615</anchor>
      <arglist>(MPSolverResponseStatus value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPSolverResponseStatus_Parse</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a415f14aa6c054ed47d050bd15e725f52</anchor>
      <arglist>(const ::std::string &amp;name, MPSolverResponseStatus *value)</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::MPSolverCommonParameters_LPAlgorithmValues &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>acc185aeb57cb91a0ccac4b25007bdcf9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::MPModelRequest_SolverType &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ab68113b1eb7e078bbcbcec215cb1abff</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::MPSolverResponseStatus &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a9f64efd8b0aaa41a3134261c9575ac01</anchor>
      <arglist>()</arglist>
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
      <type>const MPSolverCommonParameters_LPAlgorithmValues</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_LPAlgorithmValues_MIN</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a1e4803399c53b73b9ae985751803d01a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const MPSolverCommonParameters_LPAlgorithmValues</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_LPAlgorithmValues_MAX</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a35686dabc230ba01c79fb8fd0f457e40</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_LPAlgorithmValues_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a5dc5431b0bd4640975c7f6502e8013d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const MPModelRequest_SolverType</type>
      <name>MPModelRequest_SolverType_SolverType_MIN</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a33e0cbbffcf3c459144e44b3f00dc2bf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const MPModelRequest_SolverType</type>
      <name>MPModelRequest_SolverType_SolverType_MAX</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a955948242965463248545e1785583654</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>MPModelRequest_SolverType_SolverType_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ae6eb74cbdb5037acc1fb265d11616274</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const MPSolverResponseStatus</type>
      <name>MPSolverResponseStatus_MIN</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>aaf5325d95fb273624f43bf2741836834</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const MPSolverResponseStatus</type>
      <name>MPSolverResponseStatus_MAX</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a92f022bd33162332383c5f70e4821498</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>MPSolverResponseStatus_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a04af4e3a977e967ddd2f2db792ac2ad7</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>model_exporter.h</name>
    <path>/Users/lperron/Work/or-tools/ortools/linear_solver/</path>
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
    <path>/Users/lperron/Work/or-tools/ortools/linear_solver/</path>
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
    <path>/Users/lperron/Work/or-tools/ortools/linear_solver/</path>
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
    <name>google::protobuf::is_proto_enum&lt; ::operations_research::MPModelRequest_SolverType &gt;</name>
    <filename>structgoogle_1_1protobuf_1_1is__proto__enum_3_01_1_1operations__research_1_1MPModelRequest__SolverType_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>google::protobuf::is_proto_enum&lt; ::operations_research::MPSolverCommonParameters_LPAlgorithmValues &gt;</name>
    <filename>structgoogle_1_1protobuf_1_1is__proto__enum_3_01_1_1operations__research_1_1MPSolverCommonParameters__LPAlgorithmValues_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>google::protobuf::is_proto_enum&lt; ::operations_research::MPSolverResponseStatus &gt;</name>
    <filename>structgoogle_1_1protobuf_1_1is__proto__enum_3_01_1_1operations__research_1_1MPSolverResponseStatus_01_4.html</filename>
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
      <type>MPConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a5842ee6b7ed4c268cc41d87464a3b181</anchor>
      <arglist>(const MPConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a1ccc1aa516189f11cb3304ecf0439727</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::google::protobuf::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a28d363a4973a30b57653b02fcf4ef112</anchor>
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
      <anchor>a9ca8c647295ebace3e574b8e3d24bee2</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a1e47ef5e1486a0b88263226eef0cc81a</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>ae82d889c54d721b96f33556003e2ccee</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
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
      <anchor>a53b71370fe9f6028e4fdaaa7130867b3</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a50004cea51db571adac38b03d42fb465</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a2237ad5ee4fa949c66068a3bf50e48d2</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a26409443fc3814d1bb7f8447f0fbb7a3</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a50be0160e882c5c5a7b0d285a5cb8433</anchor>
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
      <type>::google::protobuf::int32</type>
      <name>var_index</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>abd590f956c62b7b53558d5485a89afbe</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_var_index</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a6f2666deb52c9f3f75c177e35723d129</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_var_index</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a8c3f7e5768e6019e02732e32ec160138</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>var_index</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a4ddcba5b8c1d08d304f4daf9199c61b4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_var_index</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a82a9ae60780f5b864eba01252d740aff</anchor>
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
      <type>const ::google::protobuf::RepeatedField&lt; double &gt; &amp;</type>
      <name>coefficient</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>acd4aebe6852a0c3ab3c656abbe20e4cf</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; double &gt; *</type>
      <name>mutable_coefficient</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a140e35bcc8eddf4a7cbf8964deaf99ec</anchor>
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
      <type>const ::std::string &amp;</type>
      <name>name</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a8ef40ebe70dd1dd05c12c148ef90ade6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a548d8dfec72ee7116bc14ba967638473</anchor>
      <arglist>(const ::std::string &amp;value)</arglist>
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
      <type>::std::string *</type>
      <name>mutable_name</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a9d6d9d5d6f97aa28b1a51bdcb8933c43</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::std::string *</type>
      <name>release_name</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>aa27185b73e53ba34df82530bdf73e4c2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_name</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a5f9eb552e3bf5ab0c41a5d8e4316c8d3</anchor>
      <arglist>(::std::string *name)</arglist>
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
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a4444b37c56ec0a1ea31c0eab930c2bc2</anchor>
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
      <type>MPGeneralConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>ade51c5fc9c7d152fc527acec3d061648</anchor>
      <arglist>(const MPGeneralConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a4e85d04301ddabea474e41b77a6e75b1</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::google::protobuf::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a2ebf4fef33bac8cc63714f80926209e6</anchor>
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
      <anchor>a041994c580bd05e7e328ac9dd966400d</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>ae14a992ec52211964dbf32b725c15c09</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a76bb8bcb9156fe416fc404e8828ce738</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
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
      <anchor>aa1c9b66adc82cf192f4bd27639fc8e24</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a2d8d43306a0a4d2b55f5b60e17cf34ed</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a3c7eff9e740a18fbd8c60a06f9493e84</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a32726d5a4bebcdd8a84aef4633275184</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>ae12981213adb64548b917a63f40d11b5</anchor>
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
      <type>const ::std::string &amp;</type>
      <name>name</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a2bac5a3122f6bbb7801b9f668da6aeeb</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a814537eab87de4406d08896370239704</anchor>
      <arglist>(const ::std::string &amp;value)</arglist>
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
      <type>::std::string *</type>
      <name>mutable_name</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>aa201de18393af8ca448358bdd7218c18</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::std::string *</type>
      <name>release_name</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a8f7b422a685ef384223cf105564f4e7d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_name</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a32ca12f453bfc19c7f9cc9c37b553066</anchor>
      <arglist>(::std::string *name)</arglist>
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
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>ade54349ab6a27de0e7a315c67fafce78</anchor>
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
      <type>MPIndicatorConstraint &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a63446fecd1ef3f7ffe6b36aa125f9fc2</anchor>
      <arglist>(const MPIndicatorConstraint &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>abc3da54ec40b22676bd20628fcfc9f1a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::google::protobuf::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>add2962171a6c2c2b4f45fdf0923a5cd1</anchor>
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
      <anchor>ad0aa365ecefa1707c49c1a9c3e0ec555</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a689618cda7a67758eff0a26f070ca064</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a5657f41955d56b14020eaa55ea3bc8df</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
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
      <anchor>a6fcd387d738d5f9a68191bc4263904ad</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a4dc781794176fe1fe24e0253aad93101</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a5ee691f4d3f6d92f3e0a99a8aad0c427</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a2cf9578630df932dce4db3227906c402</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a41c7cb8277b09fc54f262abe300fab77</anchor>
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
      <type>::google::protobuf::int32</type>
      <name>var_index</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a30de5bc42ed8cb4be1a0bd8d6fd272e9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_var_index</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a4488cdb709270d9a6e27595eeffdb662</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
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
      <type>::google::protobuf::int32</type>
      <name>var_value</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a65660188fdd8f365f82c44c06a2eced3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_var_value</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>adac9ca27d9eeaf0551a7e40d76db9529</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>ad9eb75be474fee423dd3f9a9644183d2</anchor>
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
      <type>MPModelProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a9a347af6feb7c06b20b867b0a1075d13</anchor>
      <arglist>(const MPModelProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a8336a053894af5868adb472feef3d98a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::google::protobuf::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a9f524907ee617c640e8e24c07839715b</anchor>
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
      <anchor>a69d482bdf65a3e74beb3d2393b3e413c</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a05cc581517f70f4fa70eaa233d58ce96</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a220aa0b5324a08d241e78a1dd59b9c90</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
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
      <anchor>af1ff5fcfaadc6f86f3e0f86955bb2d28</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>afb3fa8727d7352d798df80b7a22a9ac8</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a7e55fca7d3ef983d71b898610dba2301</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>aaebe1b2b0abe2ebb541a2193ac01c5ec</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>aa35460b92f66ac36f28120253704c72e</anchor>
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
      <type>::google::protobuf::RepeatedPtrField&lt; ::operations_research::MPVariableProto &gt; *</type>
      <name>mutable_variable</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a7c1201ae59d012942599c225a4b08c5f</anchor>
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
      <type>const ::google::protobuf::RepeatedPtrField&lt; ::operations_research::MPVariableProto &gt; &amp;</type>
      <name>variable</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>ab4e73296fcd61a0b2ebd4f5992dacaac</anchor>
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
      <type>::google::protobuf::RepeatedPtrField&lt; ::operations_research::MPConstraintProto &gt; *</type>
      <name>mutable_constraint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>af9746796d6e12fb4062e46abaf6408d7</anchor>
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
      <type>const ::google::protobuf::RepeatedPtrField&lt; ::operations_research::MPConstraintProto &gt; &amp;</type>
      <name>constraint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a7de4169939eff84eea61ac205a42325b</anchor>
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
      <type>::google::protobuf::RepeatedPtrField&lt; ::operations_research::MPGeneralConstraintProto &gt; *</type>
      <name>mutable_general_constraint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a052cf91befed98dcbf3fb0e2392d4bb5</anchor>
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
      <type>const ::google::protobuf::RepeatedPtrField&lt; ::operations_research::MPGeneralConstraintProto &gt; &amp;</type>
      <name>general_constraint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a449937899b047d40350f37e90613a6ea</anchor>
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
      <type>const ::std::string &amp;</type>
      <name>name</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>aa5b1db802f9d12052208f0b84da3d154</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>ac8a9945c33e6858b66ca96b8753c44f1</anchor>
      <arglist>(const ::std::string &amp;value)</arglist>
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
      <type>::std::string *</type>
      <name>mutable_name</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a2d85fe9aad80c83e7d904b73da5a06d9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::std::string *</type>
      <name>release_name</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>ad3aca9f5ef2959d6e969001fe32110e7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_name</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a4ab165579ca6b803a8e4e8ac59cdf902</anchor>
      <arglist>(::std::string *name)</arglist>
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
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a99fc5a0026667a5092e36355106f3e57</anchor>
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
      <type>MPModelRequest &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a25dd7d10f6941b9f802956fcf93e6f82</anchor>
      <arglist>(const MPModelRequest &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a96bf3ec4d50146a23f4a6fef56150054</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::google::protobuf::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a1c369a2ef680e13e248e88e5ada68f89</anchor>
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
      <anchor>adb2728d1e9929dcd813e288b611ed4a8</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a2a1db5f17af2bcc8120df6b9579e981a</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>aae6f0181f06f3cdc4d56cf11fc2e3f89</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
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
      <anchor>a82b9166551006fa361145d9f052fdd17</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>ada4fe809193291662e8e0f4dc208cccf</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>ad19b6b803471827d9f227baaea816115</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>ac291e23db35f959dd9bc27656002ba2f</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a9b2a605b190d96fcef6e877241884518</anchor>
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
      <type>const ::std::string &amp;</type>
      <name>solver_specific_parameters</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a8052b14c4df9bd603ae37cbe16a8170a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_solver_specific_parameters</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>ab30c61202c19b92c7e260bcf43742fc7</anchor>
      <arglist>(const ::std::string &amp;value)</arglist>
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
      <type>::std::string *</type>
      <name>mutable_solver_specific_parameters</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>afee47e519007f2309c89d8f2ff8001f2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::std::string *</type>
      <name>release_solver_specific_parameters</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a2feff3d15e814351fbd150cd225f8dd2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_solver_specific_parameters</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a26001ce3010c95d2cd4f084f90c7ecdc</anchor>
      <arglist>(::std::string *solver_specific_parameters)</arglist>
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
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a98bc4d08893e62dc015b1644c5a1cdb4</anchor>
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
      <type>static const ::google::protobuf::EnumDescriptor *</type>
      <name>SolverType_descriptor</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a90c5c3597fba8e409d4719fa4e596045</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::std::string &amp;</type>
      <name>SolverType_Name</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>ae2e6f9e3b4b1eae3ffb22c47fe76f241</anchor>
      <arglist>(SolverType value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>SolverType_Parse</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a3c88508eafad10d79796733e8a93213e</anchor>
      <arglist>(const ::std::string &amp;name, SolverType *value)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a3222e53b28d5734acce946e0fcdaf2a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SolverType</type>
      <name>GLOP_LINEAR_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>af428a16c4c412ec6ebacc9617cff702f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SolverType</type>
      <name>CLP_LINEAR_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a9b2666be8f137679a183a79ac44bd954</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SolverType</type>
      <name>GLPK_LINEAR_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a166cb86f727891d1cf27e56fd361feef</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SolverType</type>
      <name>GUROBI_LINEAR_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a02407a59b8a08d3c9d6793aaf6c32790</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SolverType</type>
      <name>CPLEX_LINEAR_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>aabd0eb9710977c12151f45626552f0be</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SolverType</type>
      <name>SCIP_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a76935d28e2b3cbbbf8715cfa599d4c9a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SolverType</type>
      <name>GLPK_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a775c0aead110ca24f9109ed711f12aae</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SolverType</type>
      <name>CBC_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a8a1951ca60b9cd3980dc5dbc8b78e906</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SolverType</type>
      <name>GUROBI_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>ae9122e5c618331e0990457de96281d68</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SolverType</type>
      <name>CPLEX_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a5d493e08b153dd7dc2aae3411984d751</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SolverType</type>
      <name>BOP_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a6876386800f3f8bb5dfafc0397a8d6e5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SolverType</type>
      <name>SAT_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a0ea1d81351036835bec9d5d3f9c56f59</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SolverType</type>
      <name>KNAPSACK_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>adc15f284fd46dffc07c353d9d8d993ea</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SolverType</type>
      <name>SolverType_MIN</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>aacefe99bb45dbb287c4a23d215e7fa93</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SolverType</type>
      <name>SolverType_MAX</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a85f46bdaebe6d3c872d71a1d46129bfc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>SolverType_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a4a3aae6d6f5272ce477455a0a24ae0fc</anchor>
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
      <type>MPSolutionResponse &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a81401dd9366596f4111c545f7517c091</anchor>
      <arglist>(const MPSolutionResponse &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a5ee46335727baec486aff218d6df52d6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::google::protobuf::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>aa9b685d9b3846757c65623a2c668ace8</anchor>
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
      <anchor>a9bd65f27c9729af22e2550d8fef87535</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a6e713016c5836b8786e3dd8eb76141f8</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>ace35f3b88ee492b7dc7b90c214678a24</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
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
      <anchor>aab4708caff04f7b16baabe663b0477c4</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a13692ab983543a0968d9df4fb036788b</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a1e57e92dda539378793602322b029e23</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a134ca27f7b3a507da53522b9818cec85</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a777355ef23fb36a58c1cfa170b8ff280</anchor>
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
      <type>const ::google::protobuf::RepeatedField&lt; double &gt; &amp;</type>
      <name>variable_value</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a43dd68cddac1c5aabd8aa12953f6611b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; double &gt; *</type>
      <name>mutable_variable_value</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a7022136909d7f9a4f5f0f89be9dea12d</anchor>
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
      <type>const ::google::protobuf::RepeatedField&lt; double &gt; &amp;</type>
      <name>dual_value</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a36e3780a2db3c4b6ef22592072be0937</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; double &gt; *</type>
      <name>mutable_dual_value</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a98f40db050945b91782b64b264c73b79</anchor>
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
      <type>const ::google::protobuf::RepeatedField&lt; double &gt; &amp;</type>
      <name>reduced_cost</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>ad174e6da60bdd326f1e6ceeb3be20db0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; double &gt; *</type>
      <name>mutable_reduced_cost</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>aebd03995f34999e6c321a5df5d105bb3</anchor>
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
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>ae44aa0bbbbe7958e999328c7e14a044b</anchor>
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
      <type>MPSolverCommonParameters &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>af84f57cffdf5b072009d138b985fed4a</anchor>
      <arglist>(const MPSolverCommonParameters &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a3f5a436f7d81683e675287d03a418253</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::google::protobuf::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a5a519d08fa6d92e20492fd15d5a5a89c</anchor>
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
      <anchor>aafd31bf77fc2d7f83ba6500ee2334fed</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>afb47ae0e68ebbfb236e9c84ceeedda32</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>ae456de0b9ce965a6ec7186a34ac3090a</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
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
      <anchor>a8c42678a6a0bfec69e8e8d40b5be2566</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>add1516a72142b04ea8c50f9c3e1d9da6</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>ac2dcfc9501fb2a8bfae2b2b6b8beebdc</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a1ce2dd3b914799b0f7615ac19343e16b</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a66e5ecddbed6348d6643f2ff570e31e5</anchor>
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
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>abbee1eca054fa23efca3dd469bdad856</anchor>
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
      <type>static const ::google::protobuf::EnumDescriptor *</type>
      <name>LPAlgorithmValues_descriptor</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a0bdfe222b5fe442738570ef4de620097</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::std::string &amp;</type>
      <name>LPAlgorithmValues_Name</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>aef7519eb88d0b9694884f56a1b8b59e5</anchor>
      <arglist>(LPAlgorithmValues value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>LPAlgorithmValues_Parse</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a311cb962edaf80e5ecfa860c159f2191</anchor>
      <arglist>(const ::std::string &amp;name, LPAlgorithmValues *value)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a8834cc1a6ae262a7cdb1e9b8ebe3d5d3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const LPAlgorithmValues</type>
      <name>LP_ALGO_UNSPECIFIED</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a4507de9d0be620d79d36e1fc491c193f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const LPAlgorithmValues</type>
      <name>LP_ALGO_DUAL</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a4e000d52d5f8609bde2d86b7242f2871</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const LPAlgorithmValues</type>
      <name>LP_ALGO_PRIMAL</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>ac3d39663ed3ab6d119a762554887dd95</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const LPAlgorithmValues</type>
      <name>LP_ALGO_BARRIER</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a17cf830119c8eec6bf82af43922c7a79</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const LPAlgorithmValues</type>
      <name>LPAlgorithmValues_MIN</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a400e9bd70c165f5b466930d91db4c995</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const LPAlgorithmValues</type>
      <name>LPAlgorithmValues_MAX</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a4e4cfee03566d84e0c29964a46c138ee</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>LPAlgorithmValues_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a726316270672b571630f30b70743312f</anchor>
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
      <type>MPVariableProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>abcbe8216b089c8c7e64e8222625232e3</anchor>
      <arglist>(const MPVariableProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a59492eebc80e07180b125223a2cb1240</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::google::protobuf::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a6226783aa133e32a7be1561cc7995536</anchor>
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
      <anchor>a9ce66d6ad9a4fcb057b808a9da8912cd</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a129d367db52ef98b3c55dd66723890ea</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>ab3a998c513b261c6775ffff2beef4f33</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
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
      <anchor>aa7c4165c873011fc9471588cd6c1f1ee</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a291406c2c172d0eee32a6614a0466b78</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a776ac9529fe870943e6ec095da4fa19b</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a91cd03e19b998dc79c8693cad5a510b3</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a0295dcb0581a10916aea29e57eb50e14</anchor>
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
      <type>const ::std::string &amp;</type>
      <name>name</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a90ce7b2f989daa7d98d73ded4b2ea2bb</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a309981c23311ea4ed46ab93480d3cfe5</anchor>
      <arglist>(const ::std::string &amp;value)</arglist>
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
      <type>::std::string *</type>
      <name>mutable_name</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>ab1261573cff3c47b234a300d34f6452b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::std::string *</type>
      <name>release_name</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a1d607740d3050978d04f31d01296c6b7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_name</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a41173b0267a6158addc75abf280c273a</anchor>
      <arglist>(::std::string *name)</arglist>
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
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>ab454eeff8efd212c02b645969a6f074e</anchor>
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
      <type>OptionalDouble &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a40c297e4ce229e0ddef0a3b407d87ca8</anchor>
      <arglist>(const OptionalDouble &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>ae0cf0868f3dff82ca1ba1c47a27fd886</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::google::protobuf::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a8913bafe2308773777053d850f7fabc1</anchor>
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
      <anchor>aec9a405b68ae6ea9aecf1f3528881cd7</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>adc4e54b9efa5eaf423ededfcf4f9ad9a</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>ae93136b4bd4a067082a9eccea4a90bf2</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
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
      <anchor>a12c4154c1c12f82134674bb82b7bc243</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a2c7bfbe177dd870b48c52018c716986b</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a7132104836e07bf28fe6d0f1609c3e3d</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>abceac6c5e5331b2e7fa0139f44ee549a</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a13ebb3eb0d1c537f618850a0b623e7b7</anchor>
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
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>ab8a1b50c3ad0563cc7eae92fc9ac6cca</anchor>
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
      <type>PartialVariableAssignment &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ad9d885c7b3601ce36c6166cf86b19cc6</anchor>
      <arglist>(const PartialVariableAssignment &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a4aa6b404f26812262154d96b3b3c32bc</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::google::protobuf::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a71b84e11925c3602e6277d1c6ec9d190</anchor>
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
      <anchor>af797ff9c45a1a2a5753225f05286a3a3</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a3805ecad014b8b39f8a5bec076407428</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ac69c9596f2365b0c340e2117b8cd5c29</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
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
      <anchor>a3cb9835fc0f68138132b9169eec0dd39</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ad9a8e892afef882446085fec75d6812a</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>af324d7266c4ebd1d998c369fc4ebdebc</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ad34186104406bfac678cc65149d23b84</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a9124cbb81efbbb3b0ad08299e2de2ae5</anchor>
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
      <type>::google::protobuf::int32</type>
      <name>var_index</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ab9f44a03150702ad296fee4802dc9f3c</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_var_index</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a20cf5ae120651eb3973099a49a6fa3a8</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_var_index</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a408aeda33339211a206ebbceb5a9001c</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>var_index</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a28d3ad55fecca6898480918c1c12c761</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_var_index</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a35eb008d46e30c7c44796be88cdf1258</anchor>
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
      <type>const ::google::protobuf::RepeatedField&lt; double &gt; &amp;</type>
      <name>var_value</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ac97cf17d2b424553cfd5e4a3d33c95f9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; double &gt; *</type>
      <name>mutable_var_value</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>aaa51a66b9749093ff36d7275ff11439a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>abd22bd5cbff19ff39e43652bb97cb4e7</anchor>
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
      <type>static const ::google::protobuf::internal::ParseTableField entries []</type>
      <name>PROTOBUF_SECTION_VARIABLE</name>
      <anchorfile>structTableStruct__ortools__2flinear__5fsolver__2flinear__5fsolver__2eproto.html</anchorfile>
      <anchor>aad7f9cccf25ab5347ccfdd3181c5313c</anchor>
      <arglist>(protodesc_cold)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::internal::AuxillaryParseTableField aux []</type>
      <name>PROTOBUF_SECTION_VARIABLE</name>
      <anchorfile>structTableStruct__ortools__2flinear__5fsolver__2flinear__5fsolver__2eproto.html</anchorfile>
      <anchor>a7f73ad5a3cb84317de11e0fce25b5748</anchor>
      <arglist>(protodesc_cold)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::internal::ParseTable schema [10]</type>
      <name>PROTOBUF_SECTION_VARIABLE</name>
      <anchorfile>structTableStruct__ortools__2flinear__5fsolver__2flinear__5fsolver__2eproto.html</anchorfile>
      <anchor>a36f53a995cc0a98292f46326c1a88ab4</anchor>
      <arglist>(protodesc_cold)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ::google::protobuf::internal::FieldMetadata</type>
      <name>field_metadata</name>
      <anchorfile>structTableStruct__ortools__2flinear__5fsolver__2flinear__5fsolver__2eproto.html</anchorfile>
      <anchor>aa4070ea2442883c40844f0afe9086391</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ::google::protobuf::internal::SerializationTable</type>
      <name>serialization_table</name>
      <anchorfile>structTableStruct__ortools__2flinear__5fsolver__2flinear__5fsolver__2eproto.html</anchorfile>
      <anchor>aad230e47ede8e2ef2572608919accff5</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ::google::protobuf::uint32</type>
      <name>offsets</name>
      <anchorfile>structTableStruct__ortools__2flinear__5fsolver__2flinear__5fsolver__2eproto.html</anchorfile>
      <anchor>a9c8f2578ea4d1b52ca41a188fb1db106</anchor>
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
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::MPModelRequest_SolverType &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::MPSolverCommonParameters_LPAlgorithmValues &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::MPSolverResponseStatus &gt;</class>
    <member kind="function">
      <type>::operations_research::MPConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a46426d4330f8798191f318e844342edd</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPGeneralConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPGeneralConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ab2f9904d7bb5495c08a6624901891d05</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPIndicatorConstraint *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPIndicatorConstraint &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a09aec13b641f3ff6f0aaa054d9ffb01a</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPModelProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPModelProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a782374154dcade461d694c642d1a118a</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPModelRequest *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPModelRequest &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a218a8e8e9347626cd702a3a9c5e4276f</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPSolutionResponse *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPSolutionResponse &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a97bcf46029da44aea2acff25eeb8f5e5</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPSolverCommonParameters *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPSolverCommonParameters &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a3123875581158bdc0cef8cc7334204c3</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPVariableProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPVariableProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a5241f254a7dd0adf3d39675538cf578f</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::OptionalDouble *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::OptionalDouble &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a57c05b56ab566a13251384836a0ac42e</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::PartialVariableAssignment *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::PartialVariableAssignment &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a62287180108f55a9b20f78b37944b485</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::MPSolverCommonParameters_LPAlgorithmValues &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>acc185aeb57cb91a0ccac4b25007bdcf9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::MPModelRequest_SolverType &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ab68113b1eb7e078bbcbcec215cb1abff</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::MPSolverResponseStatus &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a9f64efd8b0aaa41a3134261c9575ac01</anchor>
      <arglist>()</arglist>
    </member>
  </compound>
  <compound kind="namespace">
    <name>operations_research</name>
    <filename>namespaceoperations__research.html</filename>
    <class kind="class">operations_research::MPConstraintProto</class>
    <class kind="class">operations_research::MPGeneralConstraintProto</class>
    <class kind="class">operations_research::MPIndicatorConstraint</class>
    <class kind="struct">operations_research::MPModelExportOptions</class>
    <class kind="class">operations_research::MPModelProto</class>
    <class kind="class">operations_research::MPModelRequest</class>
    <class kind="class">operations_research::MPSolutionResponse</class>
    <class kind="class">operations_research::MPSolverCommonParameters</class>
    <class kind="class">operations_research::MPVariableProto</class>
    <class kind="class">operations_research::OptionalDouble</class>
    <class kind="class">operations_research::PartialVariableAssignment</class>
    <member kind="enumeration">
      <type></type>
      <name>MPSolverCommonParameters_LPAlgorithmValues</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab042145a1da0eaafbe215ded57dfe85f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSolverCommonParameters_LPAlgorithmValues_LP_ALGO_UNSPECIFIED</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab042145a1da0eaafbe215ded57dfe85fa2218d316cfcac5a88342c95b188f3fda</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSolverCommonParameters_LPAlgorithmValues_LP_ALGO_DUAL</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab042145a1da0eaafbe215ded57dfe85fa129c4c6d32bf9aed2414939cb02ff99a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSolverCommonParameters_LPAlgorithmValues_LP_ALGO_PRIMAL</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab042145a1da0eaafbe215ded57dfe85fa53de34dc95fb67212e335f19dc210516</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSolverCommonParameters_LPAlgorithmValues_LP_ALGO_BARRIER</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab042145a1da0eaafbe215ded57dfe85fa89ff8ffa01928d5993a1414705eecd15</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>MPModelRequest_SolverType</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_GLOP_LINEAR_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5a6fab373696058c6e9f279de4a8446411</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_CLP_LINEAR_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5a0969851c637668f95c10ddb1ade866a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_GLPK_LINEAR_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5aa32d84461e16e800e3f996d6347a304d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_GUROBI_LINEAR_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5a3af34f198d539e787263f9eded0ce0cd</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_CPLEX_LINEAR_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5a4bdeae4b1af8d2cd4aab225db4fc0407</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_SCIP_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5ac25c4844cbdf1e4d7c7efc11f1f8ebf4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_GLPK_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5af60a0830addaf4cf00bc59459fa6647e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_CBC_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5a0e93bcd472e7a9296ff02058ed60f8d1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_GUROBI_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5ac8beb7f7b026823a6bc2e4e87f546da6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_CPLEX_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5a26762918189367f5e171d0e226084d82</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_BOP_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5a67639f2cd42e1197b5ad69a004c93ba3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_SAT_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5ac0fedb2082db5e7c96da01b4149c318e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_KNAPSACK_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a66408fd9c4c05711631d208dce3118f5abe010aed8c1b29c5a0fd9ac262ce791f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>MPSolverResponseStatus</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_OPTIMAL</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56ac95cb5be9e36b31647dd28910ac6cae4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_FEASIBLE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56ac7d90afd0518be8cd6433ecad656a83b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_INFEASIBLE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56a0da2dbf49d011970a770d42141819d0a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_UNBOUNDED</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56ad73de4a0f9908a4c0d11246ecccf32b6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_ABNORMAL</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56ac77789af50586fb2f81915dd1cb790b6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_NOT_SOLVED</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56a667b6a5ed42c91ea81fa67c59cb3badb</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_MODEL_IS_VALID</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56a97ee5aaa7f57f286d4a821dd6e57523f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_UNKNOWN_STATUS</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56a84ea2a63b24de389aac6aa33b1203cd4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_MODEL_INVALID</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56a6ae83516a798f1675e1b4daf0d8ea6b1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_MODEL_INVALID_SOLUTION_HINT</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56a0f9da70b2f2b1304313c3a2a5f4876b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_MODEL_INVALID_SOLVER_PARAMETERS</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56ab90169f8480eca12c963af5ce50d36aa</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_SOLVER_TYPE_UNAVAILABLE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a51d0df17eb5fb24fcdd0a134178cde56afa008125099beaab382c42682be6bbf9</anchor>
      <arglist></arglist>
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
      <name>MPSolverCommonParameters_LPAlgorithmValues_IsValid</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab3ee5c7a9f799696432b082fd4835232</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_descriptor</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a976efc8cb83ba6997aa984b3c106da17</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_Name</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a317f48b6b35697bf02ead22157c91c52</anchor>
      <arglist>(MPSolverCommonParameters_LPAlgorithmValues value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_Parse</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ac2b888d39ca1974f8485911aa6434144</anchor>
      <arglist>(const ::std::string &amp;name, MPSolverCommonParameters_LPAlgorithmValues *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPModelRequest_SolverType_IsValid</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ad26c438ab5f1b232d7eced80a2780ca0</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>MPModelRequest_SolverType_descriptor</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>aa3fea38c7df3ab9583e34b82878e255c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>MPModelRequest_SolverType_Name</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ad1d017a27f2b89bb55910d1fceb31c64</anchor>
      <arglist>(MPModelRequest_SolverType value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPModelRequest_SolverType_Parse</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a028ee3de18b0c41c98df4de7f38c3543</anchor>
      <arglist>(const ::std::string &amp;name, MPModelRequest_SolverType *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPSolverResponseStatus_IsValid</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a7a295b0760db498bc4fa9479bb8c2329</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>MPSolverResponseStatus_descriptor</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a472faf18ff58cd6640b7b3bf6336d9b6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>MPSolverResponseStatus_Name</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a1225873debe2bc2cb173d365f06ca615</anchor>
      <arglist>(MPSolverResponseStatus value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPSolverResponseStatus_Parse</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a415f14aa6c054ed47d050bd15e725f52</anchor>
      <arglist>(const ::std::string &amp;name, MPSolverResponseStatus *value)</arglist>
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
      <type>const MPSolverCommonParameters_LPAlgorithmValues</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_LPAlgorithmValues_MIN</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a1e4803399c53b73b9ae985751803d01a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const MPSolverCommonParameters_LPAlgorithmValues</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_LPAlgorithmValues_MAX</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a35686dabc230ba01c79fb8fd0f457e40</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_LPAlgorithmValues_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a5dc5431b0bd4640975c7f6502e8013d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const MPModelRequest_SolverType</type>
      <name>MPModelRequest_SolverType_SolverType_MIN</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a33e0cbbffcf3c459144e44b3f00dc2bf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const MPModelRequest_SolverType</type>
      <name>MPModelRequest_SolverType_SolverType_MAX</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a955948242965463248545e1785583654</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>MPModelRequest_SolverType_SolverType_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ae6eb74cbdb5037acc1fb265d11616274</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const MPSolverResponseStatus</type>
      <name>MPSolverResponseStatus_MIN</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>aaf5325d95fb273624f43bf2741836834</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const MPSolverResponseStatus</type>
      <name>MPSolverResponseStatus_MAX</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a92f022bd33162332383c5f70e4821498</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>MPSolverResponseStatus_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a04af4e3a977e967ddd2f2db792ac2ad7</anchor>
      <arglist></arglist>
    </member>
  </compound>
</tagfile>
