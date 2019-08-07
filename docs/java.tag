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
    <name>pywraplp.py</name>
    <path>/Users/lperron/Work/or-tools/ortools/gen/ortools/linear_solver/</path>
    <filename>pywraplp_8py</filename>
    <class kind="class">pywraplp::_SwigNonDynamicMeta</class>
    <class kind="class">pywraplp::Solver</class>
    <class kind="class">pywraplp::Objective</class>
    <class kind="class">pywraplp::Variable</class>
    <class kind="class">pywraplp::Constraint</class>
    <class kind="class">pywraplp::MPSolverParameters</class>
    <class kind="class">pywraplp::ModelExportOptions</class>
    <namespace>pywraplp</namespace>
    <member kind="function">
      <type>&quot;bool&quot;</type>
      <name>Solver_SupportsProblemType</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>af9e5bb41885004372af323dd0af22b55</anchor>
      <arglist>(&apos;operations_research::MPSolver::OptimizationProblemType&apos; problem_type)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::MPSolutionResponse *&quot;</type>
      <name>Solver_SolveWithProto</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>a35f9e3adb5239837b87ca4c3b35047da</anchor>
      <arglist>(&apos;operations_research::MPModelRequest const &amp;&apos; model_request, &apos;operations_research::MPSolutionResponse *&apos; response)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>Solver_infinity</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>a4bf2d52f8542d3a15e817f4644f8e0fe</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>Solver_Infinity</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>abbf76685e3b21effa780d2874fb81f77</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>&quot;std::ostream &amp;&quot;</type>
      <name>__lshift__</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>ad77a2f8f2ae677134f6bb1760f8267a4</anchor>
      <arglist>(*args)</arglist>
    </member>
    <member kind="function">
      <type>&quot;std::string&quot;</type>
      <name>ExportModelAsLpFormat</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>a74f7ca30e20d70e59e22f45d45ba4b56</anchor>
      <arglist>(*args)</arglist>
    </member>
    <member kind="function">
      <type>&quot;std::string&quot;</type>
      <name>ExportModelAsMpsFormat</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>ac080b9f23d010cedbcc309121a7dda04</anchor>
      <arglist>(*args)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>setup_variable_operator</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>a372dfebc0eccbd07f50df10d83428885</anchor>
      <arglist>(opname)</arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>cvar</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>af54064148573c9d853a2680ba79498e9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>kDefaultRelativeMipGap</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>af5b8cb726edf723ee2dc3d33430eabfb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>kDefaultPrimalTolerance</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>a2f355e1fde4adc54c26c2319cd8df440</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>kDefaultDualTolerance</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>a672614bff11eaee732752229c4c233d0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>kDefaultPresolve</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>a7e83b4751d474f2982d2508906481a38</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>kDefaultIncrementality</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>a7c28fd9f4e44d2b1c3d78d43c42526f2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>sorted_interval_list.py</name>
    <path>/Users/lperron/Work/or-tools/ortools/gen/ortools/util/</path>
    <filename>sorted__interval__list_8py</filename>
    <class kind="class">sorted_interval_list::_SwigNonDynamicMeta</class>
    <class kind="class">sorted_interval_list::Domain</class>
    <namespace>sorted_interval_list</namespace>
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
    <name>pywraplp::_SwigNonDynamicMeta</name>
    <filename>classpywraplp_1_1__SwigNonDynamicMeta.html</filename>
  </compound>
  <compound kind="class">
    <name>sorted_interval_list::_SwigNonDynamicMeta</name>
    <filename>classsorted__interval__list_1_1__SwigNonDynamicMeta.html</filename>
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
    <name>pywraplp::Constraint</name>
    <filename>classpywraplp_1_1Constraint.html</filename>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classpywraplp_1_1Constraint.html</anchorfile>
      <anchor>ad105b186cae725c615b79a07d526efbf</anchor>
      <arglist>(self, *args, **kwargs)</arglist>
    </member>
    <member kind="function">
      <type>&quot;std::string const &amp;&quot;</type>
      <name>name</name>
      <anchorfile>classpywraplp_1_1Constraint.html</anchorfile>
      <anchor>a3953e0ced774981099c28965e2b02607</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>SetCoefficient</name>
      <anchorfile>classpywraplp_1_1Constraint.html</anchorfile>
      <anchor>adb3fe6debbb2dd634be14a86684493b8</anchor>
      <arglist>(self, &apos;Variable&apos; var, &apos;double&apos; coeff)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>GetCoefficient</name>
      <anchorfile>classpywraplp_1_1Constraint.html</anchorfile>
      <anchor>a50de9b0c422b5e670dd4ed03370fbc15</anchor>
      <arglist>(self, &apos;Variable&apos; var)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>lb</name>
      <anchorfile>classpywraplp_1_1Constraint.html</anchorfile>
      <anchor>afc1fbdd11b0092944240cd1f9b99334b</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>ub</name>
      <anchorfile>classpywraplp_1_1Constraint.html</anchorfile>
      <anchor>a3797d7f1e20530b3de47dc948e7ce02f</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>SetBounds</name>
      <anchorfile>classpywraplp_1_1Constraint.html</anchorfile>
      <anchor>a61c2bbaa83c081156f8bce437929bc40</anchor>
      <arglist>(self, &apos;double&apos; lb, &apos;double&apos; ub)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>set_is_lazy</name>
      <anchorfile>classpywraplp_1_1Constraint.html</anchorfile>
      <anchor>a6fc68bc7ca81022c9350e1afccc6e51c</anchor>
      <arglist>(self, &apos;bool&apos; laziness)</arglist>
    </member>
    <member kind="function">
      <type>&quot;int&quot;</type>
      <name>index</name>
      <anchorfile>classpywraplp_1_1Constraint.html</anchorfile>
      <anchor>a60eb76b005b08773f3ef766c46074175</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>dual_value</name>
      <anchorfile>classpywraplp_1_1Constraint.html</anchorfile>
      <anchor>a726f164301ab11c08614245c4e15649c</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::MPSolver::BasisStatus&quot;</type>
      <name>basis_status</name>
      <anchorfile>classpywraplp_1_1Constraint.html</anchorfile>
      <anchor>a5fa06d923ca061c0d7012ba75d03da8e</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>Lb</name>
      <anchorfile>classpywraplp_1_1Constraint.html</anchorfile>
      <anchor>a97af6c7661ffbb0bc95b47074d5eff8c</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>Ub</name>
      <anchorfile>classpywraplp_1_1Constraint.html</anchorfile>
      <anchor>a264d705f2983c600653916ed7abe9a9d</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>SetLb</name>
      <anchorfile>classpywraplp_1_1Constraint.html</anchorfile>
      <anchor>aa334f054bbb45b2b93c68fab66297902</anchor>
      <arglist>(self, &apos;double&apos; x)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>SetUb</name>
      <anchorfile>classpywraplp_1_1Constraint.html</anchorfile>
      <anchor>a5191dd719c395713c75717d3d2c631e0</anchor>
      <arglist>(self, &apos;double&apos; x)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>DualValue</name>
      <anchorfile>classpywraplp_1_1Constraint.html</anchorfile>
      <anchor>aeb4506a52d358722d1eb9d1162d0a687</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="property" static="yes">
      <type></type>
      <name>thisown</name>
      <anchorfile>classpywraplp_1_1Constraint.html</anchorfile>
      <anchor>a8fdfa6c285c5d0e13b3b1b0ec14745de</anchor>
      <arglist></arglist>
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
    <member kind="function" static="yes">
      <type>&quot;operations_research::Domain&quot;</type>
      <name>AllValues</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>a284378ca72905c11f861de33f8f83a1a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>&quot;operations_research::Domain&quot;</type>
      <name>FromValues</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>af0535f291aede466397fdc6742f8390a</anchor>
      <arglist>(&apos;std::vector&lt; int64 &gt;&apos; values)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>&quot;operations_research::Domain&quot;</type>
      <name>FromIntervals</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>adb908ef1b10b67e0a26cdebbc82400aa</anchor>
      <arglist>(&apos;std::vector&lt; std::vector&lt; int64 &gt; &gt; const &amp;&apos; intervals)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>&quot;operations_research::Domain&quot;</type>
      <name>FromFlatIntervals</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>a116d2c4397691467749d64d90105943f</anchor>
      <arglist>(&apos;std::vector&lt; int64 &gt; const &amp;&apos; flat_intervals)</arglist>
    </member>
    <member kind="property" static="yes">
      <type></type>
      <name>thisown</name>
      <anchorfile>classsorted__interval__list_1_1Domain.html</anchorfile>
      <anchor>a025aa5a6dce278dd9e4d9f98ece15bbb</anchor>
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
    <name>pywraplp::ModelExportOptions</name>
    <filename>classpywraplp_1_1ModelExportOptions.html</filename>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classpywraplp_1_1ModelExportOptions.html</anchorfile>
      <anchor>ac7040831ecaab994b4b6ae3f17cc4497</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="property" static="yes">
      <type></type>
      <name>thisown</name>
      <anchorfile>classpywraplp_1_1ModelExportOptions.html</anchorfile>
      <anchor>a696030ee3310b0234ea5f0443d6b5818</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>pywraplp::MPSolverParameters</name>
    <filename>classpywraplp_1_1MPSolverParameters.html</filename>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>ae1beb0812a4853af52beee72573df4f5</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>SetDoubleParam</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>aff94bf4d832b47877223594628f686aa</anchor>
      <arglist>(self, &apos;operations_research::MPSolverParameters::DoubleParam&apos; param, &apos;double&apos; value)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>SetIntegerParam</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>a05f8fff84cf35aa4a4d773bb4529d06b</anchor>
      <arglist>(self, &apos;operations_research::MPSolverParameters::IntegerParam&apos; param, &apos;int&apos; value)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>GetDoubleParam</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>a7bdfdafccf15cd82f9b51852d9dd2ac0</anchor>
      <arglist>(self, &apos;operations_research::MPSolverParameters::DoubleParam&apos; param)</arglist>
    </member>
    <member kind="function">
      <type>&quot;int&quot;</type>
      <name>GetIntegerParam</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>a625bfdeeb3b65a0c2d6de85b0690815f</anchor>
      <arglist>(self, &apos;operations_research::MPSolverParameters::IntegerParam&apos; param)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>RELATIVE_MIP_GAP</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>ad0e6d04da2d401b1894232408bf02023</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>PRIMAL_TOLERANCE</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>ab97254757a5165a688c2251296fed880</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>DUAL_TOLERANCE</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>ac29759bf5959597922ce0beefeaee50d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>PRESOLVE</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>ab8a5fdb52ec5379f4e15a0d10f5d5a4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>LP_ALGORITHM</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>a37aba14c711031dbdd5e180a40ac9c76</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>INCREMENTALITY</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>af39166cdc1b4c9c46e0ea8a4ace09144</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>SCALING</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>ac0c5f795c7874beb74b5810a1e34d428</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>PRESOLVE_OFF</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>a2dd94b3805240216ce9bfeb0ce7c0dc6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>PRESOLVE_ON</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>a5408abcd860e4e374bc4478abc4c9848</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>DUAL</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>a09624e82395b780c563148e53c9439e5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>PRIMAL</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>aad6e9988b4517029d249c54164452a1f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>BARRIER</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>a8d12b9a614e5b997e82269dc8f0df102</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>INCREMENTALITY_OFF</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>ad52ca9ef5464bc6666a31f9ddebc6cf2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>INCREMENTALITY_ON</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>aca3e87245236d497084bc3c4821d8f8e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>SCALING_OFF</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>a9a91d8a1413e0ac7c3c7f597337f16f4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>SCALING_ON</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>a7bed45d919f6b46930d44f03c7fe7ebf</anchor>
      <arglist></arglist>
    </member>
    <member kind="property" static="yes">
      <type></type>
      <name>thisown</name>
      <anchorfile>classpywraplp_1_1MPSolverParameters.html</anchorfile>
      <anchor>aceddc38b9a2a2673a53139d235203276</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>pywraplp::Objective</name>
    <filename>classpywraplp_1_1Objective.html</filename>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classpywraplp_1_1Objective.html</anchorfile>
      <anchor>a267162f5d174ddefb71abc7bc31edbb6</anchor>
      <arglist>(self, *args, **kwargs)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>Clear</name>
      <anchorfile>classpywraplp_1_1Objective.html</anchorfile>
      <anchor>a6a32a441b545601722ad0d0f2b430719</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>SetCoefficient</name>
      <anchorfile>classpywraplp_1_1Objective.html</anchorfile>
      <anchor>a6e791d6a75415a4f9b123d3ac5529505</anchor>
      <arglist>(self, &apos;Variable&apos; var, &apos;double&apos; coeff)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>GetCoefficient</name>
      <anchorfile>classpywraplp_1_1Objective.html</anchorfile>
      <anchor>a7dfacd43ba3bbc6acd6be9d2adf1384e</anchor>
      <arglist>(self, &apos;Variable&apos; var)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>SetOffset</name>
      <anchorfile>classpywraplp_1_1Objective.html</anchorfile>
      <anchor>afaf6cbccc6cfba2a90329acfb483fd56</anchor>
      <arglist>(self, &apos;double&apos; value)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>offset</name>
      <anchorfile>classpywraplp_1_1Objective.html</anchorfile>
      <anchor>a94b22b02e07e8114d6e7d7ca71e64c2f</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>SetOptimizationDirection</name>
      <anchorfile>classpywraplp_1_1Objective.html</anchorfile>
      <anchor>acfacb0412ff11bb91e6e837b35fc87ce</anchor>
      <arglist>(self, &apos;bool&apos; maximize)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>SetMinimization</name>
      <anchorfile>classpywraplp_1_1Objective.html</anchorfile>
      <anchor>a8c65661348888cce05fe39f06152e555</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>SetMaximization</name>
      <anchorfile>classpywraplp_1_1Objective.html</anchorfile>
      <anchor>a71a6eeb28f67580ea9bd7fb405185644</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;bool&quot;</type>
      <name>maximization</name>
      <anchorfile>classpywraplp_1_1Objective.html</anchorfile>
      <anchor>acfd5352767a411c229a6ad7f0dbcfa34</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;bool&quot;</type>
      <name>minimization</name>
      <anchorfile>classpywraplp_1_1Objective.html</anchorfile>
      <anchor>a96429cfd60d45e032b0bbce2aa33df45</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>Value</name>
      <anchorfile>classpywraplp_1_1Objective.html</anchorfile>
      <anchor>a9016bdbdedd82fe657f02a82392fac41</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>BestBound</name>
      <anchorfile>classpywraplp_1_1Objective.html</anchorfile>
      <anchor>a249904cd8a11263074b573400851b834</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>Offset</name>
      <anchorfile>classpywraplp_1_1Objective.html</anchorfile>
      <anchor>aae78fcdbb458cb0ff69fabd370973045</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="property" static="yes">
      <type></type>
      <name>thisown</name>
      <anchorfile>classpywraplp_1_1Objective.html</anchorfile>
      <anchor>a8159181629cb8eb2432d0f194296a56e</anchor>
      <arglist></arglist>
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
    <name>pywraplp::Solver</name>
    <filename>classpywraplp_1_1Solver.html</filename>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>ac8f211e4c75fdb94c45d15b1521b180e</anchor>
      <arglist>(self, &apos;std::string const &amp;&apos; name, &apos;operations_research::MPSolver::OptimizationProblemType&apos; problem_type)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>Clear</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a8cafa83cc2dae45f50187a300efd3b2f</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;int&quot;</type>
      <name>NumVariables</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>afdbeb5ef8439a7c65d780126e14a4533</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;std::vector&lt; operations_research::MPVariable * &gt; const &amp;&quot;</type>
      <name>variables</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a6854829d873ee06dea116702ad2a57fc</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::MPVariable *&quot;</type>
      <name>LookupVariable</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a2283f6fe770c5f2a24c5877450ecf398</anchor>
      <arglist>(self, &apos;std::string const &amp;&apos; var_name)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::MPVariable *&quot;</type>
      <name>Var</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a36f61e8a59c2da9abe237f295bf89c20</anchor>
      <arglist>(self, &apos;double&apos; lb, &apos;double&apos; ub, &apos;bool&apos; integer, &apos;std::string const &amp;&apos; name)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::MPVariable *&quot;</type>
      <name>NumVar</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>abfa35161dcf0681d47ae4e825d4865e2</anchor>
      <arglist>(self, &apos;double&apos; lb, &apos;double&apos; ub, &apos;std::string const &amp;&apos; name)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::MPVariable *&quot;</type>
      <name>IntVar</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>abdc0404d38033b1c11e856791c99cf21</anchor>
      <arglist>(self, &apos;double&apos; lb, &apos;double&apos; ub, &apos;std::string const &amp;&apos; name)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::MPVariable *&quot;</type>
      <name>BoolVar</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a60f3d500cd48283b6ed0b197d317dcc1</anchor>
      <arglist>(self, &apos;std::string const &amp;&apos; name)</arglist>
    </member>
    <member kind="function">
      <type>&quot;int&quot;</type>
      <name>NumConstraints</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a97ef44c6c95be5b3e167e7623b40ede8</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;std::vector&lt; operations_research::MPConstraint * &gt; const &amp;&quot;</type>
      <name>constraints</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>afd711295ad39b3ab61033aff7b3a98c2</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::MPConstraint *&quot;</type>
      <name>LookupConstraint</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>af4dfa19324fcb7f8b171b52412a9d57b</anchor>
      <arglist>(self, &apos;std::string const &amp;&apos; constraint_name)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::MPConstraint *&quot;</type>
      <name>Constraint</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a65478b4a3cac7fff0fb1def0bbaef3e0</anchor>
      <arglist>(self, *args)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::MPObjective *&quot;</type>
      <name>Objective</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a3ba4869e007b65c2a090ad1b412507a0</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::MPSolver::ResultStatus&quot;</type>
      <name>Solve</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>ab267365fb7c4695422a5f6a532d08d51</anchor>
      <arglist>(self, *args)</arglist>
    </member>
    <member kind="function">
      <type>&quot;std::vector&lt; double &gt;&quot;</type>
      <name>ComputeConstraintActivities</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>afd9726a546c4a345df83d984e31325fa</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;bool&quot;</type>
      <name>VerifySolution</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a90596476c3d6bfd55278338ce16908c4</anchor>
      <arglist>(self, &apos;double&apos; tolerance, &apos;bool&apos; log_errors)</arglist>
    </member>
    <member kind="function">
      <type>&quot;bool&quot;</type>
      <name>InterruptSolve</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>ac2312b6875367b345fe85170d6676741</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>FillSolutionResponseProto</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a8df7387708af495997d77f42d2020f99</anchor>
      <arglist>(self, &apos;operations_research::MPSolutionResponse *&apos; response)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>ExportModelToProto</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a1acb6d8657453b7995e1ec82b9e954a8</anchor>
      <arglist>(self, &apos;operations_research::MPModelProto *&apos; output_model)</arglist>
    </member>
    <member kind="function">
      <type>&quot;util::Status&quot;</type>
      <name>LoadSolutionFromProto</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a2643c875f9cda201b9df1b3f7dd248b2</anchor>
      <arglist>(self, *args)</arglist>
    </member>
    <member kind="function">
      <type>&quot;bool&quot;</type>
      <name>SetSolverSpecificParametersAsString</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a350f1ad44e1be972217f7ed41092f816</anchor>
      <arglist>(self, &apos;std::string const &amp;&apos; parameters)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>EnableOutput</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>ad823b1e6c077adbe58fbcb0203e205e9</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>SuppressOutput</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a32c373cdf87c84d6ab82bdbc6a4acd37</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;int64&quot;</type>
      <name>iterations</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>aa77717c1e844e75407136d094382587f</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;int64&quot;</type>
      <name>nodes</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a2578ec176d3d51332a4e9970cc5e18ef</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>ComputeExactConditionNumber</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>abc2f62f516fc80443448757c820233cf</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;bool&quot;</type>
      <name>NextSolution</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>afc1e0835c106e4821ec6f51ab7798a5e</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>set_time_limit</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a75b2bf6d9d638b9213ed975cfbc7cccb</anchor>
      <arglist>(self, &apos;int64&apos; time_limit_milliseconds)</arglist>
    </member>
    <member kind="function">
      <type>&quot;int64&quot;</type>
      <name>wall_time</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a5e8128e5aef60047afb983261511970b</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;std::string&quot;</type>
      <name>LoadModelFromProto</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>ae2eed37be08ae2aeebf5a1029e8e6b1e</anchor>
      <arglist>(self, &apos;operations_research::MPModelProto const &amp;&apos; input_model)</arglist>
    </member>
    <member kind="function">
      <type>&quot;std::string&quot;</type>
      <name>ExportModelAsLpFormat</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>aa6e502978869abe8487c532833d3f0ae</anchor>
      <arglist>(self, *args)</arglist>
    </member>
    <member kind="function">
      <type>&quot;std::string&quot;</type>
      <name>ExportModelAsMpsFormat</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>aff92d8a18267056faa2d71fbd7990572</anchor>
      <arglist>(self, *args)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>SetHint</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a8f201470f98e02fef5f2264dbc80ead3</anchor>
      <arglist>(self, &apos;std::vector&lt; operations_research::MPVariable * &gt; const &amp;&apos; variables, &apos;std::vector&lt; double &gt; const &amp;&apos; values)</arglist>
    </member>
    <member kind="function">
      <type>&quot;bool&quot;</type>
      <name>SetNumThreads</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a2969b7587a9384a89fda5c569f7b3abe</anchor>
      <arglist>(self, &apos;int&apos; num_theads)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Add</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>ae2a11cbdc522ad0ed79f15c6d46c97ea</anchor>
      <arglist>(self, constraint, name=&apos;&apos;)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Sum</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a1b31cba15f5f577fac2de07e7177a77d</anchor>
      <arglist>(self, expr_array)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>RowConstraint</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a0e129fb7f3226e76798aab0bcf95c14a</anchor>
      <arglist>(self, *args)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Minimize</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a83c671670713ba56970e454383e0b6ca</anchor>
      <arglist>(self, expr)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>Maximize</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a1ad9e13dcea8c7a9f3cb0e50e0d22ef7</anchor>
      <arglist>(self, expr)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>SetTimeLimit</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a6e1c577e800ba125ccf8c426fe44bda4</anchor>
      <arglist>(self, &apos;int64&apos; x)</arglist>
    </member>
    <member kind="function">
      <type>&quot;int64&quot;</type>
      <name>WallTime</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a4d0ec3a502d3a2ac563c25a80020bdd9</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;int64&quot;</type>
      <name>Iterations</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a039945b6b886f1465f095c9412e27a72</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>&quot;bool&quot;</type>
      <name>SupportsProblemType</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a90ad0f27cb171699f477662602479f49</anchor>
      <arglist>(&apos;operations_research::MPSolver::OptimizationProblemType&apos; problem_type)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>&quot;operations_research::MPSolutionResponse *&quot;</type>
      <name>SolveWithProto</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a7ed791de3ab2cdee5558d020a63b5381</anchor>
      <arglist>(&apos;operations_research::MPModelRequest const &amp;&apos; model_request, &apos;operations_research::MPSolutionResponse *&apos; response)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>&quot;double&quot;</type>
      <name>infinity</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a329fd44368b26b4285b2437d51e5b271</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>&quot;double&quot;</type>
      <name>Infinity</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a7b765fa760ccfc7a0e7b0b38bc379650</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>CLP_LINEAR_PROGRAMMING</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>aa6f6ef2276abad8d26de7da3c37f99f6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>GLOP_LINEAR_PROGRAMMING</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>ad9d502a514399c2c6995b06f86d777ef</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>SCIP_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a10fcb5589bdb25d59dc9a88fe55f04f5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>CBC_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a26651d6f12ac786329703f6534c08c62</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>BOP_INTEGER_PROGRAMMING</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a931a8a29d3a920ca41478c811ecc1a5c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>OPTIMAL</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>ac97baf198929c74f976cdd831e9b7d46</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>FEASIBLE</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>ae8dc06755bdfaa1774c96ce0e516a339</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>INFEASIBLE</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>acfe665306653c5fed4052e40fd492c1a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>UNBOUNDED</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a6c7acf5db49501f7e0abf884fdcca897</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>ABNORMAL</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>abdfadbed4f5b2491ec33fddace401bac</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>NOT_SOLVED</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>aa511f19b3511f297b46ca6084c60f3cc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>FREE</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>aaf77b9092459c62a3d6e1f65edb907b6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>AT_LOWER_BOUND</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>ad6584d621c844c10784095685f315be7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>AT_UPPER_BOUND</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a88c5ea7d7ec7c26a9b02c35b5d5b5844</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>FIXED_VALUE</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a58e86c15a0aea97480ffe6846955c973</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type></type>
      <name>BASIC</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a624cd977ff30fa0d4680f5ad7fe20d08</anchor>
      <arglist></arglist>
    </member>
    <member kind="property" static="yes">
      <type></type>
      <name>thisown</name>
      <anchorfile>classpywraplp_1_1Solver.html</anchorfile>
      <anchor>a11bf36f1583be6fd414a5b47a6b55ee4</anchor>
      <arglist></arglist>
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
  <compound kind="class">
    <name>pywraplp::Variable</name>
    <filename>classpywraplp_1_1Variable.html</filename>
    <member kind="function">
      <type>def</type>
      <name>__init__</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>a3fd12f0e6ae654416a51db05e39d2c41</anchor>
      <arglist>(self, *args, **kwargs)</arglist>
    </member>
    <member kind="function">
      <type>&quot;std::string const &amp;&quot;</type>
      <name>name</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>af885bff2114a3a8b19c4fc07683a0e8c</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;bool&quot;</type>
      <name>integer</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>a56fc4a76f561c23adb92b66ba6aadca3</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>solution_value</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>af2432c2eca4b7c5c516f7a453284d83a</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;int&quot;</type>
      <name>index</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>ac88b7d377d181b5d6127c0d251ea1d21</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>lb</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>a95687841baaedf679c0e989be8d5e651</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>ub</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>a7d24bf162cb07194f5195c7ddb7e4e97</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>SetBounds</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>aa0a198c79c9fd190473e9b3bcf14e4aa</anchor>
      <arglist>(self, &apos;double&apos; lb, &apos;double&apos; ub)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>reduced_cost</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>a94801c028e9a59c74066a14433b5d1c7</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::MPSolver::BasisStatus&quot;</type>
      <name>basis_status</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>afb842490582f8cc246b5f5fc441af172</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;std::string&quot;</type>
      <name>__str__</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>a8e0677a46a9cd87b82b199f8d001c73d</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;std::string&quot;</type>
      <name>__repr__</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>aea95ec239091d0a7c539daad0e6680b9</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>__getattr__</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>a996ae2a3f042f0ad477221dac819ed92</anchor>
      <arglist>(self, name)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>SolutionValue</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>a1550f199ea315f574e5e0acd582729a8</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;bool&quot;</type>
      <name>Integer</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>aee5d5b28c119a5542affdafc2b2908b4</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>Lb</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>a121d93cc78f33574289c2ca513595dd0</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>Ub</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>ae3f5cb7e6c8b723db5233652ff2ec760</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>SetLb</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>a2e92b67cff8ebc6fd31b33bef31645dc</anchor>
      <arglist>(self, &apos;double&apos; x)</arglist>
    </member>
    <member kind="function">
      <type>&quot;void&quot;</type>
      <name>SetUb</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>a850eb76bb192eb102310ee479ea9ba15</anchor>
      <arglist>(self, &apos;double&apos; x)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>ReducedCost</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>ae9362c1e4096320914086ef60ec0f887</anchor>
      <arglist>(self)</arglist>
    </member>
    <member kind="property" static="yes">
      <type></type>
      <name>thisown</name>
      <anchorfile>classpywraplp_1_1Variable.html</anchorfile>
      <anchor>a5c16ef9664033032e20adde8e7ab6097</anchor>
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
    <name>pywraplp</name>
    <filename>namespacepywraplp.html</filename>
    <class kind="class">pywraplp::_SwigNonDynamicMeta</class>
    <class kind="class">pywraplp::Constraint</class>
    <class kind="class">pywraplp::ModelExportOptions</class>
    <class kind="class">pywraplp::MPSolverParameters</class>
    <class kind="class">pywraplp::Objective</class>
    <class kind="class">pywraplp::Solver</class>
    <class kind="class">pywraplp::Variable</class>
    <member kind="function">
      <type>&quot;bool&quot;</type>
      <name>Solver_SupportsProblemType</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>af9e5bb41885004372af323dd0af22b55</anchor>
      <arglist>(&apos;operations_research::MPSolver::OptimizationProblemType&apos; problem_type)</arglist>
    </member>
    <member kind="function">
      <type>&quot;operations_research::MPSolutionResponse *&quot;</type>
      <name>Solver_SolveWithProto</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>a35f9e3adb5239837b87ca4c3b35047da</anchor>
      <arglist>(&apos;operations_research::MPModelRequest const &amp;&apos; model_request, &apos;operations_research::MPSolutionResponse *&apos; response)</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>Solver_infinity</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>a4bf2d52f8542d3a15e817f4644f8e0fe</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>&quot;double&quot;</type>
      <name>Solver_Infinity</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>abbf76685e3b21effa780d2874fb81f77</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>&quot;std::ostream &amp;&quot;</type>
      <name>__lshift__</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>ad77a2f8f2ae677134f6bb1760f8267a4</anchor>
      <arglist>(*args)</arglist>
    </member>
    <member kind="function">
      <type>&quot;std::string&quot;</type>
      <name>ExportModelAsLpFormat</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>a74f7ca30e20d70e59e22f45d45ba4b56</anchor>
      <arglist>(*args)</arglist>
    </member>
    <member kind="function">
      <type>&quot;std::string&quot;</type>
      <name>ExportModelAsMpsFormat</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>ac080b9f23d010cedbcc309121a7dda04</anchor>
      <arglist>(*args)</arglist>
    </member>
    <member kind="function">
      <type>def</type>
      <name>setup_variable_operator</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>a372dfebc0eccbd07f50df10d83428885</anchor>
      <arglist>(opname)</arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>cvar</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>af54064148573c9d853a2680ba79498e9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>kDefaultRelativeMipGap</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>af5b8cb726edf723ee2dc3d33430eabfb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>kDefaultPrimalTolerance</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>a2f355e1fde4adc54c26c2319cd8df440</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>kDefaultDualTolerance</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>a672614bff11eaee732752229c4c233d0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>kDefaultPresolve</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>a7e83b4751d474f2982d2508906481a38</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>kDefaultIncrementality</name>
      <anchorfile>namespacepywraplp.html</anchorfile>
      <anchor>a7c28fd9f4e44d2b1c3d78d43c42526f2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="namespace">
    <name>sorted_interval_list</name>
    <filename>namespacesorted__interval__list.html</filename>
    <class kind="class">sorted_interval_list::_SwigNonDynamicMeta</class>
    <class kind="class">sorted_interval_list::Domain</class>
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
  </compound>
</tagfile>
