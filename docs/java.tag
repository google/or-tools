<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile>
  <compound kind="file">
    <name>cp_model.py</name>
    <path>/Users/lperron/Work/or-tools/ortools/sat/python/</path>
    <filename>cp__model_8py</filename>
    <class kind="class">ortools::sat::python::cp_model::LinearExpr</class>
    <class kind="class">ortools::sat::python::cp_model::_ProductCst</class>
    <class kind="class">ortools::sat::python::cp_model::_SumArray</class>
    <class kind="class">ortools::sat::python::cp_model::_ScalProd</class>
    <class kind="class">ortools::sat::python::cp_model::IntVar</class>
    <class kind="class">ortools::sat::python::cp_model::_NotBooleanVariable</class>
    <class kind="class">ortools::sat::python::cp_model::BoundedLinearExpression</class>
    <class kind="class">ortools::sat::python::cp_model::Constraint</class>
    <class kind="class">ortools::sat::python::cp_model::IntervalVar</class>
    <class kind="class">ortools::sat::python::cp_model::CpModel</class>
    <class kind="class">ortools::sat::python::cp_model::CpSolver</class>
    <class kind="class">ortools::sat::python::cp_model::CpSolverSolutionCallback</class>
    <class kind="class">ortools::sat::python::cp_model::ObjectiveSolutionPrinter</class>
    <class kind="class">ortools::sat::python::cp_model::VarArrayAndObjectiveSolutionPrinter</class>
    <class kind="class">ortools::sat::python::cp_model::VarArraySolutionPrinter</class>
    <namespace>ortools::sat::python::cp_model</namespace>
    <member kind="function">
      <type>def</type>
      <name>DisplayBounds</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>af51ab275fe6ec1a6a1ecca81a3756f23</anchor>
      <arglist>(bounds)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>ShortName</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>acba81538416dea40488e0bc47d7f1bce</anchor>
      <arglist>(model, i)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>EvaluateLinearExpr</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>adc24b18bcf23f3d0ee27de4fda0d4bbc</anchor>
      <arglist>(expression, solution)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>EvaluateBooleanExpression</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a4f4a79fda7a70ae5d20f444b49fa9235</anchor>
      <arglist>(literal, solution)</arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>Domain</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>aa561f079171b584b0e30a5dcc8355407</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>INT_MIN</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a6ee78cf2c5f4f2ac2a6178692065026d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>INT_MAX</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a300a0abf6c30e041dc44e318806a1d92</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>INT32_MAX</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a6eb5ab64ecf3d84de634c4bcffa6ae6f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>INT32_MIN</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a16a17e52284a461ae9674180f5fec3f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>UNKNOWN</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a0d7ec85f0667ae3498caf24d368c447f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>MODEL_INVALID</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>afdbcf57d5bceaaf00142399327dd83ab</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>FEASIBLE</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a85b2db34962f377ba16ec6f64194a668</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>INFEASIBLE</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>ae2ffaad1f9fcede1a25729e7459f36cf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>OPTIMAL</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>ac939f5fe74d8b27529295ca315bdfa60</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>CHOOSE_FIRST</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>adccb01dd707f7de3f6df3ef4f3aff8f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>CHOOSE_LOWEST_MIN</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a9243469e415b1a356b7d3c2c3d0971d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>CHOOSE_HIGHEST_MAX</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a2bc9ec509993f37f9e8dd6ed2a4ec421</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>tuple</type>
      <name>CHOOSE_MIN_DOMAIN_SIZE</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>ab6c1acb8f211484600d5e6f8c88873ae</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>tuple</type>
      <name>CHOOSE_MAX_DOMAIN_SIZE</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a1091e441a9c06c1d9f15f23449a23c70</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>SELECT_MIN_VALUE</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a05250be64e0373e7908a73256d1f5156</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>SELECT_MAX_VALUE</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>aca5d87b3995a56ee30af4332bbd9e4ab</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>SELECT_LOWER_HALF</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a1769365136c1eeb7e93023e6597d370e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>SELECT_UPPER_HALF</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a101c70c1f9391364f16565daa611d55d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>AUTOMATIC_SEARCH</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>ab0ff22e75f159c943eec0259cc0a94fa</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>FIXED_SEARCH</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>ad97296bce4450fe6edcc6c49b5568c17</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>PORTFOLIO_SEARCH</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>afe97a78be6c27ea1887bba2bc7171cdf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>LP_SEARCH</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a981bb9249d0bf4cdc4e82206671373a4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>cp_model_helper.py</name>
    <path>/Users/lperron/Work/or-tools/ortools/sat/python/</path>
    <filename>cp__model__helper_8py</filename>
    <namespace>ortools::sat::python::cp_model_helper</namespace>
    <member kind="function">
      <type>def</type>
      <name>AssertIsInt64</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model__helper.html</anchorfile>
      <anchor>ae1bc8b7f58746e02d00fceda6fc9cb7a</anchor>
      <arglist>(x)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AssertIsInt32</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model__helper.html</anchorfile>
      <anchor>a14c7d4ab8503eeecd159cf99293569e0</anchor>
      <arglist>(x)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AssertIsBoolean</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model__helper.html</anchorfile>
      <anchor>a3a1864a790a33cda426707d3e008bade</anchor>
      <arglist>(x)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>CapInt64</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model__helper.html</anchorfile>
      <anchor>adeae447da96724fc05787a75e3ea6e52</anchor>
      <arglist>(v)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>CapSub</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model__helper.html</anchorfile>
      <anchor>ac1032dad2dd9fd20d07d07d4d507f966</anchor>
      <arglist>(x, y)</arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>INT_MIN</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model__helper.html</anchorfile>
      <anchor>a8eb5041ef6578acb939595611f02ca9d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>INT_MAX</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model__helper.html</anchorfile>
      <anchor>a54e68cc6699eb765ab10722022e7b6ee</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>INT32_MIN</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model__helper.html</anchorfile>
      <anchor>a3178ea13a815ea65cfc5fa4a98ba354d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>INT32_MAX</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model__helper.html</anchorfile>
      <anchor>aa2cf011afadfc21b61dedc04947d3a1a</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>cp_model_pb2.py</name>
    <path>/Users/lperron/Work/or-tools/ortools/gen/ortools/sat/</path>
    <filename>cp__model__pb2_8py</filename>
    <namespace>cp_model_pb2</namespace>
    <member kind="variable">
      <type></type>
      <name>DESCRIPTOR</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a3b82e474ed9aff0c7ca224f06943a3ee</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>CpSolverStatus</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a7c8075cf164d908fc3473f92cca8771c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>UNKNOWN</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a85ddc4e22375679709030de35d684a69</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>MODEL_INVALID</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a20042bf14439bd22f972416f33012c01</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>FEASIBLE</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a0d5a946aae8dc83ff15b5f36143888e5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>INFEASIBLE</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>ab4c9c603b8ee974a13ca74ebc56ea6d4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>OPTIMAL</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a8f4487ded736edf3c8f706515ac462b6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>message_type</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>aeb868052e90c8726f3e157c41c7d591b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>containing_oneof</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>add652f8e14fe6018c1db62ef4e37ebb8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>containing_type</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>ab32c0752fd1a423229a92f6f2066820e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>enum_type</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>abaf9cd6046cfc380254b89112ae06a05</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>IntegerVariableProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a99f38a6adf366ac7a21c168586a4c172</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>BoolArgumentProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a729cb7daea0c00d1fe2b5f513e4eacc6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>IntegerArgumentProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a5f05e1338d5485af9ac091e86f2d52b2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>AllDifferentConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>af4162d2efebe4c63f3c2372767a7a363</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>LinearConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a555f951bcafcd6f9697566024666eca9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>ElementConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>ab997ab84dfa686a4781c84119a37f4df</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>IntervalConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a95240b4cadf3b6ebc2403399140cfb62</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>NoOverlapConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a547f8da0d83229e28751b1b96b55cfa2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>NoOverlap2DConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a8e96711376196bd71e128fe368b1c19d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>CumulativeConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a9d296b05819f10f319c055746fb24995</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>ReservoirConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>acf9c2c83a4a8d74a66ec5b74b9fe31db</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>CircuitConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a58a76bc08d1f160946c07f8ca7963072</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>RoutesConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a40aa00267a4546d54cb9a740bbf15bde</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>CircuitCoveringConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>ad9990659ab4af7fd8f024cac4dce4c7d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>TableConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>acb46d2c46bf65f4c4afe9baaf2e3d713</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>InverseConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a151eed6d8a0ab2af30eb191ce1e66303</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>AutomatonConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a3e8db83ee219e253e1122be6e7f6a13a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>ConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>af4945cbe4db803e86d070edc6f05aa70</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>CpObjectiveProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a624e2be969bacd96ec4b128b01e1dd86</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>DecisionStrategyProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>ae00c8bfbf29aa5b2ecdb31ecf273817b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>PartialVariableAssignment</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a6e5000310f70e903e62de157274e35cf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>CpModelProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a9b45f2c6923fd752e13791cd0c5162b2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>CpSolverResponse</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a88e7fc6178d7b0154d6e1c1b41ec3ff4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>optional_boolean_pb2.py</name>
    <path>/Users/lperron/Work/or-tools/ortools/gen/ortools/util/</path>
    <filename>optional__boolean__pb2_8py</filename>
    <namespace>optional_boolean_pb2</namespace>
    <member kind="variable">
      <type></type>
      <name>DESCRIPTOR</name>
      <anchorfile>namespaceoptional__boolean__pb2.html</anchorfile>
      <anchor>a752f4bab1b329021567697f4a2f8b183</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>OptionalBoolean</name>
      <anchorfile>namespaceoptional__boolean__pb2.html</anchorfile>
      <anchor>a352156d3d2612850a8ad910de0b844e8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>BOOL_UNSPECIFIED</name>
      <anchorfile>namespaceoptional__boolean__pb2.html</anchorfile>
      <anchor>a5b204b6c1f39553058619ba78057640e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>BOOL_FALSE</name>
      <anchorfile>namespaceoptional__boolean__pb2.html</anchorfile>
      <anchor>a9ec5d7cd566cf9a9e5414e0c5da45a6c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>BOOL_TRUE</name>
      <anchorfile>namespaceoptional__boolean__pb2.html</anchorfile>
      <anchor>a9df97a7ea75c8da10089be0ffd7666e4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>sat_parameters_pb2.py</name>
    <path>/Users/lperron/Work/or-tools/ortools/gen/ortools/sat/</path>
    <filename>sat__parameters__pb2_8py</filename>
    <namespace>sat_parameters_pb2</namespace>
    <member kind="variable">
      <type></type>
      <name>DESCRIPTOR</name>
      <anchorfile>namespacesat__parameters__pb2.html</anchorfile>
      <anchor>ac468144cf2c5f114b2677d02856ba4ca</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>enum_type</name>
      <anchorfile>namespacesat__parameters__pb2.html</anchorfile>
      <anchor>a150c7a2dfac007f9f47e37c2f21cc5c2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>containing_type</name>
      <anchorfile>namespacesat__parameters__pb2.html</anchorfile>
      <anchor>a80f883776f9055456687e6fb1171d642</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>SatParameters</name>
      <anchorfile>namespacesat__parameters__pb2.html</anchorfile>
      <anchor>a5a73e7bf8272a7b5b374bca54beb3b6e</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>sorted_interval_list.py</name>
    <path>/Users/lperron/Work/or-tools/ortools/gen/ortools/util/</path>
    <filename>sorted__interval__list_8py</filename>
    <class kind="class">sorted_interval_list::_object</class>
    <class kind="class">sorted_interval_list::Domain</class>
    <namespace>sorted_interval_list</namespace>
    <member kind="function">
      <type>def</type>
      <name>swig_import_helper</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>a14bb19c00b1f20e0d7bed17a28471bd4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::Domain&quot;</type>
      <name>Domain_AllValues</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>aba1c402d7d63b1fcce95292aa3cfe836</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::Domain&quot;</type>
      <name>Domain_FromValues</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>aebfb4145f9aa735e2803aca9c489a4d3</anchor>
      <arglist>(&apos;std::vector&lt; int64 &gt;&apos; values)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::Domain&quot;</type>
      <name>Domain_FromIntervals</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>a2a36487130d7d8470d89d4f9da21e66f</anchor>
      <arglist>(&apos;std::vector&lt; std::vector&lt; int64 &gt; &gt; const &amp;&apos; intervals)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::Domain&quot;</type>
      <name>Domain_FromFlatIntervals</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>ad7632d9ec0e7f377b107093e62c3c9db</anchor>
      <arglist>(&apos;std::vector&lt; int64 &gt; const &amp;&apos; flat_intervals)</arglist>
    </member>
    <member kind="function">
      <type>&quot;std::ostream &amp;&quot;</type>
      <name>__lshift__</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>a07208216d9c3caa39dd3c82856fa868a</anchor>
      <arglist>(*args)</arglist>
    </member>
    <member kind="variable">
      <type>def</type>
      <name>Domain_swigregister</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>aa378907dc2f41a2a20756019c22ccf80</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>def</type>
      <name>Domain_AllValues</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>a56ebc1695d4c71b4c47ffcbcfce9445e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>def</type>
      <name>Domain_FromValues</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>a1494ecbeeed013d511256f3547c45021</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>def</type>
      <name>Domain_FromIntervals</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>aff0c690beb785512fa3e56435646680d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>def</type>
      <name>Domain_FromFlatIntervals</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>ad3d6a94115982d3f035243d679195860</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>ortools::sat::python::cp_model::_NotBooleanVariable</name>
    <filename>classortools_1_1sat_1_1python_1_1cp__model_1_1__NotBooleanVariable.html</filename>
    <base>ortools::sat::python::cp_model::LinearExpr</base>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1__NotBooleanVariable.html</anchorfile>
      <anchor>a14f4f19636404f0b4644291232507fbd</anchor>
      <arglist>(self, boolvar)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Index</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1__NotBooleanVariable.html</anchorfile>
      <anchor>adec6055fad1ff25d047c9a56de49c438</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Not</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1__NotBooleanVariable.html</anchorfile>
      <anchor>ac084900e4c98b2db4c8e664dab6082bd</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__str__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1__NotBooleanVariable.html</anchorfile>
      <anchor>a32e0dac40dfae00f76845ee3249df0ce</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Sum</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a973beabf6c21a6610b1eec20586f166d</anchor>
      <arglist>(cls, expressions)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>ScalProd</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a3f316b0213f4ec7d685a76b8954cdbb5</anchor>
      <arglist>(cls, expressions, coefficients)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>GetVarValueMap</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a35701f3f9cba6e1c7e8bfc806e3babb0</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__hash__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a62e4c2ea7e3f69b23b3ab3e4ab27ece9</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__abs__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a2cba411c779d8bd31321d7224da8ad4a</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__add__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a5b5fc460604a780d3351ba2c4d6d3189</anchor>
      <arglist>(self, expr)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__radd__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>afc9ccf7e1ddcdd16f0a54431988545f2</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__sub__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>aca05f3cb46757380679eebde01b9ed33</anchor>
      <arglist>(self, expr)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__rsub__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>ab470293c967ced236c2c14efe23e78b9</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__mul__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a21f24356400cc4ab99fa1d3c804983ee</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__rmul__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>aab263dcb9b9c794a489826e3581de945</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__div__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>acf5a2b8a8f9be335c12fdea745508e42</anchor>
      <arglist>(self, _)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__truediv__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a52e3de0e92398c1bf26905819a1e8358</anchor>
      <arglist>(self, _)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__mod__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a2be8fcb5fbe7f6bcda06597b753d0969</anchor>
      <arglist>(self, _)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__neg__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a53a600c215c1911e4c94208537587603</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__eq__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a7f879111ab9c6c3bb657d65e10839cfc</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__ge__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a86b9629ae762267a815652f002287a78</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__le__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a82cb0b1dc448a7a4c2a2d24b8640ecce</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__lt__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a6ab97a5e24d4b1ba769ac1e4caf5d549</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__gt__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a866da212ab25ad65975afd32520fd091</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__ne__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a81f1434405db7bb21d8d5fa12a7e7808</anchor>
      <arglist>(self, arg)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>sorted_interval_list::_object</name>
    <filename>classsorted__interval__list_1_1__object.html</filename>
  </compound>
  <compound kind="class">
    <name>ortools::sat::python::cp_model::_ProductCst</name>
    <filename>classortools_1_1sat_1_1python_1_1cp__model_1_1__ProductCst.html</filename>
    <base>ortools::sat::python::cp_model::LinearExpr</base>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1__ProductCst.html</anchorfile>
      <anchor>a03d776b1cde263992b6bd3d13805b034</anchor>
      <arglist>(self, expr, coef)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__str__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1__ProductCst.html</anchorfile>
      <anchor>ad7d618537c8a1b61b33100fca993b9e6</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__repr__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1__ProductCst.html</anchorfile>
      <anchor>af5ca7b00cf0cc43f9212702ff8577c3a</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Coefficient</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1__ProductCst.html</anchorfile>
      <anchor>abbe555e33ff7e121144752f2a8f19b0f</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Expression</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1__ProductCst.html</anchorfile>
      <anchor>a3ae2b92da91351f92b22cc4d77848a4e</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Sum</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a973beabf6c21a6610b1eec20586f166d</anchor>
      <arglist>(cls, expressions)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>ScalProd</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a3f316b0213f4ec7d685a76b8954cdbb5</anchor>
      <arglist>(cls, expressions, coefficients)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>GetVarValueMap</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a35701f3f9cba6e1c7e8bfc806e3babb0</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__hash__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a62e4c2ea7e3f69b23b3ab3e4ab27ece9</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__abs__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a2cba411c779d8bd31321d7224da8ad4a</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__add__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a5b5fc460604a780d3351ba2c4d6d3189</anchor>
      <arglist>(self, expr)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__radd__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>afc9ccf7e1ddcdd16f0a54431988545f2</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__sub__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>aca05f3cb46757380679eebde01b9ed33</anchor>
      <arglist>(self, expr)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__rsub__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>ab470293c967ced236c2c14efe23e78b9</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__mul__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a21f24356400cc4ab99fa1d3c804983ee</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__rmul__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>aab263dcb9b9c794a489826e3581de945</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__div__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>acf5a2b8a8f9be335c12fdea745508e42</anchor>
      <arglist>(self, _)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__truediv__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a52e3de0e92398c1bf26905819a1e8358</anchor>
      <arglist>(self, _)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__mod__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a2be8fcb5fbe7f6bcda06597b753d0969</anchor>
      <arglist>(self, _)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__neg__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a53a600c215c1911e4c94208537587603</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__eq__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a7f879111ab9c6c3bb657d65e10839cfc</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__ge__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a86b9629ae762267a815652f002287a78</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__le__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a82cb0b1dc448a7a4c2a2d24b8640ecce</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__lt__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a6ab97a5e24d4b1ba769ac1e4caf5d549</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__gt__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a866da212ab25ad65975afd32520fd091</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__ne__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a81f1434405db7bb21d8d5fa12a7e7808</anchor>
      <arglist>(self, arg)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>ortools::sat::python::cp_model::_ScalProd</name>
    <filename>classortools_1_1sat_1_1python_1_1cp__model_1_1__ScalProd.html</filename>
    <base>ortools::sat::python::cp_model::LinearExpr</base>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1__ScalProd.html</anchorfile>
      <anchor>ac2f664c59c23c01ac0e92cb36cac8ec7</anchor>
      <arglist>(self, expressions, coefficients)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__str__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1__ScalProd.html</anchorfile>
      <anchor>a7a6f1d6eb6965125e87bf67422245d48</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__repr__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1__ScalProd.html</anchorfile>
      <anchor>a7f05c4d171dd5bd0517096c56a0d7159</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Expressions</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1__ScalProd.html</anchorfile>
      <anchor>a7f56f9019a35c8470d41b4ba101880af</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Coefficients</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1__ScalProd.html</anchorfile>
      <anchor>ac33fde99bfd332b391387e66a75bd386</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Constant</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1__ScalProd.html</anchorfile>
      <anchor>a6dc9685871c50651e8f151dd981a3f25</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Sum</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a973beabf6c21a6610b1eec20586f166d</anchor>
      <arglist>(cls, expressions)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>ScalProd</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a3f316b0213f4ec7d685a76b8954cdbb5</anchor>
      <arglist>(cls, expressions, coefficients)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>GetVarValueMap</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a35701f3f9cba6e1c7e8bfc806e3babb0</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__hash__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a62e4c2ea7e3f69b23b3ab3e4ab27ece9</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__abs__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a2cba411c779d8bd31321d7224da8ad4a</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__add__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a5b5fc460604a780d3351ba2c4d6d3189</anchor>
      <arglist>(self, expr)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__radd__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>afc9ccf7e1ddcdd16f0a54431988545f2</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__sub__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>aca05f3cb46757380679eebde01b9ed33</anchor>
      <arglist>(self, expr)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__rsub__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>ab470293c967ced236c2c14efe23e78b9</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__mul__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a21f24356400cc4ab99fa1d3c804983ee</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__rmul__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>aab263dcb9b9c794a489826e3581de945</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__div__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>acf5a2b8a8f9be335c12fdea745508e42</anchor>
      <arglist>(self, _)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__truediv__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a52e3de0e92398c1bf26905819a1e8358</anchor>
      <arglist>(self, _)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__mod__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a2be8fcb5fbe7f6bcda06597b753d0969</anchor>
      <arglist>(self, _)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__neg__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a53a600c215c1911e4c94208537587603</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__eq__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a7f879111ab9c6c3bb657d65e10839cfc</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__ge__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a86b9629ae762267a815652f002287a78</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__le__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a82cb0b1dc448a7a4c2a2d24b8640ecce</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__lt__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a6ab97a5e24d4b1ba769ac1e4caf5d549</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__gt__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a866da212ab25ad65975afd32520fd091</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__ne__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a81f1434405db7bb21d8d5fa12a7e7808</anchor>
      <arglist>(self, arg)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>ortools::sat::python::cp_model::_SumArray</name>
    <filename>classortools_1_1sat_1_1python_1_1cp__model_1_1__SumArray.html</filename>
    <base>ortools::sat::python::cp_model::LinearExpr</base>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1__SumArray.html</anchorfile>
      <anchor>a4fd141034d2634ef707df2c0049aac31</anchor>
      <arglist>(self, expressions)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__str__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1__SumArray.html</anchorfile>
      <anchor>ac93ff3af85a533be51bc23ff68f7e18c</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__repr__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1__SumArray.html</anchorfile>
      <anchor>adf2bfcc81d393a5aeef99580159ea2ab</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Expressions</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1__SumArray.html</anchorfile>
      <anchor>ab476c0a0407c5050671b602f4b07916a</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Constant</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1__SumArray.html</anchorfile>
      <anchor>af52328ee59182fe61b4023f2049a1754</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Sum</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a973beabf6c21a6610b1eec20586f166d</anchor>
      <arglist>(cls, expressions)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>ScalProd</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a3f316b0213f4ec7d685a76b8954cdbb5</anchor>
      <arglist>(cls, expressions, coefficients)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>GetVarValueMap</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a35701f3f9cba6e1c7e8bfc806e3babb0</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__hash__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a62e4c2ea7e3f69b23b3ab3e4ab27ece9</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__abs__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a2cba411c779d8bd31321d7224da8ad4a</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__add__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a5b5fc460604a780d3351ba2c4d6d3189</anchor>
      <arglist>(self, expr)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__radd__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>afc9ccf7e1ddcdd16f0a54431988545f2</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__sub__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>aca05f3cb46757380679eebde01b9ed33</anchor>
      <arglist>(self, expr)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__rsub__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>ab470293c967ced236c2c14efe23e78b9</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__mul__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a21f24356400cc4ab99fa1d3c804983ee</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__rmul__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>aab263dcb9b9c794a489826e3581de945</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__div__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>acf5a2b8a8f9be335c12fdea745508e42</anchor>
      <arglist>(self, _)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__truediv__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a52e3de0e92398c1bf26905819a1e8358</anchor>
      <arglist>(self, _)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__mod__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a2be8fcb5fbe7f6bcda06597b753d0969</anchor>
      <arglist>(self, _)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__neg__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a53a600c215c1911e4c94208537587603</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__eq__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a7f879111ab9c6c3bb657d65e10839cfc</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__ge__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a86b9629ae762267a815652f002287a78</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__le__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a82cb0b1dc448a7a4c2a2d24b8640ecce</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__lt__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a6ab97a5e24d4b1ba769ac1e4caf5d549</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__gt__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a866da212ab25ad65975afd32520fd091</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__ne__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a81f1434405db7bb21d8d5fa12a7e7808</anchor>
      <arglist>(self, arg)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>ortools::sat::python::cp_model::BoundedLinearExpression</name>
    <filename>classortools_1_1sat_1_1python_1_1cp__model_1_1BoundedLinearExpression.html</filename>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1BoundedLinearExpression.html</anchorfile>
      <anchor>a5cdf075ce89b8258289f7d6a0464317f</anchor>
      <arglist>(self, expr, bounds)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__str__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1BoundedLinearExpression.html</anchorfile>
      <anchor>adb6055b3a5334b59af3b1f3326a9103c</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Expression</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1BoundedLinearExpression.html</anchorfile>
      <anchor>ad98b89057fe605c527117d09881d09be</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Bounds</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1BoundedLinearExpression.html</anchorfile>
      <anchor>a1b623e85e5d9185107b0e5a7aa43c1b3</anchor>
      <arglist>(self)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>ortools::sat::python::cp_model::Constraint</name>
    <filename>classortools_1_1sat_1_1python_1_1cp__model_1_1Constraint.html</filename>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1Constraint.html</anchorfile>
      <anchor>a0550c70e55cd89b379abe665166f447c</anchor>
      <arglist>(self, constraints)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>OnlyEnforceIf</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1Constraint.html</anchorfile>
      <anchor>a79c82d3578d1f32f538d887f298a2eba</anchor>
      <arglist>(self, boolvar)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Index</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1Constraint.html</anchorfile>
      <anchor>af9864fbf813dbce94cec58935878c7e3</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Proto</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1Constraint.html</anchorfile>
      <anchor>a52d3d8592acc583994a361e35213fc8d</anchor>
      <arglist>(self)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>ortools::sat::python::cp_model::CpModel</name>
    <filename>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</filename>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a6ce4c035ad7b750569dabd78eb46b8a6</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>NewIntVar</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a20672fd6fde1e66008dda73e156ce254</anchor>
      <arglist>(self, lb, ub, name)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>NewIntVarFromDomain</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a73e5626eada0970b7e29d25b97034cc4</anchor>
      <arglist>(self, domain, name)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>NewBoolVar</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>abea141dec2d13420e7fc7bcd523fd4b9</anchor>
      <arglist>(self, name)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>NewConstant</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a28ad28c2c374cad115e2f5b3628e786d</anchor>
      <arglist>(self, value)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddLinearConstraint</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>aa10a9fdf77af43677379eb676e2e4c0b</anchor>
      <arglist>(self, linear_expr, lb, ub)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddLinearExpressionInDomain</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a9e4dda68a5de96151a32e5b3da296e13</anchor>
      <arglist>(self, linear_expr, domain)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Add</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a59ed94edf0844d1341a7a438efad4c8d</anchor>
      <arglist>(self, ct)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddAllDifferent</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>ae7bccd54095964cefe4131b27166bf6f</anchor>
      <arglist>(self, variables)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddElement</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>ae7a2721415381cbf18caa3b5e95f2fb8</anchor>
      <arglist>(self, index, variables, target)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddCircuit</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a20173b782ed94679bd5c4bdddc07c8ae</anchor>
      <arglist>(self, arcs)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddAllowedAssignments</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a82e9e24262e940fc07f8d54d4810ffe4</anchor>
      <arglist>(self, variables, tuples_list)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddForbiddenAssignments</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>ab586aa6ba54431efeadf69906c8d3f1a</anchor>
      <arglist>(self, variables, tuples_list)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddAutomaton</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a677f27dffe8da172c49aa79c17df55c9</anchor>
      <arglist>(self, transition_variables, starting_state, final_states, transition_triples)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddInverse</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a6dc19afed7a09f861f37d25d74b58a77</anchor>
      <arglist>(self, variables, inverse_variables)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddReservoirConstraint</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a28b75292c581cd260d5afccf72dd5ff4</anchor>
      <arglist>(self, times, demands, min_level, max_level)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddReservoirConstraintWithActive</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>ac505c1372fab806b69b84e8dbe4235e0</anchor>
      <arglist>(self, times, demands, actives, min_level, max_level)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddMapDomain</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>ae9f652b8cf8f283410a94e9b9c494454</anchor>
      <arglist>(self, var, bool_var_array, offset=0)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddImplication</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a4d23633c1800cdead2a0143a1c5ab65e</anchor>
      <arglist>(self, a, b)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddBoolOr</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a2725bb3df1af8864f5e9f8370eb1ed19</anchor>
      <arglist>(self, literals)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddBoolAnd</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a37169181925182e0e5ad576dd9af5298</anchor>
      <arglist>(self, literals)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddBoolXOr</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a01155534534d786d4c07b7ed95fd052f</anchor>
      <arglist>(self, literals)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddMinEquality</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>aecb79feee04847f0a39e39a4724a74c5</anchor>
      <arglist>(self, target, variables)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddMaxEquality</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a09c04a58dc952c40e2cfbf1d3318f9f8</anchor>
      <arglist>(self, target, variables)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddDivisionEquality</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a97ab2f5ce7784917a5cb2ac771fac3af</anchor>
      <arglist>(self, target, num, denom)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddAbsEquality</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a2d4d020b8f1e30f56c8e8b211218a12a</anchor>
      <arglist>(self, target, var)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddModuloEquality</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a6dfa169ea8ec80d46255ec901af07808</anchor>
      <arglist>(self, target, var, mod)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddMultiplicationEquality</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a994c15d26c34fbe845c76c5497071abf</anchor>
      <arglist>(self, target, variables)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddProdEquality</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>af25eea5c447831eb1828639378d856ea</anchor>
      <arglist>(self, target, variables)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>NewIntervalVar</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a580f8dfa9b8c342afa44c33710e3c951</anchor>
      <arglist>(self, start, size, end, name)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>NewOptionalIntervalVar</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a741dc2aea4d6e09c864a55c06277e637</anchor>
      <arglist>(self, start, size, end, is_present, name)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddNoOverlap</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a12095b98a7b1b4e439c6ff7b16a63186</anchor>
      <arglist>(self, interval_vars)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddNoOverlap2D</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a2b29e161f75832b11244c0f2de1d959f</anchor>
      <arglist>(self, x_intervals, y_intervals)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddCumulative</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>ae33cdbdc60bddd1ba0c6e130672e0529</anchor>
      <arglist>(self, intervals, demands, capacity)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__str__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a639f1e61c4315a803fada4c497e2fa9a</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Proto</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a0ca1262812c3dd67548faab26c10b3d7</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Negated</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a8449e72524d7825733bebaf8962b22ac</anchor>
      <arglist>(self, index)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>GetOrMakeIndex</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a5c8eff19d98b95277c58f434aa109c10</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>GetOrMakeBooleanIndex</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a09c05e25a95809abac94492baa8b5291</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>GetIntervalIndex</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>aa7e52714d03f891060142c1bda264063</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>GetOrMakeIndexFromConstant</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a7fb008c17971c05ce5bb155cab0aa169</anchor>
      <arglist>(self, value)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>VarIndexToVarProto</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>aa4bbf6e639eb64d1e2c26f0b57320035</anchor>
      <arglist>(self, var_index)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Minimize</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a1ccdfdb4117861311e120a3e734ee049</anchor>
      <arglist>(self, obj)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Maximize</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a361f443fda95e89eab5bd5561c597a5a</anchor>
      <arglist>(self, obj)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>HasObjective</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a35d84602b34e89ced73c399e15dbfeb4</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AddDecisionStrategy</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>aa3e75705b3582ede9a641c654ce9e43c</anchor>
      <arglist>(self, variables, var_strategy, domain_strategy)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>ModelStats</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a92e9bf64e77574ef3fe67f45bca652bb</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Validate</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a35954d390b43af01adf84fc08b0546e9</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AssertIsBooleanVariable</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpModel.html</anchorfile>
      <anchor>a05284bb73ee07a60c1127e7b9aa63017</anchor>
      <arglist>(self, x)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>ortools::sat::python::cp_model::CpSolver</name>
    <filename>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolver.html</filename>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolver.html</anchorfile>
      <anchor>ab4e4027d96abf19fb67fb5f36e0472c0</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Solve</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolver.html</anchorfile>
      <anchor>af28e7126044554ae81c041136137af98</anchor>
      <arglist>(self, model)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>SolveWithSolutionCallback</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolver.html</anchorfile>
      <anchor>ac6bb9beea9d756045a2546ba5030ae4a</anchor>
      <arglist>(self, model, callback)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>SearchForAllSolutions</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolver.html</anchorfile>
      <anchor>ae6e1319c3401f8ec41f4dfa95cc0ed0d</anchor>
      <arglist>(self, model, callback)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Value</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolver.html</anchorfile>
      <anchor>ac7c855d7d73f719685557cc380bc7fb6</anchor>
      <arglist>(self, expression)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>BooleanValue</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolver.html</anchorfile>
      <anchor>aabe6691aeaf454d1403a4cef662a00be</anchor>
      <arglist>(self, literal)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>ObjectiveValue</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolver.html</anchorfile>
      <anchor>a5d4c6b0533a3468d74d901018d8a1330</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>BestObjectiveBound</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolver.html</anchorfile>
      <anchor>a20ea24ccf80a36c87ae41400cd0a6248</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>StatusName</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolver.html</anchorfile>
      <anchor>a078a3ae53f1d1a820488394bd8c52528</anchor>
      <arglist>(self, status)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>NumBooleans</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolver.html</anchorfile>
      <anchor>a2cf2a3c585d959f56b706878678c1159</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>NumConflicts</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolver.html</anchorfile>
      <anchor>a16d20e0775caba853039662b3d05a20a</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>NumBranches</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolver.html</anchorfile>
      <anchor>ab5c8b915ecc5c299b2cd1f9b24508532</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>WallTime</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolver.html</anchorfile>
      <anchor>afc407c36ab715bcddb131f6a6c728e64</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>UserTime</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolver.html</anchorfile>
      <anchor>af46703eb825f903034a3d791bc87a4f6</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>ResponseStats</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolver.html</anchorfile>
      <anchor>ac00cdc217e5b644aae3bda8cb9d87a83</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>ResponseProto</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolver.html</anchorfile>
      <anchor>a6726f0c98cadceabe4974245a54cc4c2</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>parameters</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolver.html</anchorfile>
      <anchor>a566bee874a11750a6b583f88836e27bb</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>ortools::sat::python::cp_model::CpSolverSolutionCallback</name>
    <filename>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolverSolutionCallback.html</filename>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolverSolutionCallback.html</anchorfile>
      <anchor>a3415cc066549910349725a9df8cb3696</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>OnSolutionCallback</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolverSolutionCallback.html</anchorfile>
      <anchor>a9e93b1118bcbf01fe482d7cd734308be</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>BooleanValue</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolverSolutionCallback.html</anchorfile>
      <anchor>a037ef23e537c12d86444ac0730ab7304</anchor>
      <arglist>(self, lit)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Value</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolverSolutionCallback.html</anchorfile>
      <anchor>a5fdafe0522600de5aaac297c0ee0ef31</anchor>
      <arglist>(self, expression)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>sorted_interval_list::Domain</name>
    <filename>classsorted__interval__list_1_1Domain.html</filename>
    <base>sorted_interval_list::_object</base>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>a409154221f17cca1eea31317840e1515</anchor>
      <arglist>(self, *args)</arglist>
    </member>
    <member kind="function">
      <type>&quot;std::vector&lt; int64 &gt;&quot;</type>
      <name>FlattenedIntervals</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>a0b4948b5aed9df051e0d7e2913a20a78</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;bool&quot;</type>
      <name>IsEmpty</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>a1c01285d86d94460a7a1476bc1c41449</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;int64&quot;</type>
      <name>Size</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>aad0aa3d3a6766e3b2499abd2af49caf8</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;int64&quot;</type>
      <name>Min</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>a3de5592dfe10809917ddfdf8f8e1fd14</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;int64&quot;</type>
      <name>Max</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>a1fcaf571b95bfe7ee1faac8b94d015d4</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;bool&quot;</type>
      <name>Contains</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>a9e74bd295d83e2c709022924f5d17f79</anchor>
      <arglist>(self, &apos;int64&apos; value)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::Domain&quot;</type>
      <name>Complement</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>a5b3c423c174eec014ff14c706c35e3dd</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::Domain&quot;</type>
      <name>Negation</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>abb6c82f26dc6e96671a822c47e431d50</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::Domain&quot;</type>
      <name>IntersectionWith</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>a345bd73f7f6a8f79356e7692ceac2023</anchor>
      <arglist>(self, &apos;Domain&apos; domain)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::Domain&quot;</type>
      <name>UnionWith</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>a617f630f4e0e05913b66888540c03485</anchor>
      <arglist>(self, &apos;Domain&apos; domain)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::Domain&quot;</type>
      <name>AdditionWith</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>adab5cd47ddb46cb311ceb9da03d5f179</anchor>
      <arglist>(self, &apos;Domain&apos; domain)</arglist>
    </member>
    <member kind="function">
      <type>&quot;std::string&quot;</type>
      <name>__str__</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>a3a9381d7db20c1c9bc7a2b94bd657b64</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;bool&quot;</type>
      <name>__lt__</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>a395c7c907487a47acde4dd7dac88d005</anchor>
      <arglist>(self, &apos;Domain&apos; other)</arglist>
    </member>
    <member kind="function">
      <type>&quot;bool&quot;</type>
      <name>__eq__</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>a372da5f258631a489ece8e7035ed992f</anchor>
      <arglist>(self, &apos;Domain&apos; other)</arglist>
    </member>
    <member kind="function">
      <type>&quot;bool&quot;</type>
      <name>__ne__</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>a281740a0e29157bb9f4121b895402bdc</anchor>
      <arglist>(self, &apos;Domain&apos; other)</arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>this</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>ad2a304f6c53d2c8c01245737ea889e84</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>AllValues</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>a05474254908e3d2069c65372bbfe28ec</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>FromValues</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>ad66de415ec742ab11c925d39cad31152</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>FromIntervals</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>a3147ad0aa1064b4b2df9d0efe2ca61dd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>FromFlatIntervals</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>a6de9a2e62728ca59e63addaeaf27b80c</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>ortools::sat::python::cp_model::IntervalVar</name>
    <filename>classortools_1_1sat_1_1python_1_1cp__model_1_1IntervalVar.html</filename>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1IntervalVar.html</anchorfile>
      <anchor>a2502bb09efdc10ec3fe6747d53193486</anchor>
      <arglist>(self, model, start_index, size_index, end_index, is_present_index, name)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Index</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1IntervalVar.html</anchorfile>
      <anchor>abb590fc14bae8e8fb5c6155dd4b3a462</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Proto</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1IntervalVar.html</anchorfile>
      <anchor>abb42f0e836de79a7353f81e80a834bfc</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__str__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1IntervalVar.html</anchorfile>
      <anchor>a4a9ad6af57b35f456e3be0016a059584</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__repr__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1IntervalVar.html</anchorfile>
      <anchor>af4a68268016bb869983e0500046a79a4</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Name</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1IntervalVar.html</anchorfile>
      <anchor>a2525dc961733836b138617436213087a</anchor>
      <arglist>(self)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>ortools::sat::python::cp_model::IntVar</name>
    <filename>classortools_1_1sat_1_1python_1_1cp__model_1_1IntVar.html</filename>
    <base>ortools::sat::python::cp_model::LinearExpr</base>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1IntVar.html</anchorfile>
      <anchor>aa6446334169888d93d6783d557e77c60</anchor>
      <arglist>(self, model, domain, name)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Index</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1IntVar.html</anchorfile>
      <anchor>ae9b97488fd520fa654aa56f1fb9a7bbc</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Proto</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1IntVar.html</anchorfile>
      <anchor>aa4e56ee62e7cbbbab035f658a4c87c12</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__str__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1IntVar.html</anchorfile>
      <anchor>acccf215eefcf482bcff7b14828916592</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__repr__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1IntVar.html</anchorfile>
      <anchor>a28b8dc565e2b6614e64c75ec4d42c579</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Name</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1IntVar.html</anchorfile>
      <anchor>afeb5e35d71cc05137d1036fe542dd3b4</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Not</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1IntVar.html</anchorfile>
      <anchor>a8bfdd36e965e92f856c67f32d9e53107</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Sum</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a973beabf6c21a6610b1eec20586f166d</anchor>
      <arglist>(cls, expressions)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>ScalProd</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a3f316b0213f4ec7d685a76b8954cdbb5</anchor>
      <arglist>(cls, expressions, coefficients)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>GetVarValueMap</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a35701f3f9cba6e1c7e8bfc806e3babb0</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__hash__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a62e4c2ea7e3f69b23b3ab3e4ab27ece9</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__abs__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a2cba411c779d8bd31321d7224da8ad4a</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__add__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a5b5fc460604a780d3351ba2c4d6d3189</anchor>
      <arglist>(self, expr)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__radd__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>afc9ccf7e1ddcdd16f0a54431988545f2</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__sub__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>aca05f3cb46757380679eebde01b9ed33</anchor>
      <arglist>(self, expr)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__rsub__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>ab470293c967ced236c2c14efe23e78b9</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__mul__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a21f24356400cc4ab99fa1d3c804983ee</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__rmul__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>aab263dcb9b9c794a489826e3581de945</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__div__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>acf5a2b8a8f9be335c12fdea745508e42</anchor>
      <arglist>(self, _)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__truediv__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a52e3de0e92398c1bf26905819a1e8358</anchor>
      <arglist>(self, _)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__mod__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a2be8fcb5fbe7f6bcda06597b753d0969</anchor>
      <arglist>(self, _)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__neg__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a53a600c215c1911e4c94208537587603</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__eq__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a7f879111ab9c6c3bb657d65e10839cfc</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__ge__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a86b9629ae762267a815652f002287a78</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__le__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a82cb0b1dc448a7a4c2a2d24b8640ecce</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__lt__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a6ab97a5e24d4b1ba769ac1e4caf5d549</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__gt__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a866da212ab25ad65975afd32520fd091</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__ne__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a81f1434405db7bb21d8d5fa12a7e7808</anchor>
      <arglist>(self, arg)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>ortools::sat::python::cp_model::LinearExpr</name>
    <filename>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</filename>
    <member kind="function">
      <type>def</type>
      <name>Sum</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a973beabf6c21a6610b1eec20586f166d</anchor>
      <arglist>(cls, expressions)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>ScalProd</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a3f316b0213f4ec7d685a76b8954cdbb5</anchor>
      <arglist>(cls, expressions, coefficients)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>GetVarValueMap</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a35701f3f9cba6e1c7e8bfc806e3babb0</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__hash__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a62e4c2ea7e3f69b23b3ab3e4ab27ece9</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__abs__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a2cba411c779d8bd31321d7224da8ad4a</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__add__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a5b5fc460604a780d3351ba2c4d6d3189</anchor>
      <arglist>(self, expr)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__radd__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>afc9ccf7e1ddcdd16f0a54431988545f2</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__sub__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>aca05f3cb46757380679eebde01b9ed33</anchor>
      <arglist>(self, expr)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__rsub__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>ab470293c967ced236c2c14efe23e78b9</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__mul__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a21f24356400cc4ab99fa1d3c804983ee</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__rmul__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>aab263dcb9b9c794a489826e3581de945</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__div__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>acf5a2b8a8f9be335c12fdea745508e42</anchor>
      <arglist>(self, _)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__truediv__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a52e3de0e92398c1bf26905819a1e8358</anchor>
      <arglist>(self, _)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__mod__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a2be8fcb5fbe7f6bcda06597b753d0969</anchor>
      <arglist>(self, _)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__neg__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a53a600c215c1911e4c94208537587603</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__eq__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a7f879111ab9c6c3bb657d65e10839cfc</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__ge__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a86b9629ae762267a815652f002287a78</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__le__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a82cb0b1dc448a7a4c2a2d24b8640ecce</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__lt__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a6ab97a5e24d4b1ba769ac1e4caf5d549</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__gt__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a866da212ab25ad65975afd32520fd091</anchor>
      <arglist>(self, arg)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__ne__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1LinearExpr.html</anchorfile>
      <anchor>a81f1434405db7bb21d8d5fa12a7e7808</anchor>
      <arglist>(self, arg)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>ortools::sat::python::cp_model::ObjectiveSolutionPrinter</name>
    <filename>classortools_1_1sat_1_1python_1_1cp__model_1_1ObjectiveSolutionPrinter.html</filename>
    <base>ortools::sat::python::cp_model::CpSolverSolutionCallback</base>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1ObjectiveSolutionPrinter.html</anchorfile>
      <anchor>aa001ca0781a5586af1257ee032f8c31b</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>on_solution_callback</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1ObjectiveSolutionPrinter.html</anchorfile>
      <anchor>a528b7bf077f0ea25bb7e3fdf03500c18</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>solution_count</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1ObjectiveSolutionPrinter.html</anchorfile>
      <anchor>a12f124da1b48341e55950aab50e40914</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>OnSolutionCallback</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolverSolutionCallback.html</anchorfile>
      <anchor>a9e93b1118bcbf01fe482d7cd734308be</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>BooleanValue</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolverSolutionCallback.html</anchorfile>
      <anchor>a037ef23e537c12d86444ac0730ab7304</anchor>
      <arglist>(self, lit)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Value</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolverSolutionCallback.html</anchorfile>
      <anchor>a5fdafe0522600de5aaac297c0ee0ef31</anchor>
      <arglist>(self, expression)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>ortools::sat::python::cp_model::VarArrayAndObjectiveSolutionPrinter</name>
    <filename>classortools_1_1sat_1_1python_1_1cp__model_1_1VarArrayAndObjectiveSolutionPrinter.html</filename>
    <base>ortools::sat::python::cp_model::CpSolverSolutionCallback</base>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1VarArrayAndObjectiveSolutionPrinter.html</anchorfile>
      <anchor>adf8070a07af3cf4919ca2526a686d30b</anchor>
      <arglist>(self, variables)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>on_solution_callback</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1VarArrayAndObjectiveSolutionPrinter.html</anchorfile>
      <anchor>ab47a0eb6e4a9d1f9c0143bbfb00dc3f9</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>solution_count</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1VarArrayAndObjectiveSolutionPrinter.html</anchorfile>
      <anchor>aa2b4ad082aa0cf0f620d5d5ff0f46c14</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>OnSolutionCallback</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolverSolutionCallback.html</anchorfile>
      <anchor>a9e93b1118bcbf01fe482d7cd734308be</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>BooleanValue</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolverSolutionCallback.html</anchorfile>
      <anchor>a037ef23e537c12d86444ac0730ab7304</anchor>
      <arglist>(self, lit)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Value</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolverSolutionCallback.html</anchorfile>
      <anchor>a5fdafe0522600de5aaac297c0ee0ef31</anchor>
      <arglist>(self, expression)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>ortools::sat::python::cp_model::VarArraySolutionPrinter</name>
    <filename>classortools_1_1sat_1_1python_1_1cp__model_1_1VarArraySolutionPrinter.html</filename>
    <base>ortools::sat::python::cp_model::CpSolverSolutionCallback</base>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1VarArraySolutionPrinter.html</anchorfile>
      <anchor>a428b9e068591bf09d253c5589a4d8ce8</anchor>
      <arglist>(self, variables)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>on_solution_callback</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1VarArraySolutionPrinter.html</anchorfile>
      <anchor>a039d7477e5e75b82cdd64301f0fa1794</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>solution_count</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1VarArraySolutionPrinter.html</anchorfile>
      <anchor>a8be3229422be1b3c3125b861fe5a9c0b</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>OnSolutionCallback</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolverSolutionCallback.html</anchorfile>
      <anchor>a9e93b1118bcbf01fe482d7cd734308be</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>BooleanValue</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolverSolutionCallback.html</anchorfile>
      <anchor>a037ef23e537c12d86444ac0730ab7304</anchor>
      <arglist>(self, lit)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Value</name>
      <anchorfile>classortools_1_1sat_1_1python_1_1cp__model_1_1CpSolverSolutionCallback.html</anchorfile>
      <anchor>a5fdafe0522600de5aaac297c0ee0ef31</anchor>
      <arglist>(self, expression)</arglist>
    </member>
  </compound>
  <compound kind="namespace">
    <name>cp_model_pb2</name>
    <filename>namespacecp__model__pb2.html</filename>
    <member kind="variable">
      <type></type>
      <name>DESCRIPTOR</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a3b82e474ed9aff0c7ca224f06943a3ee</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>CpSolverStatus</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a7c8075cf164d908fc3473f92cca8771c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>UNKNOWN</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a85ddc4e22375679709030de35d684a69</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>MODEL_INVALID</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a20042bf14439bd22f972416f33012c01</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>FEASIBLE</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a0d5a946aae8dc83ff15b5f36143888e5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>INFEASIBLE</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>ab4c9c603b8ee974a13ca74ebc56ea6d4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>OPTIMAL</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a8f4487ded736edf3c8f706515ac462b6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>message_type</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>aeb868052e90c8726f3e157c41c7d591b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>containing_oneof</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>add652f8e14fe6018c1db62ef4e37ebb8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>containing_type</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>ab32c0752fd1a423229a92f6f2066820e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>enum_type</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>abaf9cd6046cfc380254b89112ae06a05</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>IntegerVariableProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a99f38a6adf366ac7a21c168586a4c172</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>BoolArgumentProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a729cb7daea0c00d1fe2b5f513e4eacc6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>IntegerArgumentProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a5f05e1338d5485af9ac091e86f2d52b2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>AllDifferentConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>af4162d2efebe4c63f3c2372767a7a363</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>LinearConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a555f951bcafcd6f9697566024666eca9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>ElementConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>ab997ab84dfa686a4781c84119a37f4df</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>IntervalConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a95240b4cadf3b6ebc2403399140cfb62</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>NoOverlapConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a547f8da0d83229e28751b1b96b55cfa2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>NoOverlap2DConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a8e96711376196bd71e128fe368b1c19d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>CumulativeConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a9d296b05819f10f319c055746fb24995</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>ReservoirConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>acf9c2c83a4a8d74a66ec5b74b9fe31db</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>CircuitConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a58a76bc08d1f160946c07f8ca7963072</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>RoutesConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a40aa00267a4546d54cb9a740bbf15bde</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>CircuitCoveringConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>ad9990659ab4af7fd8f024cac4dce4c7d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>TableConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>acb46d2c46bf65f4c4afe9baaf2e3d713</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>InverseConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a151eed6d8a0ab2af30eb191ce1e66303</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>AutomatonConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a3e8db83ee219e253e1122be6e7f6a13a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>ConstraintProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>af4945cbe4db803e86d070edc6f05aa70</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>CpObjectiveProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a624e2be969bacd96ec4b128b01e1dd86</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>DecisionStrategyProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>ae00c8bfbf29aa5b2ecdb31ecf273817b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>PartialVariableAssignment</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a6e5000310f70e903e62de157274e35cf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>CpModelProto</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a9b45f2c6923fd752e13791cd0c5162b2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>CpSolverResponse</name>
      <anchorfile>namespacecp__model__pb2.html</anchorfile>
      <anchor>a88e7fc6178d7b0154d6e1c1b41ec3ff4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="namespace">
    <name>optional_boolean_pb2</name>
    <filename>namespaceoptional__boolean__pb2.html</filename>
    <member kind="variable">
      <type></type>
      <name>DESCRIPTOR</name>
      <anchorfile>namespaceoptional__boolean__pb2.html</anchorfile>
      <anchor>a752f4bab1b329021567697f4a2f8b183</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>OptionalBoolean</name>
      <anchorfile>namespaceoptional__boolean__pb2.html</anchorfile>
      <anchor>a352156d3d2612850a8ad910de0b844e8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>BOOL_UNSPECIFIED</name>
      <anchorfile>namespaceoptional__boolean__pb2.html</anchorfile>
      <anchor>a5b204b6c1f39553058619ba78057640e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>BOOL_FALSE</name>
      <anchorfile>namespaceoptional__boolean__pb2.html</anchorfile>
      <anchor>a9ec5d7cd566cf9a9e5414e0c5da45a6c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>BOOL_TRUE</name>
      <anchorfile>namespaceoptional__boolean__pb2.html</anchorfile>
      <anchor>a9df97a7ea75c8da10089be0ffd7666e4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="namespace">
    <name>ortools</name>
    <filename>namespaceortools.html</filename>
    <namespace>ortools::sat</namespace>
  </compound>
  <compound kind="namespace">
    <name>ortools::sat</name>
    <filename>namespaceortools_1_1sat.html</filename>
    <namespace>ortools::sat::python</namespace>
  </compound>
  <compound kind="namespace">
    <name>ortools::sat::python</name>
    <filename>namespaceortools_1_1sat_1_1python.html</filename>
    <namespace>ortools::sat::python::cp_model</namespace>
    <namespace>ortools::sat::python::cp_model_helper</namespace>
  </compound>
  <compound kind="namespace">
    <name>ortools::sat::python::cp_model</name>
    <filename>namespaceortools_1_1sat_1_1python_1_1cp__model.html</filename>
    <class kind="class">ortools::sat::python::cp_model::_NotBooleanVariable</class>
    <class kind="class">ortools::sat::python::cp_model::_ProductCst</class>
    <class kind="class">ortools::sat::python::cp_model::_ScalProd</class>
    <class kind="class">ortools::sat::python::cp_model::_SumArray</class>
    <class kind="class">ortools::sat::python::cp_model::BoundedLinearExpression</class>
    <class kind="class">ortools::sat::python::cp_model::Constraint</class>
    <class kind="class">ortools::sat::python::cp_model::CpModel</class>
    <class kind="class">ortools::sat::python::cp_model::CpSolver</class>
    <class kind="class">ortools::sat::python::cp_model::CpSolverSolutionCallback</class>
    <class kind="class">ortools::sat::python::cp_model::IntervalVar</class>
    <class kind="class">ortools::sat::python::cp_model::IntVar</class>
    <class kind="class">ortools::sat::python::cp_model::LinearExpr</class>
    <class kind="class">ortools::sat::python::cp_model::ObjectiveSolutionPrinter</class>
    <class kind="class">ortools::sat::python::cp_model::VarArrayAndObjectiveSolutionPrinter</class>
    <class kind="class">ortools::sat::python::cp_model::VarArraySolutionPrinter</class>
    <member kind="function">
      <type>def</type>
      <name>DisplayBounds</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>af51ab275fe6ec1a6a1ecca81a3756f23</anchor>
      <arglist>(bounds)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>ShortName</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>acba81538416dea40488e0bc47d7f1bce</anchor>
      <arglist>(model, i)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>EvaluateLinearExpr</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>adc24b18bcf23f3d0ee27de4fda0d4bbc</anchor>
      <arglist>(expression, solution)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>EvaluateBooleanExpression</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a4f4a79fda7a70ae5d20f444b49fa9235</anchor>
      <arglist>(literal, solution)</arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>Domain</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>aa561f079171b584b0e30a5dcc8355407</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>INT_MIN</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a6ee78cf2c5f4f2ac2a6178692065026d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>INT_MAX</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a300a0abf6c30e041dc44e318806a1d92</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>INT32_MAX</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a6eb5ab64ecf3d84de634c4bcffa6ae6f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>INT32_MIN</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a16a17e52284a461ae9674180f5fec3f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>UNKNOWN</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a0d7ec85f0667ae3498caf24d368c447f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>MODEL_INVALID</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>afdbcf57d5bceaaf00142399327dd83ab</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>FEASIBLE</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a85b2db34962f377ba16ec6f64194a668</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>INFEASIBLE</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>ae2ffaad1f9fcede1a25729e7459f36cf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>OPTIMAL</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>ac939f5fe74d8b27529295ca315bdfa60</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>CHOOSE_FIRST</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>adccb01dd707f7de3f6df3ef4f3aff8f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>CHOOSE_LOWEST_MIN</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a9243469e415b1a356b7d3c2c3d0971d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>CHOOSE_HIGHEST_MAX</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a2bc9ec509993f37f9e8dd6ed2a4ec421</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>tuple</type>
      <name>CHOOSE_MIN_DOMAIN_SIZE</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>ab6c1acb8f211484600d5e6f8c88873ae</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>tuple</type>
      <name>CHOOSE_MAX_DOMAIN_SIZE</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a1091e441a9c06c1d9f15f23449a23c70</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>SELECT_MIN_VALUE</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a05250be64e0373e7908a73256d1f5156</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>SELECT_MAX_VALUE</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>aca5d87b3995a56ee30af4332bbd9e4ab</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>SELECT_LOWER_HALF</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a1769365136c1eeb7e93023e6597d370e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>SELECT_UPPER_HALF</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a101c70c1f9391364f16565daa611d55d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>AUTOMATIC_SEARCH</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>ab0ff22e75f159c943eec0259cc0a94fa</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>FIXED_SEARCH</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>ad97296bce4450fe6edcc6c49b5568c17</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>PORTFOLIO_SEARCH</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>afe97a78be6c27ea1887bba2bc7171cdf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>LP_SEARCH</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model.html</anchorfile>
      <anchor>a981bb9249d0bf4cdc4e82206671373a4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="namespace">
    <name>ortools::sat::python::cp_model_helper</name>
    <filename>namespaceortools_1_1sat_1_1python_1_1cp__model__helper.html</filename>
    <member kind="function">
      <type>def</type>
      <name>AssertIsInt64</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model__helper.html</anchorfile>
      <anchor>ae1bc8b7f58746e02d00fceda6fc9cb7a</anchor>
      <arglist>(x)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AssertIsInt32</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model__helper.html</anchorfile>
      <anchor>a14c7d4ab8503eeecd159cf99293569e0</anchor>
      <arglist>(x)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>AssertIsBoolean</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model__helper.html</anchorfile>
      <anchor>a3a1864a790a33cda426707d3e008bade</anchor>
      <arglist>(x)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>CapInt64</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model__helper.html</anchorfile>
      <anchor>adeae447da96724fc05787a75e3ea6e52</anchor>
      <arglist>(v)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>CapSub</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model__helper.html</anchorfile>
      <anchor>ac1032dad2dd9fd20d07d07d4d507f966</anchor>
      <arglist>(x, y)</arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>INT_MIN</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model__helper.html</anchorfile>
      <anchor>a8eb5041ef6578acb939595611f02ca9d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>INT_MAX</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model__helper.html</anchorfile>
      <anchor>a54e68cc6699eb765ab10722022e7b6ee</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>INT32_MIN</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model__helper.html</anchorfile>
      <anchor>a3178ea13a815ea65cfc5fa4a98ba354d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>INT32_MAX</name>
      <anchorfile>namespaceortools_1_1sat_1_1python_1_1cp__model__helper.html</anchorfile>
      <anchor>aa2cf011afadfc21b61dedc04947d3a1a</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="namespace">
    <name>sat_parameters_pb2</name>
    <filename>namespacesat__parameters__pb2.html</filename>
    <member kind="variable">
      <type></type>
      <name>DESCRIPTOR</name>
      <anchorfile>namespacesat__parameters__pb2.html</anchorfile>
      <anchor>ac468144cf2c5f114b2677d02856ba4ca</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>enum_type</name>
      <anchorfile>namespacesat__parameters__pb2.html</anchorfile>
      <anchor>a150c7a2dfac007f9f47e37c2f21cc5c2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>containing_type</name>
      <anchorfile>namespacesat__parameters__pb2.html</anchorfile>
      <anchor>a80f883776f9055456687e6fb1171d642</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>SatParameters</name>
      <anchorfile>namespacesat__parameters__pb2.html</anchorfile>
      <anchor>a5a73e7bf8272a7b5b374bca54beb3b6e</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="namespace">
    <name>sorted_interval_list</name>
    <filename>namespacesorted__interval__list.html</filename>
    <class kind="class">sorted_interval_list::_object</class>
    <class kind="class">sorted_interval_list::Domain</class>
    <member kind="function">
      <type>def</type>
      <name>swig_import_helper</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>a14bb19c00b1f20e0d7bed17a28471bd4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::Domain&quot;</type>
      <name>Domain_AllValues</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>aba1c402d7d63b1fcce95292aa3cfe836</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::Domain&quot;</type>
      <name>Domain_FromValues</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>aebfb4145f9aa735e2803aca9c489a4d3</anchor>
      <arglist>(&apos;std::vector&lt; int64 &gt;&apos; values)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::Domain&quot;</type>
      <name>Domain_FromIntervals</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>a2a36487130d7d8470d89d4f9da21e66f</anchor>
      <arglist>(&apos;std::vector&lt; std::vector&lt; int64 &gt; &gt; const &amp;&apos; intervals)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::Domain&quot;</type>
      <name>Domain_FromFlatIntervals</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>ad7632d9ec0e7f377b107093e62c3c9db</anchor>
      <arglist>(&apos;std::vector&lt; int64 &gt; const &amp;&apos; flat_intervals)</arglist>
    </member>
    <member kind="function">
      <type>&quot;std::ostream &amp;&quot;</type>
      <name>__lshift__</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>a07208216d9c3caa39dd3c82856fa868a</anchor>
      <arglist>(*args)</arglist>
    </member>
    <member kind="variable">
      <type>def</type>
      <name>Domain_swigregister</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>aa378907dc2f41a2a20756019c22ccf80</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>def</type>
      <name>Domain_AllValues</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>a56ebc1695d4c71b4c47ffcbcfce9445e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>def</type>
      <name>Domain_FromValues</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>a1494ecbeeed013d511256f3547c45021</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>def</type>
      <name>Domain_FromIntervals</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>aff0c690beb785512fa3e56435646680d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>def</type>
      <name>Domain_FromFlatIntervals</name>
      <anchorfile>namespacesorted__interval__list.html</anchorfile>
      <anchor>ad3d6a94115982d3f035243d679195860</anchor>
      <arglist></arglist>
    </member>
  </compound>
</tagfile>
