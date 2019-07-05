<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile>
  <compound kind="file">
    <name>christofides.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/graph/</path>
    <filename>christofides_8h</filename>
    <includes id="eulerian__path_8h" name="eulerian_path.h" local="yes" imported="no">ortools/graph/eulerian_path.h</includes>
    <includes id="graph_8h" name="graph.h" local="yes" imported="no">ortools/graph/graph.h</includes>
    <includes id="minimum__spanning__tree_8h" name="minimum_spanning_tree.h" local="yes" imported="no">ortools/graph/minimum_spanning_tree.h</includes>
    <class kind="class">operations_research::ChristofidesPathSolver</class>
    <namespace>operations_research</namespace>
  </compound>
  <compound kind="file">
    <name>cliques.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/graph/</path>
    <filename>cliques_8h</filename>
    <class kind="class">operations_research::BronKerboschAlgorithm</class>
    <namespace>operations_research</namespace>
    <member kind="enumeration">
      <type></type>
      <name>CliqueResponse</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>af2d89e69d073dc3036a6de24710b416f</anchor>
      <arglist></arglist>
      <enumvalue file="namespaceoperations__research.html" anchor="af2d89e69d073dc3036a6de24710b416fa2f453cfe638e57e27bb0c9512436111e">CONTINUE</enumvalue>
      <enumvalue file="namespaceoperations__research.html" anchor="af2d89e69d073dc3036a6de24710b416fa615a46af313786fc4e349f34118be111">STOP</enumvalue>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>BronKerboschAlgorithmStatus</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a708cf34b342e7d2ed89a3b73dbec4eae</anchor>
      <arglist></arglist>
      <enumvalue file="namespaceoperations__research.html" anchor="a708cf34b342e7d2ed89a3b73dbec4eaea8f7afecbc8fbc4cd0f50a57d1172482e">COMPLETED</enumvalue>
      <enumvalue file="namespaceoperations__research.html" anchor="a708cf34b342e7d2ed89a3b73dbec4eaea658f2cadfdf09b6046246e9314f7cd43">INTERRUPTED</enumvalue>
    </member>
    <member kind="function">
      <type>void</type>
      <name>FindCliques</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a509097448ff5705cf3e64d889362bdec</anchor>
      <arglist>(std::function&lt; bool(int, int)&gt; graph, int node_count, std::function&lt; bool(const std::vector&lt; int &gt; &amp;)&gt; callback)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CoverArcsByCliques</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>afe4b5a6c0e4019314f288e3f4307c114</anchor>
      <arglist>(std::function&lt; bool(int, int)&gt; graph, int node_count, std::function&lt; bool(const std::vector&lt; int &gt; &amp;)&gt; callback)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>connected_components.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/graph/</path>
    <filename>connected__components_8h</filename>
    <class kind="class">DenseConnectedComponentsFinder</class>
    <class kind="class">ConnectedComponentsFinder</class>
    <namespace>util</namespace>
    <member kind="function">
      <type>std::vector&lt; int &gt;</type>
      <name>GetConnectedComponents</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a13f0e8f7e15873600cf8e395958c71e1</anchor>
      <arglist>(int num_nodes, const UndirectedGraph &amp;graph)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>connectivity.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/graph/</path>
    <filename>connectivity_8h</filename>
    <class kind="class">operations_research::ConnectedComponents</class>
    <namespace>operations_research</namespace>
  </compound>
  <compound kind="file">
    <name>ebert_graph.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/graph/</path>
    <filename>ebert__graph_8h</filename>
    <class kind="class">operations_research::EbertGraph</class>
    <class kind="class">operations_research::ForwardEbertGraph</class>
    <class kind="class">operations_research::ForwardStaticGraph</class>
    <class kind="class">operations_research::StarGraphBase</class>
    <class kind="class">operations_research::StarGraphBase::NodeIterator</class>
    <class kind="class">operations_research::StarGraphBase::ArcIterator</class>
    <class kind="class">operations_research::StarGraphBase::OutgoingArcIterator</class>
    <class kind="class">operations_research::PermutationIndexComparisonByArcHead</class>
    <class kind="class">operations_research::ForwardStaticGraph</class>
    <class kind="class">operations_research::ForwardStaticGraph::CycleHandlerForAnnotatedArcs</class>
    <class kind="class">operations_research::EbertGraphBase</class>
    <class kind="class">operations_research::EbertGraphBase::CycleHandlerForAnnotatedArcs</class>
    <class kind="class">operations_research::EbertGraph</class>
    <class kind="class">operations_research::EbertGraph::OutgoingOrOppositeIncomingArcIterator</class>
    <class kind="class">operations_research::EbertGraph::IncomingArcIterator</class>
    <class kind="class">operations_research::ForwardEbertGraph</class>
    <class kind="struct">operations_research::graph_traits</class>
    <class kind="struct">operations_research::graph_traits&lt; ForwardEbertGraph&lt; NodeIndexType, ArcIndexType &gt; &gt;</class>
    <class kind="struct">operations_research::graph_traits&lt; ForwardStaticGraph&lt; NodeIndexType, ArcIndexType &gt; &gt;</class>
    <class kind="class">operations_research::TailArrayManager</class>
    <class kind="class">operations_research::ArcFunctorOrderingByTailAndHead</class>
    <class kind="class">operations_research::AnnotatedGraphBuildManager</class>
    <namespace>operations_research</namespace>
    <member kind="typedef">
      <type>int32</type>
      <name>NodeIndex</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a0e629e35bfa311b31dd7f5065eb834bb</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>int32</type>
      <name>ArcIndex</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a31d858394c5eed1fa21edc3da47047c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>int64</type>
      <name>FlowQuantity</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a5841ff601ab08548afb15c45b2245de7</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>int64</type>
      <name>CostValue</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>aa7950685633ee869aa9471b2ec5fbcfa</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>EbertGraph&lt; NodeIndex, ArcIndex &gt;</type>
      <name>StarGraph</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ae39f15b318a3cba17b1e60e6da51c0d4</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ForwardEbertGraph&lt; NodeIndex, ArcIndex &gt;</type>
      <name>ForwardStarGraph</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a652af62fa5f211aa0c54d7994ca1c504</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ForwardStaticGraph&lt; NodeIndex, ArcIndex &gt;</type>
      <name>ForwardStarStaticGraph</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ac7440a08c859325694df19d4d4aee95c</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ZVector&lt; NodeIndex &gt;</type>
      <name>NodeIndexArray</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a389e5320fb5bcd0fb99d894488f9820b</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ZVector&lt; ArcIndex &gt;</type>
      <name>ArcIndexArray</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>aa79bf252fa6483cd33cbf95170353fb0</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ZVector&lt; FlowQuantity &gt;</type>
      <name>QuantityArray</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a7d4fc0319cb4e28ec175fc9163775a6e</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ZVector&lt; CostValue &gt;</type>
      <name>CostArray</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>afdee62ecefa0520e530c18a55b083e6d</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>BuildLineGraph</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>acb53c505b8fd29ceb3abdcc7dfd809ce</anchor>
      <arglist>(const GraphType &amp;graph, GraphType *const line_graph)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>eulerian_path.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/graph/</path>
    <filename>eulerian__path_8h</filename>
    <namespace>operations_research</namespace>
    <member kind="function">
      <type>bool</type>
      <name>IsEulerianGraph</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab1cf773de0cae72d0c44efe5b8f4bb89</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsSemiEulerianGraph</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a6b312dd19c90b2af099e6f159869f7ee</anchor>
      <arglist>(const Graph &amp;graph, std::vector&lt; NodeIndex &gt; *odd_nodes)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; NodeIndex &gt;</type>
      <name>BuildEulerianPathFromNode</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a743d8c9d6f64531bdeb7bbf18023e9d4</anchor>
      <arglist>(const Graph &amp;graph, NodeIndex root)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; NodeIndex &gt;</type>
      <name>BuildEulerianTourFromNode</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>aa63055860fc53f8eed56d23d2571c180</anchor>
      <arglist>(const Graph &amp;graph, NodeIndex root)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; typename Graph::NodeIndex &gt;</type>
      <name>BuildEulerianTour</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a034666fe63ca105b735272974006362a</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; typename Graph::NodeIndex &gt;</type>
      <name>BuildEulerianPath</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a49b170b2d03863c465331e67b21f0c33</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>graph.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/graph/</path>
    <filename>graph_8h</filename>
    <includes id="iterators_8h" name="iterators.h" local="yes" imported="no">ortools/graph/iterators.h</includes>
    <class kind="class">util::SVector</class>
    <class kind="class">util::BaseGraph</class>
    <class kind="class">util::ListGraph</class>
    <class kind="class">util::StaticGraph</class>
    <class kind="class">util::ReverseArcListGraph</class>
    <class kind="class">util::ReverseArcStaticGraph</class>
    <class kind="class">util::ReverseArcMixedGraph</class>
    <class kind="class">util::SVector</class>
    <class kind="class">util::ListGraph::OutgoingArcIterator</class>
    <class kind="class">util::ListGraph::OutgoingHeadIterator</class>
    <class kind="class">util::StaticGraph::OutgoingArcIterator</class>
    <class kind="class">util::ReverseArcListGraph::OutgoingArcIterator</class>
    <class kind="class">util::ReverseArcListGraph::OppositeIncomingArcIterator</class>
    <class kind="class">util::ReverseArcListGraph::IncomingArcIterator</class>
    <class kind="class">util::ReverseArcListGraph::OutgoingOrOppositeIncomingArcIterator</class>
    <class kind="class">util::ReverseArcListGraph::OutgoingHeadIterator</class>
    <class kind="class">util::ReverseArcStaticGraph::OutgoingArcIterator</class>
    <class kind="class">util::ReverseArcStaticGraph::OppositeIncomingArcIterator</class>
    <class kind="class">util::ReverseArcStaticGraph::IncomingArcIterator</class>
    <class kind="class">util::ReverseArcStaticGraph::OutgoingOrOppositeIncomingArcIterator</class>
    <class kind="class">util::ReverseArcMixedGraph::OutgoingArcIterator</class>
    <class kind="class">util::ReverseArcMixedGraph::OppositeIncomingArcIterator</class>
    <class kind="class">util::ReverseArcMixedGraph::IncomingArcIterator</class>
    <class kind="class">util::ReverseArcMixedGraph::OutgoingOrOppositeIncomingArcIterator</class>
    <class kind="class">util::CompleteGraph</class>
    <class kind="class">util::CompleteBipartiteGraph</class>
    <class kind="class">util::CompleteBipartiteGraph::OutgoingArcIterator</class>
    <namespace>util</namespace>
    <member kind="define">
      <type>#define</type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>graph_8h.html</anchorfile>
      <anchor>a48a8a7aa004fc40d0d1d0ba63311cece</anchor>
      <arglist>(c, t, e)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DEFINE_STL_ITERATOR_FUNCTIONS</name>
      <anchorfile>graph_8h.html</anchorfile>
      <anchor>aa560f5e55268f818d5e5f43ed31e19a0</anchor>
      <arglist>(iterator_class_name)</arglist>
    </member>
    <member kind="typedef">
      <type>ListGraph</type>
      <name>Graph</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ae76339cb2dcd3bc05ad762146f91fdda</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>PermuteWithExplicitElementType</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a9470623ca7db3c4a62ce3b326c6b07d8</anchor>
      <arglist>(const IntVector &amp;permutation, Array *array_to_permute, ElementType unused)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Permute</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a8c227a057c1ce9d46b1185abf77ad91e</anchor>
      <arglist>(const IntVector &amp;permutation, Array *array_to_permute)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Permute</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ac497881c4166bc694adc4bee62746118</anchor>
      <arglist>(const IntVector &amp;permutation, std::vector&lt; bool &gt; *array_to_permute)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a37be0131ae922e30a286797a0bef0c96</anchor>
      <arglist>(ListGraph, Outgoing, Base::kNilArc)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>af3c40fc068f645d9dcd15c332e44fc25</anchor>
      <arglist>(StaticGraph, Outgoing, DirectArcLimit(node))</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a3098e161a6aceeca482be78d2d221b3b</anchor>
      <arglist>(ReverseArcListGraph, Outgoing, Base::kNilArc)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a4d0ae05975a2063f2edbeb749f690fc7</anchor>
      <arglist>(ReverseArcListGraph, Incoming, Base::kNilArc)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a22b5dcc01043ab8da01ebab71ec3ad31</anchor>
      <arglist>(ReverseArcListGraph, OutgoingOrOppositeIncoming, Base::kNilArc)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a863ccdb51afb5ef92fe6c94188a5f7e0</anchor>
      <arglist>(ReverseArcListGraph, OppositeIncoming, Base::kNilArc)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a2cc2a1037195d237820edc97d35404be</anchor>
      <arglist>(ReverseArcStaticGraph, Outgoing, DirectArcLimit(node))</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a2a51d676cd5d9354bfe1f80d09c44f39</anchor>
      <arglist>(ReverseArcStaticGraph, Incoming, ReverseArcLimit(node))</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a1db1a919e67261878ff8abda53e664c7</anchor>
      <arglist>(ReverseArcStaticGraph, OutgoingOrOppositeIncoming, DirectArcLimit(node))</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a1728675285eb75f9f18d6ed7c134d0b6</anchor>
      <arglist>(ReverseArcStaticGraph, OppositeIncoming, ReverseArcLimit(node))</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ab3308688d13e59e2351bef038ce1fdb0</anchor>
      <arglist>(ReverseArcMixedGraph, Outgoing, DirectArcLimit(node))</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a3c022b68f68916770fe09996df2f35a3</anchor>
      <arglist>(ReverseArcMixedGraph, Incoming, Base::kNilArc)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a97910ddfce7560b406aa3f4939434eb8</anchor>
      <arglist>(ReverseArcMixedGraph, OutgoingOrOppositeIncoming, DirectArcLimit(node))</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a6ce1a67d16c75b202f56301321a457c6</anchor>
      <arglist>(ReverseArcMixedGraph, OppositeIncoming, Base::kNilArc)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>graphs.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/graph/</path>
    <filename>graphs_8h</filename>
    <includes id="ebert__graph_8h" name="ebert_graph.h" local="yes" imported="no">ortools/graph/ebert_graph.h</includes>
    <class kind="struct">operations_research::Graphs</class>
    <class kind="struct">operations_research::Graphs&lt; operations_research::StarGraph &gt;</class>
    <namespace>operations_research</namespace>
  </compound>
  <compound kind="file">
    <name>hamiltonian_path.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/graph/</path>
    <filename>hamiltonian__path_8h</filename>
    <class kind="class">operations_research::ElementIterator</class>
    <class kind="class">operations_research::Set</class>
    <class kind="class">operations_research::SetRangeIterator</class>
    <class kind="class">operations_research::SetRangeWithCardinality</class>
    <class kind="class">operations_research::LatticeMemoryManager</class>
    <class kind="class">operations_research::HamiltonianPathSolver</class>
    <class kind="class">operations_research::PruningHamiltonianSolver</class>
    <namespace>operations_research</namespace>
    <member kind="typedef">
      <type>int</type>
      <name>PathNodeIndex</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a09767b3634289e432c3ce1d7c649666a</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>HamiltonianPathSolver&lt; CostType, CostFunction &gt;</type>
      <name>MakeHamiltonianPathSolver</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a715b0dbb9f0903ab629b8c6da1b35b45</anchor>
      <arglist>(int num_nodes, CostFunction cost)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>io.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/graph/</path>
    <filename>io_8h</filename>
    <includes id="graph_8h" name="graph.h" local="yes" imported="no">ortools/graph/graph.h</includes>
    <namespace>util</namespace>
    <member kind="enumeration">
      <type></type>
      <name>GraphToStringFormat</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ae705e1342dacc10a13fb3f11f91d0696</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PRINT_GRAPH_ARCS</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ae705e1342dacc10a13fb3f11f91d0696acac9245da1bf36d1d9382dc579e1a4fd</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PRINT_GRAPH_ADJACENCY_LISTS</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ae705e1342dacc10a13fb3f11f91d0696aaed5759e3b6e3a8592c9a21e0048b565</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PRINT_GRAPH_ADJACENCY_LISTS_SORTED</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ae705e1342dacc10a13fb3f11f91d0696a454bb1ede69e280a1e4959acb82748ef</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>GraphToString</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>affed79554a202aaa8bda5b5e98c3a6b2</anchor>
      <arglist>(const Graph &amp;graph, GraphToStringFormat format)</arglist>
    </member>
    <member kind="function">
      <type>false</type>
      <name>ValueOrDie</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a86199e4832dd5c1d61baa53bfecb7b6d</anchor>
      <arglist>())</arglist>
    </member>
    <member kind="function">
      <type>*</type>
      <name>if</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a0a640b5d8a0ba7deaba9afbd4f3ca438</anchor>
      <arglist>(!error_or_graph.ok())</arglist>
    </member>
    <member kind="function">
      <type>***util::StatusOr&lt; Graph * &gt;</type>
      <name>ReadGraphFile</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a9c5c6763e52cd1465a3e1a3ab2437e37</anchor>
      <arglist>(const std::string &amp;filename, bool directed, std::vector&lt; int &gt; *num_nodes_with_color_or_null)</arglist>
    </member>
    <member kind="function">
      <type>util::Status</type>
      <name>WriteGraphToFile</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a6600986f328a49c9485aa03fb6c82946</anchor>
      <arglist>(const Graph &amp;graph, const std::string &amp;filename, bool directed, const std::vector&lt; int &gt; &amp;num_nodes_with_color)</arglist>
    </member>
    <member kind="function">
      <type>util::StatusOr&lt; Graph * &gt;</type>
      <name>ReadGraphFile</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>aeec5b66be4fd6b3021e6eb08b3045a0e</anchor>
      <arglist>(const std::string &amp;filename, bool directed, std::vector&lt; int &gt; *num_nodes_with_color_or_null)</arglist>
    </member>
    <member kind="variable">
      <type>***util::StatusOr&lt; MyGraph * &gt;</type>
      <name>error_or_graph</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a123e77d101e4aeb54a2b9e7d9612ad1b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>else</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a99d2b83baf3f908e76fb2161b1c73322</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>iterators.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/graph/</path>
    <filename>iterators_8h</filename>
    <class kind="struct">MutableVectorIteration::Iterator</class>
    <namespace>util</namespace>
    <member kind="typedef">
      <type>typename std::iterator_traits&lt; Iterator &gt;::value_type</type>
      <name>value_type</name>
      <anchorfile>iterators_8h.html</anchorfile>
      <anchor>ae7f303a443fbf651b13f8289d05ef498</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>BeginEndWrapper</name>
      <anchorfile>iterators_8h.html</anchorfile>
      <anchor>af3f6bc803bbe87af730cf9e41a35cf68</anchor>
      <arglist>(Iterator begin, Iterator end)</arglist>
    </member>
    <member kind="function">
      <type>Iterator</type>
      <name>begin</name>
      <anchorfile>iterators_8h.html</anchorfile>
      <anchor>a09dd208593b9721a30a83ed978ede577</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>Iterator</type>
      <name>end</name>
      <anchorfile>iterators_8h.html</anchorfile>
      <anchor>a62469461ed7c932afba3808f4da0fe3d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>empty</name>
      <anchorfile>iterators_8h.html</anchorfile>
      <anchor>a644718bb2fb240de962dc3c9a1fdf0dc</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>false</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>aadd7603ae6e78cc2490ca9710fbaf180</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>*OutgoingArcIterator</type>
      <name>this</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>acc90f8dbcd326a450a7c781ea7a9539d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>**And a client will use it like</type>
      <name>this</name>
      <anchorfile>iterators_8h.html</anchorfile>
      <anchor>a83b9f519556accd1f067da73da5f0624</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>linear_assignment.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/graph/</path>
    <filename>linear__assignment_8h</filename>
    <includes id="ebert__graph_8h" name="ebert_graph.h" local="yes" imported="no">ortools/graph/ebert_graph.h</includes>
    <class kind="class">operations_research::LinearSumAssignment</class>
    <class kind="class">operations_research::LinearSumAssignment::BipartiteLeftNodeIterator</class>
    <class kind="class">operations_research::CostValueCycleHandler</class>
    <class kind="class">operations_research::ArcIndexOrderingByTailNode</class>
    <namespace>operations_research</namespace>
    <member kind="function">
      <type></type>
      <name>DECLARE_int64</name>
      <anchorfile>linear__assignment_8h.html</anchorfile>
      <anchor>aeb1d0880abde13d03b5dc361d19d8cf3</anchor>
      <arglist>(assignment_alpha)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_int32</name>
      <anchorfile>linear__assignment_8h.html</anchorfile>
      <anchor>a7e9c0b76beb761af447a08684cded9a7</anchor>
      <arglist>(assignment_progress_logging_period)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_bool</name>
      <anchorfile>linear__assignment_8h.html</anchorfile>
      <anchor>a9982454bded965321d3f3b7d5300b0dc</anchor>
      <arglist>(assignment_stack_order)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>max_flow.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/graph/</path>
    <filename>max__flow_8h</filename>
    <includes id="ebert__graph_8h" name="ebert_graph.h" local="yes" imported="no">ortools/graph/ebert_graph.h</includes>
    <includes id="graph_8h" name="graph.h" local="yes" imported="no">ortools/graph/graph.h</includes>
    <class kind="class">operations_research::GenericMaxFlow</class>
    <class kind="class">operations_research::SimpleMaxFlow</class>
    <class kind="class">operations_research::PriorityQueueWithRestrictedPush</class>
    <class kind="class">operations_research::MaxFlowStatusClass</class>
    <class kind="class">operations_research::GenericMaxFlow</class>
    <class kind="class">operations_research::MaxFlow</class>
    <namespace>operations_research</namespace>
  </compound>
  <compound kind="file">
    <name>min_cost_flow.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/graph/</path>
    <filename>min__cost__flow_8h</filename>
    <includes id="ebert__graph_8h" name="ebert_graph.h" local="yes" imported="no">ortools/graph/ebert_graph.h</includes>
    <includes id="graph_8h" name="graph.h" local="yes" imported="no">ortools/graph/graph.h</includes>
    <class kind="class">operations_research::GenericMinCostFlow</class>
    <class kind="class">operations_research::MinCostFlowBase</class>
    <class kind="class">operations_research::SimpleMinCostFlow</class>
    <class kind="class">operations_research::GenericMinCostFlow</class>
    <class kind="class">operations_research::MinCostFlow</class>
    <namespace>operations_research</namespace>
  </compound>
  <compound kind="file">
    <name>minimum_spanning_tree.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/graph/</path>
    <filename>minimum__spanning__tree_8h</filename>
    <includes id="connectivity_8h" name="connectivity.h" local="yes" imported="no">ortools/graph/connectivity.h</includes>
    <namespace>operations_research</namespace>
    <member kind="function">
      <type>std::vector&lt; typename Graph::ArcIndex &gt;</type>
      <name>BuildKruskalMinimumSpanningTreeFromSortedArcs</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a00ab79ee21ffd8dece0996e37f9faa7a</anchor>
      <arglist>(const Graph &amp;graph, const std::vector&lt; typename Graph::ArcIndex &gt; &amp;sorted_arcs)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; typename Graph::ArcIndex &gt;</type>
      <name>BuildKruskalMinimumSpanningTree</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>aa565a47a059ef32ef1aec39768e4ec98</anchor>
      <arglist>(const Graph &amp;graph, const ArcComparator &amp;arc_comparator)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; typename Graph::ArcIndex &gt;</type>
      <name>BuildPrimMinimumSpanningTree</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a33a2f4c26fd60cd0fa98257b571c974f</anchor>
      <arglist>(const Graph &amp;graph, const ArcValue &amp;arc_value)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>one_tree_lower_bound.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/graph/</path>
    <filename>one__tree__lower__bound_8h</filename>
    <includes id="christofides_8h" name="christofides.h" local="yes" imported="no">ortools/graph/christofides.h</includes>
    <includes id="minimum__spanning__tree_8h" name="minimum_spanning_tree.h" local="yes" imported="no">ortools/graph/minimum_spanning_tree.h</includes>
    <member kind="function">
      <type>trees with all degrees equal w the current value of int</type>
      <name>step1_</name>
      <anchorfile>one__tree__lower__bound_8h.html</anchorfile>
      <anchor>adba1a0c02ca6636f5610de2337f3f697</anchor>
      <arglist>(0)</arglist>
    </member>
    <member kind="function">
      <type>trees with all degrees equal w the current value of int</type>
      <name>iteration_</name>
      <anchorfile>one__tree__lower__bound_8h.html</anchorfile>
      <anchor>a6e4c1555a52b411d0e25bae82066dcc4</anchor>
      <arglist>(0)</arglist>
    </member>
    <member kind="function">
      <type>trees with all degrees equal w the current value of int</type>
      <name>max_iterations_</name>
      <anchorfile>one__tree__lower__bound_8h.html</anchorfile>
      <anchor>ad15ce7e24cde3d28bc88c4a32bf3a834</anchor>
      <arglist>(max_iterations &gt; 0 ? max_iterations :MaxIterations(number_of_nodes))</arglist>
    </member>
    <member kind="function">
      <type>trees with all degrees equal w the current value of int</type>
      <name>number_of_nodes_</name>
      <anchorfile>one__tree__lower__bound_8h.html</anchorfile>
      <anchor>a01198f1b6b698fc8c033ef878977c27d</anchor>
      <arglist>(number_of_nodes)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Next</name>
      <anchorfile>one__tree__lower__bound_8h.html</anchorfile>
      <anchor>a3947d19ac087ef2cd68c2409920339c4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>GetStep</name>
      <anchorfile>one__tree__lower__bound_8h.html</anchorfile>
      <anchor>a61aed6a943277e531b904cfdc3616890</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>OnOneTree</name>
      <anchorfile>one__tree__lower__bound_8h.html</anchorfile>
      <anchor>a33c2c5b8d838c77c2701a538f7f30ae4</anchor>
      <arglist>(CostType one_tree_cost, double w, const std::vector&lt; int &gt; &amp;degrees)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>OnNewWMax</name>
      <anchorfile>one__tree__lower__bound_8h.html</anchorfile>
      <anchor>a2ad04ff9537d97fcabc58c86183890c3</anchor>
      <arglist>(CostType one_tree_cost)</arglist>
    </member>
    <member kind="variable">
      <type>trees with all degrees equal</type>
      <name>to</name>
      <anchorfile>one__tree__lower__bound_8h.html</anchorfile>
      <anchor>a717d5dae07519577a5ed09ac24a4708b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>trees with all degrees equal</type>
      <name>therefore</name>
      <anchorfile>one__tree__lower__bound_8h.html</anchorfile>
      <anchor>a6f353d8684dff1645f1601cd68b9a0be</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>trees with all degrees equal w the current value of</type>
      <name>w</name>
      <anchorfile>one__tree__lower__bound_8h.html</anchorfile>
      <anchor>a095b3978da7ad09782c0be1a6572e352</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>trees with all degrees equal w the current value of</type>
      <name>degrees</name>
      <anchorfile>one__tree__lower__bound_8h.html</anchorfile>
      <anchor>af86d4c4e6a18908cda194651a9ab7beb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>trees with all degrees equal w the current value of int</type>
      <name>max_iterations</name>
      <anchorfile>one__tree__lower__bound_8h.html</anchorfile>
      <anchor>af85cb5d69bd041414717718f6a833325</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>shortestpaths.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/graph/</path>
    <filename>shortestpaths_8h</filename>
    <namespace>operations_research</namespace>
    <member kind="function">
      <type>bool</type>
      <name>DijkstraShortestPath</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a53e6a83fcbd689abf5b3078b0236f9f1</anchor>
      <arglist>(int node_count, int start_node, int end_node, std::function&lt; int64(int, int)&gt; graph, int64 disconnected_distance, std::vector&lt; int &gt; *nodes)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>StableDijkstraShortestPath</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ad56bae19a2298c3163af96ca7f8b89b1</anchor>
      <arglist>(int node_count, int start_node, int end_node, std::function&lt; int64(int, int)&gt; graph, int64 disconnected_distance, std::vector&lt; int &gt; *nodes)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>BellmanFordShortestPath</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ad5bfec6ea714171fbff2d8b791d0d286</anchor>
      <arglist>(int node_count, int start_node, int end_node, std::function&lt; int64(int, int)&gt; graph, int64 disconnected_distance, std::vector&lt; int &gt; *nodes)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>AStarShortestPath</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>acff272be25bcf9641218c05c59ec1a4e</anchor>
      <arglist>(int node_count, int start_node, int end_node, std::function&lt; int64(int, int)&gt; graph, std::function&lt; int64(int)&gt; heuristic, int64 disconnected_distance, std::vector&lt; int &gt; *nodes)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>strongly_connected_components.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/graph/</path>
    <filename>strongly__connected__components_8h</filename>
    <class kind="struct">SccCounterOutput</class>
    <member kind="function">
      <type>void</type>
      <name>FindStronglyConnectedComponents</name>
      <anchorfile>strongly__connected__components_8h.html</anchorfile>
      <anchor>aafab5785b250e1013c13511ce478f36b</anchor>
      <arglist>(const NodeIndex num_nodes, const Graph &amp;graph, SccOutput *components)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>util.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/graph/</path>
    <filename>util_8h</filename>
    <includes id="connected__components_8h" name="connected_components.h" local="yes" imported="no">ortools/graph/connected_components.h</includes>
    <includes id="graph_8h" name="graph.h" local="yes" imported="no">ortools/graph/graph.h</includes>
    <includes id="iterators_8h" name="iterators.h" local="yes" imported="no">ortools/graph/iterators.h</includes>
    <class kind="class">util::UndirectedAdjacencyListsOfDirectedGraph</class>
    <class kind="class">util::UndirectedAdjacencyListsOfDirectedGraph::AdjacencyListIterator</class>
    <namespace>util</namespace>
    <member kind="function">
      <type>bool</type>
      <name>GraphHasSelfArcs</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ac4af76993c891ee4ad507783edec2a1c</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>GraphHasDuplicateArcs</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a6b37593970a26f5c88b3d2ea9acea9d2</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>GraphIsSymmetric</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a784b483eeae1b49164a8a02fe9c0d3ba</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>GraphIsWeaklyConnected</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a97311561fd1f01e9f35b2f7ce18b0af3</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>std::unique_ptr&lt; Graph &gt;</type>
      <name>CopyGraph</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a0ed748741b17dad9e6cc485728bb0043</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>std::unique_ptr&lt; Graph &gt;</type>
      <name>RemapGraph</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>acfecdce43e9933bde2a94fd879f12f5f</anchor>
      <arglist>(const Graph &amp;graph, const std::vector&lt; int &gt; &amp;new_node_index)</arglist>
    </member>
    <member kind="function">
      <type>std::unique_ptr&lt; Graph &gt;</type>
      <name>GetSubgraphOfNodes</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ad1df170a504d335462a1104a942e6069</anchor>
      <arglist>(const Graph &amp;graph, const std::vector&lt; int &gt; &amp;nodes)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; int &gt;</type>
      <name>GetWeaklyConnectedComponents</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ab34783e729bb5fc99042893f6bfcbb2f</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsSubsetOf0N</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>aa9fb4c9a176acaf72053b11727436e9e</anchor>
      <arglist>(const std::vector&lt; int &gt; &amp;v, int n)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsValidPermutation</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ad7986b01cf61a31c09a27b4a97db6a83</anchor>
      <arglist>(const std::vector&lt; int &gt; &amp;v)</arglist>
    </member>
    <member kind="function">
      <type>std::unique_ptr&lt; Graph &gt;</type>
      <name>RemoveSelfArcsAndDuplicateArcs</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a8a06031908a024a50dbdddc394a22490</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>RemoveCyclesFromPath</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a06fa201576c927d92657e090fa86bfdb</anchor>
      <arglist>(const Graph &amp;graph, std::vector&lt; int &gt; *arc_path)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>PathHasCycle</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>adbb18bcb2f9d64cbbaeb57c328f57e7b</anchor>
      <arglist>(const Graph &amp;graph, const std::vector&lt; int &gt; &amp;arc_path)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; int &gt;</type>
      <name>ComputeOnePossibleReverseArcMapping</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ae469c559688b92f36bae2788c2e6063e</anchor>
      <arglist>(const Graph &amp;graph, bool die_if_not_symmetric)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::UndirectedAdjacencyListsOfDirectedGraph::AdjacencyListIterator</name>
    <filename>classutil_1_1UndirectedAdjacencyListsOfDirectedGraph_1_1AdjacencyListIterator.html</filename>
    <member kind="function">
      <type></type>
      <name>AdjacencyListIterator</name>
      <anchorfile>classutil_1_1UndirectedAdjacencyListsOfDirectedGraph_1_1AdjacencyListIterator.html</anchorfile>
      <anchor>a12996aa34ae37ed7efa6fb2eca4ff54e</anchor>
      <arglist>(const Graph &amp;graph, ArcIterator &amp;&amp;arc_it)</arglist>
    </member>
    <member kind="function">
      <type>Graph::NodeIndex</type>
      <name>operator *</name>
      <anchorfile>classutil_1_1UndirectedAdjacencyListsOfDirectedGraph_1_1AdjacencyListIterator.html</anchorfile>
      <anchor>a9c411230b6cd51ca80cb31a1f8387ae8</anchor>
      <arglist>() const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::AnnotatedGraphBuildManager</name>
    <filename>classoperations__research_1_1AnnotatedGraphBuildManager.html</filename>
    <templarg></templarg>
    <member kind="function">
      <type></type>
      <name>AnnotatedGraphBuildManager</name>
      <anchorfile>classoperations__research_1_1AnnotatedGraphBuildManager.html</anchorfile>
      <anchor>a54e58efb7e00c121962f0642e086ff62</anchor>
      <arglist>(typename GraphType::NodeIndex num_nodes, typename GraphType::ArcIndex num_arcs, bool sort_arcs)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::ArcFunctorOrderingByTailAndHead</name>
    <filename>classoperations__research_1_1ArcFunctorOrderingByTailAndHead.html</filename>
    <templarg></templarg>
    <member kind="function">
      <type></type>
      <name>ArcFunctorOrderingByTailAndHead</name>
      <anchorfile>classoperations__research_1_1ArcFunctorOrderingByTailAndHead.html</anchorfile>
      <anchor>a47fb62aeee589ee246d918654aa5fcbf</anchor>
      <arglist>(const GraphType &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>operator()</name>
      <anchorfile>classoperations__research_1_1ArcFunctorOrderingByTailAndHead.html</anchorfile>
      <anchor>adcc7e1ea101f9a6a7f276d7d5ae85926</anchor>
      <arglist>(typename GraphType::ArcIndex a, typename GraphType::ArcIndex b) const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::ArcIndexOrderingByTailNode</name>
    <filename>classoperations__research_1_1ArcIndexOrderingByTailNode.html</filename>
    <templarg>GraphType</templarg>
    <member kind="function">
      <type></type>
      <name>ArcIndexOrderingByTailNode</name>
      <anchorfile>classoperations__research_1_1ArcIndexOrderingByTailNode.html</anchorfile>
      <anchor>abe28f103a119480ebe81781bec2b980e</anchor>
      <arglist>(const GraphType &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>operator()</name>
      <anchorfile>classoperations__research_1_1ArcIndexOrderingByTailNode.html</anchorfile>
      <anchor>a469f26e2984e4bb6058323ff48609f08</anchor>
      <arglist>(typename GraphType::ArcIndex a, typename GraphType::ArcIndex b) const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::StarGraphBase::ArcIterator</name>
    <filename>classoperations__research_1_1StarGraphBase_1_1ArcIterator.html</filename>
    <member kind="function">
      <type></type>
      <name>ArcIterator</name>
      <anchorfile>classoperations__research_1_1StarGraphBase_1_1ArcIterator.html</anchorfile>
      <anchor>a3656b335cbb811daf46ffbc07629c101</anchor>
      <arglist>(const DerivedGraph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classoperations__research_1_1StarGraphBase_1_1ArcIterator.html</anchorfile>
      <anchor>ad1b734838923b0401394878f884f80bf</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classoperations__research_1_1StarGraphBase_1_1ArcIterator.html</anchorfile>
      <anchor>a261b6e58a0dfd38d8d0c0d882c2ccf8d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase_1_1ArcIterator.html</anchorfile>
      <anchor>a682828ce4c89cae5de8accd10a9a12c4</anchor>
      <arglist>() const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::BaseGraph</name>
    <filename>classutil_1_1BaseGraph.html</filename>
    <templarg>NodeIndexType</templarg>
    <templarg>ArcIndexType</templarg>
    <templarg>HasReverseArcs</templarg>
    <member kind="typedef">
      <type>NodeIndexType</type>
      <name>NodeIndex</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>adb271cf4bcf2de5b5bbe300d7054af29</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ArcIndexType</type>
      <name>ArcIndex</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a0eba6e5899924388644dfa2258ae8929</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>BaseGraph</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab2aa905e49f98689100f071c493d20fa</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~BaseGraph</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ad2353019d9890202a2220fd230940fb5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>num_nodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a0f551c921fa0b5aaa334a6e36f61db4a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>num_arcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aefb468e4d4a3128c91b3bad9f5b314c8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; NodeIndex &gt;</type>
      <name>AllNodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a32ba4a5ca9a4b89f750eb2dc56518b02</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; ArcIndex &gt;</type>
      <name>AllForwardArcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>abcfd7c21143e5ed38573c0dd60826dd8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>abf853acea86e01356f53055f77661770</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsArcValid</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a2097ccda3b1ad27e2c82166979018bda</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>node_capacity</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a463d57480c9563a7a707c5d0928c9946</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>arc_capacity</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a92ffd852b2ab2e5241f9832e71a2de71</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>ReserveNodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a69a71cbb575b13bde9899f5a6a217139</anchor>
      <arglist>(NodeIndexType bound)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>ReserveArcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a9563f7154a759843923dd9bb27b288e7</anchor>
      <arglist>(ArcIndexType bound)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Reserve</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab731ca5c638e7b1b0a2c459c94a90f55</anchor>
      <arglist>(NodeIndexType node_capacity, ArcIndexType arc_capacity)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>FreezeCapacities</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aa3bd051d1e141b09dda17aa9b5f24f69</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GroupForwardArcsByFunctor</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a07214b96597069d781e27b1dd17ef83e</anchor>
      <arglist>(const A &amp;a, B *b)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_end_arc_index</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a346b8a5811f0e287e1ebce2de2c1ad28</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kNilNode</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab1292c82a3f43be3bd57b63a05fe0214</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kNilArc</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ad5b77846f77c2771e840820812ad5521</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>ComputeCumulativeSum</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ac47030fcb75a3220f8cf9ed79697056e</anchor>
      <arglist>(std::vector&lt; ArcIndexType &gt; *v)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>BuildStartAndForwardHead</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a63fd586eed6c345866317e2f0faf377e</anchor>
      <arglist>(SVector&lt; NodeIndexType &gt; *head, std::vector&lt; ArcIndexType &gt; *start, std::vector&lt; ArcIndexType &gt; *permutation)</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>num_nodes_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a473db46afec1eabf0762411830dee30f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>node_capacity_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a424bd34a9767e7edeaf3a60ecd3cb000</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>num_arcs_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>af16f99d41856a7b22ae8a226ef09abff</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>arc_capacity_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a895e891d1ad52ce3efcfeb7ba11194c7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>const_capacities_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aa980e5526b9ded17a83928fc339c71e4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>BaseGraph&lt; NodeIndexType, ArcIndexType, false &gt;</name>
    <filename>classutil_1_1BaseGraph.html</filename>
    <member kind="typedef">
      <type>NodeIndexType</type>
      <name>NodeIndex</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>adb271cf4bcf2de5b5bbe300d7054af29</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ArcIndexType</type>
      <name>ArcIndex</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a0eba6e5899924388644dfa2258ae8929</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>BaseGraph</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab2aa905e49f98689100f071c493d20fa</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~BaseGraph</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ad2353019d9890202a2220fd230940fb5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>num_nodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a0f551c921fa0b5aaa334a6e36f61db4a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>num_arcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aefb468e4d4a3128c91b3bad9f5b314c8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; NodeIndex &gt;</type>
      <name>AllNodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a32ba4a5ca9a4b89f750eb2dc56518b02</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; ArcIndex &gt;</type>
      <name>AllForwardArcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>abcfd7c21143e5ed38573c0dd60826dd8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>abf853acea86e01356f53055f77661770</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsArcValid</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a2097ccda3b1ad27e2c82166979018bda</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>node_capacity</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a463d57480c9563a7a707c5d0928c9946</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>arc_capacity</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a92ffd852b2ab2e5241f9832e71a2de71</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>ReserveNodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a69a71cbb575b13bde9899f5a6a217139</anchor>
      <arglist>(NodeIndexType bound)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>ReserveArcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a9563f7154a759843923dd9bb27b288e7</anchor>
      <arglist>(ArcIndexType bound)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Reserve</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab731ca5c638e7b1b0a2c459c94a90f55</anchor>
      <arglist>(NodeIndexType node_capacity, ArcIndexType arc_capacity)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>FreezeCapacities</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aa3bd051d1e141b09dda17aa9b5f24f69</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GroupForwardArcsByFunctor</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a07214b96597069d781e27b1dd17ef83e</anchor>
      <arglist>(const A &amp;a, B *b)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_end_arc_index</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a346b8a5811f0e287e1ebce2de2c1ad28</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kNilNode</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab1292c82a3f43be3bd57b63a05fe0214</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kNilArc</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ad5b77846f77c2771e840820812ad5521</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>ComputeCumulativeSum</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ac47030fcb75a3220f8cf9ed79697056e</anchor>
      <arglist>(std::vector&lt; ArcIndexType &gt; *v)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>BuildStartAndForwardHead</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a63fd586eed6c345866317e2f0faf377e</anchor>
      <arglist>(SVector&lt; NodeIndexType &gt; *head, std::vector&lt; ArcIndexType &gt; *start, std::vector&lt; ArcIndexType &gt; *permutation)</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>num_nodes_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a473db46afec1eabf0762411830dee30f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>node_capacity_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a424bd34a9767e7edeaf3a60ecd3cb000</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>num_arcs_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>af16f99d41856a7b22ae8a226ef09abff</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>arc_capacity_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a895e891d1ad52ce3efcfeb7ba11194c7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>const_capacities_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aa980e5526b9ded17a83928fc339c71e4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>BaseGraph&lt; NodeIndexType, ArcIndexType, true &gt;</name>
    <filename>classutil_1_1BaseGraph.html</filename>
    <member kind="typedef">
      <type>NodeIndexType</type>
      <name>NodeIndex</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>adb271cf4bcf2de5b5bbe300d7054af29</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ArcIndexType</type>
      <name>ArcIndex</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a0eba6e5899924388644dfa2258ae8929</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>BaseGraph</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab2aa905e49f98689100f071c493d20fa</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~BaseGraph</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ad2353019d9890202a2220fd230940fb5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>num_nodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a0f551c921fa0b5aaa334a6e36f61db4a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>num_arcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aefb468e4d4a3128c91b3bad9f5b314c8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; NodeIndex &gt;</type>
      <name>AllNodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a32ba4a5ca9a4b89f750eb2dc56518b02</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; ArcIndex &gt;</type>
      <name>AllForwardArcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>abcfd7c21143e5ed38573c0dd60826dd8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>abf853acea86e01356f53055f77661770</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsArcValid</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a2097ccda3b1ad27e2c82166979018bda</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>node_capacity</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a463d57480c9563a7a707c5d0928c9946</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>arc_capacity</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a92ffd852b2ab2e5241f9832e71a2de71</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>ReserveNodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a69a71cbb575b13bde9899f5a6a217139</anchor>
      <arglist>(NodeIndexType bound)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>ReserveArcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a9563f7154a759843923dd9bb27b288e7</anchor>
      <arglist>(ArcIndexType bound)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Reserve</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab731ca5c638e7b1b0a2c459c94a90f55</anchor>
      <arglist>(NodeIndexType node_capacity, ArcIndexType arc_capacity)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>FreezeCapacities</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aa3bd051d1e141b09dda17aa9b5f24f69</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GroupForwardArcsByFunctor</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a07214b96597069d781e27b1dd17ef83e</anchor>
      <arglist>(const A &amp;a, B *b)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_end_arc_index</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a346b8a5811f0e287e1ebce2de2c1ad28</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kNilNode</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab1292c82a3f43be3bd57b63a05fe0214</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kNilArc</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ad5b77846f77c2771e840820812ad5521</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>ComputeCumulativeSum</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ac47030fcb75a3220f8cf9ed79697056e</anchor>
      <arglist>(std::vector&lt; ArcIndexType &gt; *v)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>BuildStartAndForwardHead</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a63fd586eed6c345866317e2f0faf377e</anchor>
      <arglist>(SVector&lt; NodeIndexType &gt; *head, std::vector&lt; ArcIndexType &gt; *start, std::vector&lt; ArcIndexType &gt; *permutation)</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>num_nodes_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a473db46afec1eabf0762411830dee30f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>node_capacity_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a424bd34a9767e7edeaf3a60ecd3cb000</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>num_arcs_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>af16f99d41856a7b22ae8a226ef09abff</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>arc_capacity_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a895e891d1ad52ce3efcfeb7ba11194c7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>const_capacities_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aa980e5526b9ded17a83928fc339c71e4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::LinearSumAssignment::BipartiteLeftNodeIterator</name>
    <filename>classoperations__research_1_1LinearSumAssignment_1_1BipartiteLeftNodeIterator.html</filename>
    <member kind="function">
      <type></type>
      <name>BipartiteLeftNodeIterator</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment_1_1BipartiteLeftNodeIterator.html</anchorfile>
      <anchor>afba2834f3d7371e144baf2b197e91531</anchor>
      <arglist>(const GraphType &amp;graph, NodeIndex num_left_nodes)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>BipartiteLeftNodeIterator</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment_1_1BipartiteLeftNodeIterator.html</anchorfile>
      <anchor>aa8fc1a4359689fce936304944920dcd9</anchor>
      <arglist>(const LinearSumAssignment &amp;assignment)</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>Index</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment_1_1BipartiteLeftNodeIterator.html</anchorfile>
      <anchor>a6d8310143b745dc4e1ddb43ccf67b495</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment_1_1BipartiteLeftNodeIterator.html</anchorfile>
      <anchor>aaaf694760ebdabb160b78fb2a21fbd38</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment_1_1BipartiteLeftNodeIterator.html</anchorfile>
      <anchor>ae9da472542deee8fc78fc2ce4bfc2f15</anchor>
      <arglist>()</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::BronKerboschAlgorithm</name>
    <filename>classoperations__research_1_1BronKerboschAlgorithm.html</filename>
    <templarg></templarg>
    <member kind="typedef">
      <type>std::function&lt; bool(NodeIndex, NodeIndex)&gt;</type>
      <name>IsArcCallback</name>
      <anchorfile>classoperations__research_1_1BronKerboschAlgorithm.html</anchorfile>
      <anchor>a2c635d9b0b40012abf93606dd023e535</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>std::function&lt; CliqueResponse(const std::vector&lt; NodeIndex &gt; &amp;)&gt;</type>
      <name>CliqueCallback</name>
      <anchorfile>classoperations__research_1_1BronKerboschAlgorithm.html</anchorfile>
      <anchor>ae49289c12a088e9f15bb26f927406c2f</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>BronKerboschAlgorithm</name>
      <anchorfile>classoperations__research_1_1BronKerboschAlgorithm.html</anchorfile>
      <anchor>ae7ca5a1b7767ea98c2f337e8b6f94c44</anchor>
      <arglist>(IsArcCallback is_arc, NodeIndex num_nodes, CliqueCallback clique_callback)</arglist>
    </member>
    <member kind="function">
      <type>BronKerboschAlgorithmStatus</type>
      <name>Run</name>
      <anchorfile>classoperations__research_1_1BronKerboschAlgorithm.html</anchorfile>
      <anchor>a6dcb077f31531cc17c94da3364b6f099</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>BronKerboschAlgorithmStatus</type>
      <name>RunIterations</name>
      <anchorfile>classoperations__research_1_1BronKerboschAlgorithm.html</anchorfile>
      <anchor>a14b12a4c5ea291009f47c1a98c458f81</anchor>
      <arglist>(int64 max_num_iterations)</arglist>
    </member>
    <member kind="function">
      <type>BronKerboschAlgorithmStatus</type>
      <name>RunWithTimeLimit</name>
      <anchorfile>classoperations__research_1_1BronKerboschAlgorithm.html</anchorfile>
      <anchor>aa28a934535a824fa63672b6c314b61cc</anchor>
      <arglist>(int64 max_num_iterations, TimeLimit *time_limit)</arglist>
    </member>
    <member kind="function">
      <type>BronKerboschAlgorithmStatus</type>
      <name>RunWithTimeLimit</name>
      <anchorfile>classoperations__research_1_1BronKerboschAlgorithm.html</anchorfile>
      <anchor>a4a1ea40849bc5e92cecec1a90f2f2bb9</anchor>
      <arglist>(TimeLimit *time_limit)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::ChristofidesPathSolver</name>
    <filename>classoperations__research_1_1ChristofidesPathSolver.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <templarg></templarg>
    <templarg></templarg>
    <member kind="enumeration">
      <type></type>
      <name>MatchingAlgorithm</name>
      <anchorfile>classoperations__research_1_1ChristofidesPathSolver.html</anchorfile>
      <anchor>ad44822002f75a6f478a62e107d880018</anchor>
      <arglist></arglist>
      <enumvalue file="classoperations__research_1_1ChristofidesPathSolver.html" anchor="ad44822002f75a6f478a62e107d880018a99c5fe202c37dcd8ed9cc60926a4f525">MINIMAL_WEIGHT_MATCHING</enumvalue>
    </member>
    <member kind="function">
      <type></type>
      <name>ChristofidesPathSolver</name>
      <anchorfile>classoperations__research_1_1ChristofidesPathSolver.html</anchorfile>
      <anchor>a4c46f50f1d26cb5d1945fee66db56029</anchor>
      <arglist>(NodeIndex num_nodes, CostFunction costs)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetMatchingAlgorithm</name>
      <anchorfile>classoperations__research_1_1ChristofidesPathSolver.html</anchorfile>
      <anchor>a76c636e1d48da64087686dd06bc45519</anchor>
      <arglist>(MatchingAlgorithm matching)</arglist>
    </member>
    <member kind="function">
      <type>CostType</type>
      <name>TravelingSalesmanCost</name>
      <anchorfile>classoperations__research_1_1ChristofidesPathSolver.html</anchorfile>
      <anchor>ada30f267a0dea4a4f25fcce1a6158438</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; NodeIndex &gt;</type>
      <name>TravelingSalesmanPath</name>
      <anchorfile>classoperations__research_1_1ChristofidesPathSolver.html</anchorfile>
      <anchor>a8db7cbf56d8882ecefcb1f88ff20c755</anchor>
      <arglist>()</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::CompleteBipartiteGraph</name>
    <filename>classutil_1_1CompleteBipartiteGraph.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <base>BaseGraph&lt; NodeIndexType, ArcIndexType, false &gt;</base>
    <class kind="class">util::CompleteBipartiteGraph::OutgoingArcIterator</class>
    <member kind="typedef">
      <type>NodeIndexType</type>
      <name>NodeIndex</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>adb271cf4bcf2de5b5bbe300d7054af29</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ArcIndexType</type>
      <name>ArcIndex</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a0eba6e5899924388644dfa2258ae8929</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>CompleteBipartiteGraph</name>
      <anchorfile>classutil_1_1CompleteBipartiteGraph.html</anchorfile>
      <anchor>a63dffe1efb9b218697c5658752d2f557</anchor>
      <arglist>(NodeIndexType left_nodes, NodeIndexType right_nodes)</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Head</name>
      <anchorfile>classutil_1_1CompleteBipartiteGraph.html</anchorfile>
      <anchor>ad68ad92884b0ed0e3b2eb6888a6eebea</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Tail</name>
      <anchorfile>classutil_1_1CompleteBipartiteGraph.html</anchorfile>
      <anchor>a31e96f6c4453535f09ba76224fd4628a</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>OutDegree</name>
      <anchorfile>classutil_1_1CompleteBipartiteGraph.html</anchorfile>
      <anchor>acf1c65e6f320741ffab9a8717ac4ba28</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; ArcIndexType &gt;</type>
      <name>OutgoingArcs</name>
      <anchorfile>classutil_1_1CompleteBipartiteGraph.html</anchorfile>
      <anchor>a8e4bf50d021c9ed65c6da43b6ed014ae</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; ArcIndexType &gt;</type>
      <name>OutgoingArcsStartingFrom</name>
      <anchorfile>classutil_1_1CompleteBipartiteGraph.html</anchorfile>
      <anchor>a6c20eacdf4ddd2af4c40b1fcce27ada9</anchor>
      <arglist>(NodeIndexType node, ArcIndexType from) const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; NodeIndexType &gt;</type>
      <name>operator[]</name>
      <anchorfile>classutil_1_1CompleteBipartiteGraph.html</anchorfile>
      <anchor>ae2e3523b6d6cdff67cbca55bdc2bcc4b</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>num_nodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a0f551c921fa0b5aaa334a6e36f61db4a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>num_arcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aefb468e4d4a3128c91b3bad9f5b314c8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; NodeIndex &gt;</type>
      <name>AllNodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a32ba4a5ca9a4b89f750eb2dc56518b02</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; ArcIndex &gt;</type>
      <name>AllForwardArcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>abcfd7c21143e5ed38573c0dd60826dd8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>abf853acea86e01356f53055f77661770</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsArcValid</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a2097ccda3b1ad27e2c82166979018bda</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>node_capacity</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a463d57480c9563a7a707c5d0928c9946</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>arc_capacity</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a92ffd852b2ab2e5241f9832e71a2de71</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>ReserveNodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a69a71cbb575b13bde9899f5a6a217139</anchor>
      <arglist>(NodeIndexType bound)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>ReserveArcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a9563f7154a759843923dd9bb27b288e7</anchor>
      <arglist>(ArcIndexType bound)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Reserve</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab731ca5c638e7b1b0a2c459c94a90f55</anchor>
      <arglist>(NodeIndexType node_capacity, ArcIndexType arc_capacity)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>FreezeCapacities</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aa3bd051d1e141b09dda17aa9b5f24f69</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GroupForwardArcsByFunctor</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a07214b96597069d781e27b1dd17ef83e</anchor>
      <arglist>(const A &amp;a, B *b)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_end_arc_index</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a346b8a5811f0e287e1ebce2de2c1ad28</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kNilNode</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab1292c82a3f43be3bd57b63a05fe0214</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kNilArc</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ad5b77846f77c2771e840820812ad5521</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>ComputeCumulativeSum</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ac47030fcb75a3220f8cf9ed79697056e</anchor>
      <arglist>(std::vector&lt; ArcIndexType &gt; *v)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>BuildStartAndForwardHead</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a63fd586eed6c345866317e2f0faf377e</anchor>
      <arglist>(SVector&lt; NodeIndexType &gt; *head, std::vector&lt; ArcIndexType &gt; *start, std::vector&lt; ArcIndexType &gt; *permutation)</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>num_nodes_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a473db46afec1eabf0762411830dee30f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>node_capacity_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a424bd34a9767e7edeaf3a60ecd3cb000</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>num_arcs_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>af16f99d41856a7b22ae8a226ef09abff</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>arc_capacity_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a895e891d1ad52ce3efcfeb7ba11194c7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>const_capacities_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aa980e5526b9ded17a83928fc339c71e4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::CompleteGraph</name>
    <filename>classutil_1_1CompleteGraph.html</filename>
    <templarg>NodeIndexType</templarg>
    <templarg>ArcIndexType</templarg>
    <base>BaseGraph&lt; NodeIndexType, ArcIndexType, false &gt;</base>
    <member kind="typedef">
      <type>NodeIndexType</type>
      <name>NodeIndex</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>adb271cf4bcf2de5b5bbe300d7054af29</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ArcIndexType</type>
      <name>ArcIndex</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a0eba6e5899924388644dfa2258ae8929</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>CompleteGraph</name>
      <anchorfile>classutil_1_1CompleteGraph.html</anchorfile>
      <anchor>ad3401c3d9df9b08cd9be279464029a32</anchor>
      <arglist>(NodeIndexType num_nodes)</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Head</name>
      <anchorfile>classutil_1_1CompleteGraph.html</anchorfile>
      <anchor>af32d9f7de8ae0ccb7a859794a8fa7329</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Tail</name>
      <anchorfile>classutil_1_1CompleteGraph.html</anchorfile>
      <anchor>aa6dc77def1cc80ffd14cb931a3043e52</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>OutDegree</name>
      <anchorfile>classutil_1_1CompleteGraph.html</anchorfile>
      <anchor>a715345c684f1cbeb1896f09ebd989dd1</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; ArcIndexType &gt;</type>
      <name>OutgoingArcs</name>
      <anchorfile>classutil_1_1CompleteGraph.html</anchorfile>
      <anchor>aa5561d57ac19a632ed3086e7f96fdbe6</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; ArcIndexType &gt;</type>
      <name>OutgoingArcsStartingFrom</name>
      <anchorfile>classutil_1_1CompleteGraph.html</anchorfile>
      <anchor>a77b6facc3e744aa4ff764c9b9725e2d8</anchor>
      <arglist>(NodeIndexType node, ArcIndexType from) const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; NodeIndexType &gt;</type>
      <name>operator[]</name>
      <anchorfile>classutil_1_1CompleteGraph.html</anchorfile>
      <anchor>ab12a266c0c091c00a150667619f491d4</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>num_nodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a0f551c921fa0b5aaa334a6e36f61db4a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>num_arcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aefb468e4d4a3128c91b3bad9f5b314c8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; NodeIndex &gt;</type>
      <name>AllNodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a32ba4a5ca9a4b89f750eb2dc56518b02</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; ArcIndex &gt;</type>
      <name>AllForwardArcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>abcfd7c21143e5ed38573c0dd60826dd8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>abf853acea86e01356f53055f77661770</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsArcValid</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a2097ccda3b1ad27e2c82166979018bda</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>node_capacity</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a463d57480c9563a7a707c5d0928c9946</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>arc_capacity</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a92ffd852b2ab2e5241f9832e71a2de71</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>ReserveNodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a69a71cbb575b13bde9899f5a6a217139</anchor>
      <arglist>(NodeIndexType bound)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>ReserveArcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a9563f7154a759843923dd9bb27b288e7</anchor>
      <arglist>(ArcIndexType bound)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Reserve</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab731ca5c638e7b1b0a2c459c94a90f55</anchor>
      <arglist>(NodeIndexType node_capacity, ArcIndexType arc_capacity)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>FreezeCapacities</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aa3bd051d1e141b09dda17aa9b5f24f69</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GroupForwardArcsByFunctor</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a07214b96597069d781e27b1dd17ef83e</anchor>
      <arglist>(const A &amp;a, B *b)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_end_arc_index</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a346b8a5811f0e287e1ebce2de2c1ad28</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kNilNode</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab1292c82a3f43be3bd57b63a05fe0214</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kNilArc</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ad5b77846f77c2771e840820812ad5521</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>ComputeCumulativeSum</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ac47030fcb75a3220f8cf9ed79697056e</anchor>
      <arglist>(std::vector&lt; ArcIndexType &gt; *v)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>BuildStartAndForwardHead</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a63fd586eed6c345866317e2f0faf377e</anchor>
      <arglist>(SVector&lt; NodeIndexType &gt; *head, std::vector&lt; ArcIndexType &gt; *start, std::vector&lt; ArcIndexType &gt; *permutation)</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>num_nodes_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a473db46afec1eabf0762411830dee30f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>node_capacity_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a424bd34a9767e7edeaf3a60ecd3cb000</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>num_arcs_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>af16f99d41856a7b22ae8a226ef09abff</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>arc_capacity_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a895e891d1ad52ce3efcfeb7ba11194c7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>const_capacities_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aa980e5526b9ded17a83928fc339c71e4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::ConnectedComponents</name>
    <filename>classoperations__research_1_1ConnectedComponents.html</filename>
    <templarg>NodeIndex</templarg>
    <templarg>ArcIndex</templarg>
    <member kind="function">
      <type></type>
      <name>ConnectedComponents</name>
      <anchorfile>classoperations__research_1_1ConnectedComponents.html</anchorfile>
      <anchor>afcf39280b3d8dad94c80e2977415acac</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Init</name>
      <anchorfile>classoperations__research_1_1ConnectedComponents.html</anchorfile>
      <anchor>a2d8283743a52c2e331309c33c798b23c</anchor>
      <arglist>(NodeIndex num_nodes)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddArc</name>
      <anchorfile>classoperations__research_1_1ConnectedComponents.html</anchorfile>
      <anchor>a2b3b54f216c61506eaaace2d29819f79</anchor>
      <arglist>(NodeIndex tail, NodeIndex head)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddGraph</name>
      <anchorfile>classoperations__research_1_1ConnectedComponents.html</anchorfile>
      <anchor>a257c3606c853fb7b79f62a5b62c65359</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>CompressPath</name>
      <anchorfile>classoperations__research_1_1ConnectedComponents.html</anchorfile>
      <anchor>a37aff82e0d7503f2dda86e1211cc89af</anchor>
      <arglist>(NodeIndex node)</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>GetClassRepresentative</name>
      <anchorfile>classoperations__research_1_1ConnectedComponents.html</anchorfile>
      <anchor>aa3cdc15d5fad9ed6672bd4e436d987dd</anchor>
      <arglist>(NodeIndex node)</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>GetNumberOfConnectedComponents</name>
      <anchorfile>classoperations__research_1_1ConnectedComponents.html</anchorfile>
      <anchor>abe650442ae4ce0a544245721946abb89</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeClasses</name>
      <anchorfile>classoperations__research_1_1ConnectedComponents.html</anchorfile>
      <anchor>a8de720dd960dc6e2e831bb27c3500466</anchor>
      <arglist>(NodeIndex node1, NodeIndex node2)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>ConnectedComponentsFinder</name>
    <filename>classConnectedComponentsFinder.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <member kind="function">
      <type></type>
      <name>ConnectedComponentsFinder</name>
      <anchorfile>classConnectedComponentsFinder.html</anchorfile>
      <anchor>aa669a113d3ee23c506b97949f740b1cc</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>ConnectedComponentsFinder</name>
      <anchorfile>classConnectedComponentsFinder.html</anchorfile>
      <anchor>a3985503e0bc5adaeba09484e723378c6</anchor>
      <arglist>(const ConnectedComponentsFinder &amp;)=delete</arglist>
    </member>
    <member kind="function">
      <type>ConnectedComponentsFinder &amp;</type>
      <name>operator=</name>
      <anchorfile>classConnectedComponentsFinder.html</anchorfile>
      <anchor>a21da7363443c34ad00c529fae4b3b3e2</anchor>
      <arglist>(const ConnectedComponentsFinder &amp;)=delete</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddNode</name>
      <anchorfile>classConnectedComponentsFinder.html</anchorfile>
      <anchor>a93b2c753675c77bfdbc0c33eb582380b</anchor>
      <arglist>(T node)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddEdge</name>
      <anchorfile>classConnectedComponentsFinder.html</anchorfile>
      <anchor>a77a24beaed190e2f3a0d12662a65b18a</anchor>
      <arglist>(T node1, T node2)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Connected</name>
      <anchorfile>classConnectedComponentsFinder.html</anchorfile>
      <anchor>a67dc34888511ab5b90b7389c90dbc1f8</anchor>
      <arglist>(T node1, T node2)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetSize</name>
      <anchorfile>classConnectedComponentsFinder.html</anchorfile>
      <anchor>a7a173636e3dce11a27f2f7121e01a563</anchor>
      <arglist>(T node)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; std::vector&lt; T &gt; &gt;</type>
      <name>FindConnectedComponents</name>
      <anchorfile>classConnectedComponentsFinder.html</anchorfile>
      <anchor>a9eaa06cffceda90c1d44f550ad3459f3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>FindConnectedComponents</name>
      <anchorfile>classConnectedComponentsFinder.html</anchorfile>
      <anchor>a7f7b2f745e4c87bcaafec8fa295bfa91</anchor>
      <arglist>(std::vector&lt; typename internal::ConnectedComponentsTypeHelper&lt; T, CompareOrHashT &gt;::Set &gt; *components)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetNumberOfComponents</name>
      <anchorfile>classConnectedComponentsFinder.html</anchorfile>
      <anchor>a30350bb98f0e1f7ae47ec1292d253b52</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetNumberOfNodes</name>
      <anchorfile>classConnectedComponentsFinder.html</anchorfile>
      <anchor>a350a0a0790e558f619478ce97c8f2ae7</anchor>
      <arglist>() const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::CostValueCycleHandler</name>
    <filename>classoperations__research_1_1CostValueCycleHandler.html</filename>
    <templarg>ArcIndexType</templarg>
    <member kind="function">
      <type></type>
      <name>CostValueCycleHandler</name>
      <anchorfile>classoperations__research_1_1CostValueCycleHandler.html</anchorfile>
      <anchor>a1b62936a1f89c012e183861069715e58</anchor>
      <arglist>(std::vector&lt; CostValue &gt; *cost)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetTempFromIndex</name>
      <anchorfile>classoperations__research_1_1CostValueCycleHandler.html</anchorfile>
      <anchor>aab4c66b41c942bd6829688d6714aa141</anchor>
      <arglist>(ArcIndexType source) override</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetIndexFromIndex</name>
      <anchorfile>classoperations__research_1_1CostValueCycleHandler.html</anchorfile>
      <anchor>a537baf6f7affea8a66e36dd158827dac</anchor>
      <arglist>(ArcIndexType source, ArcIndexType destination) const override</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetIndexFromTemp</name>
      <anchorfile>classoperations__research_1_1CostValueCycleHandler.html</anchorfile>
      <anchor>a2ccf134a7aed1ea8eaf972a932853c4f</anchor>
      <arglist>(ArcIndexType destination) const override</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>~CostValueCycleHandler</name>
      <anchorfile>classoperations__research_1_1CostValueCycleHandler.html</anchorfile>
      <anchor>a54662c4637c7f7b59b14fb5627e9cdad</anchor>
      <arglist>() override</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::ForwardStaticGraph::CycleHandlerForAnnotatedArcs</name>
    <filename>classoperations__research_1_1ForwardStaticGraph_1_1CycleHandlerForAnnotatedArcs.html</filename>
    <member kind="function">
      <type></type>
      <name>CycleHandlerForAnnotatedArcs</name>
      <anchorfile>classoperations__research_1_1ForwardStaticGraph_1_1CycleHandlerForAnnotatedArcs.html</anchorfile>
      <anchor>a1fbbf60ee265ec17759b7a27d86cf749</anchor>
      <arglist>(PermutationCycleHandler&lt; ArcIndexType &gt; *annotation_handler, NodeIndexType *data)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetTempFromIndex</name>
      <anchorfile>classoperations__research_1_1ForwardStaticGraph_1_1CycleHandlerForAnnotatedArcs.html</anchorfile>
      <anchor>a0e12d2bc93981bad96347e900ee9f536</anchor>
      <arglist>(ArcIndexType source) override</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetIndexFromIndex</name>
      <anchorfile>classoperations__research_1_1ForwardStaticGraph_1_1CycleHandlerForAnnotatedArcs.html</anchorfile>
      <anchor>aecc58d70f5a9416d066ec56e74565aa7</anchor>
      <arglist>(ArcIndexType source, ArcIndexType destination) const override</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetIndexFromTemp</name>
      <anchorfile>classoperations__research_1_1ForwardStaticGraph_1_1CycleHandlerForAnnotatedArcs.html</anchorfile>
      <anchor>a9f8a444244738d0f1379b1fbd08169a5</anchor>
      <arglist>(ArcIndexType destination) const override</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::EbertGraphBase::CycleHandlerForAnnotatedArcs</name>
    <filename>classoperations__research_1_1EbertGraphBase_1_1CycleHandlerForAnnotatedArcs.html</filename>
    <member kind="function">
      <type></type>
      <name>CycleHandlerForAnnotatedArcs</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase_1_1CycleHandlerForAnnotatedArcs.html</anchorfile>
      <anchor>a17c2563a5b10c0744e7fd702f418ce78</anchor>
      <arglist>(PermutationCycleHandler&lt; ArcIndexType &gt; *annotation_handler, DerivedGraph *graph)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetTempFromIndex</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase_1_1CycleHandlerForAnnotatedArcs.html</anchorfile>
      <anchor>a621cffffa2e91040eeefc7dd034b0403</anchor>
      <arglist>(ArcIndexType source) override</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetIndexFromIndex</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase_1_1CycleHandlerForAnnotatedArcs.html</anchorfile>
      <anchor>ab28e8ad8099ea2c4ec297746b45d85fc</anchor>
      <arglist>(ArcIndexType source, ArcIndexType destination) const override</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetIndexFromTemp</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase_1_1CycleHandlerForAnnotatedArcs.html</anchorfile>
      <anchor>a8e71b73cab8df6ee38da5dd53331729a</anchor>
      <arglist>(ArcIndexType destination) const override</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetSeen</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase_1_1CycleHandlerForAnnotatedArcs.html</anchorfile>
      <anchor>a298ac0673adae718024b7d7e31b8f108</anchor>
      <arglist>(ArcIndexType *permutation_element) const override</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Unseen</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase_1_1CycleHandlerForAnnotatedArcs.html</anchorfile>
      <anchor>ad36ba8c0d7c1d9d8ebed31a6feec3d3d</anchor>
      <arglist>(ArcIndexType permutation_element) const override</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>~CycleHandlerForAnnotatedArcs</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase_1_1CycleHandlerForAnnotatedArcs.html</anchorfile>
      <anchor>ad8904ac1fe6697a4009428b18c36f333</anchor>
      <arglist>() override</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>DenseConnectedComponentsFinder</name>
    <filename>classDenseConnectedComponentsFinder.html</filename>
    <member kind="function">
      <type></type>
      <name>DenseConnectedComponentsFinder</name>
      <anchorfile>classDenseConnectedComponentsFinder.html</anchorfile>
      <anchor>a33639c91528c2a525e1591df24ff2cc9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DenseConnectedComponentsFinder</name>
      <anchorfile>classDenseConnectedComponentsFinder.html</anchorfile>
      <anchor>a0026050f607ccbad98a3ec2b7dcae3c8</anchor>
      <arglist>(const DenseConnectedComponentsFinder &amp;)=delete</arglist>
    </member>
    <member kind="function">
      <type>DenseConnectedComponentsFinder &amp;</type>
      <name>operator=</name>
      <anchorfile>classDenseConnectedComponentsFinder.html</anchorfile>
      <anchor>a20f2a5d6bb7631a0dcdd678387a1b61a</anchor>
      <arglist>(const DenseConnectedComponentsFinder &amp;)=delete</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddEdge</name>
      <anchorfile>classDenseConnectedComponentsFinder.html</anchorfile>
      <anchor>aba450cf3f16d9386d043c4354a19a4aa</anchor>
      <arglist>(int node1, int node2)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Connected</name>
      <anchorfile>classDenseConnectedComponentsFinder.html</anchorfile>
      <anchor>a071f52956431e211e14ed7f3894c1d0b</anchor>
      <arglist>(int node1, int node2)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetSize</name>
      <anchorfile>classDenseConnectedComponentsFinder.html</anchorfile>
      <anchor>a0a639b64de57bc14f04f0f238e0092d1</anchor>
      <arglist>(int node)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetNumberOfComponents</name>
      <anchorfile>classDenseConnectedComponentsFinder.html</anchorfile>
      <anchor>a5090a0bdc205d23778d2b8a742f5c5fd</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetNumberOfNodes</name>
      <anchorfile>classDenseConnectedComponentsFinder.html</anchorfile>
      <anchor>a317fb43e59ef01ebe3d0d492b4ab33dd</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetNumberOfNodes</name>
      <anchorfile>classDenseConnectedComponentsFinder.html</anchorfile>
      <anchor>a46780e194c525fa9853fa899d69b4340</anchor>
      <arglist>(int num_nodes)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>FindRoot</name>
      <anchorfile>classDenseConnectedComponentsFinder.html</anchorfile>
      <anchor>afdcff89dd2374ce33c05934d328e64ec</anchor>
      <arglist>(int node)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; int &gt;</type>
      <name>GetComponentIds</name>
      <anchorfile>classDenseConnectedComponentsFinder.html</anchorfile>
      <anchor>aee7f0057f42256a520b5ef32bae58b36</anchor>
      <arglist>()</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::EbertGraph</name>
    <filename>classoperations__research_1_1EbertGraph.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <base>operations_research::EbertGraphBase</base>
    <class kind="class">operations_research::EbertGraph::IncomingArcIterator</class>
    <class kind="class">operations_research::EbertGraph::OutgoingOrOppositeIncomingArcIterator</class>
    <member kind="typedef">
      <type>NodeIndexType</type>
      <name>NodeIndex</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>af1720ff38e249834efb67023e4393fad</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ArcIndexType</type>
      <name>ArcIndex</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>a63a3ceac3b9c14b81d517117914a7010</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>EbertGraph</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>adbd8962e4234a144e016479cf3fc7357</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>EbertGraph</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>a485bd7f0bcfb0aa0b4906f06c895b5fd</anchor>
      <arglist>(NodeIndexType max_num_nodes, ArcIndexType max_num_arcs)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>~EbertGraph</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>a85b7aa85a264fcc445bdbce6befc8c78</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CheckArcBounds</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>a70424e6ef2e1bb8ac923679b32527f41</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CheckArcValidity</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>a93e854b7d8f6448efa30629052c1b835</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Tail</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>a8f4a674ce54ae70d858381ba3622c487</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>DirectArcTail</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>a9a55d0ed76944c652d73c1247db82996</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>DirectArcHead</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>a0f9cd34115afbe06ea269ee5bcc74ac5</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>DirectArc</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>a964f5e0f6ead404b708f2113a5cdd67d</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>ReverseArc</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>ab6c2a8f285309e1325aa1a8b690d8011</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Opposite</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>a02cadbddb2eabfe3a26a3af5f96cc6a5</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsDirect</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>a9d0409b2867beb2739b7ae20c10395e1</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsReverse</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>a6b983fc03d4366ac2a697c45a0ba30fc</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsOutgoingOrOppositeIncoming</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>a1672b2b1d01190e5b9f44a55dd48d2d9</anchor>
      <arglist>(ArcIndexType arc, NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsIncoming</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>aa0c2a078e397860fe425fba54cb5adae</anchor>
      <arglist>(ArcIndexType arc, NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsOutgoing</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>ae34257a5477fbac4642416580f237494</anchor>
      <arglist>(ArcIndexType arc, NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>BuildRepresentation</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>a95976397d32c3be512b959314745d176</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>DebugString</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>af943e2518d92aae9de9e682b2a3dbdd1</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Head</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>aac64d5e43f89177ab46f7ef763bb3c64</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>ad69ccdbdbceba88bf784e2add4964b5b</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Reserve</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a2b9709a996fb28b5572783b40e1067c5</anchor>
      <arglist>(NodeIndexType new_max_num_nodes, ArcIndexType new_max_num_arcs)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>AddArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a96bf1fdcbfa88edabb7843b6142cb3bd</anchor>
      <arglist>(NodeIndexType tail, NodeIndexType head)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GroupForwardArcsByFunctor</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a7adcbbf1af098a96abf7a3397246304d</anchor>
      <arglist>(const ArcIndexTypeStrictWeakOrderingFunctor &amp;compare, PermutationCycleHandler&lt; ArcIndexType &gt; *annotation_handler)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>end_arc_index</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a68a1d993a6371e6fbf6a8ffe5c944a45</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>end_arc_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a68a1d993a6371e6fbf6a8ffe5c944a45</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad69ccdbdbceba88bf784e2add4964b5b</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>num_nodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa8a10d242f7088bfdee282e1246e00b8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>num_arcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ac0ae30f08a54ea67bf295b446157aed3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>end_node_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a03cebcf1fdf356a8217ae2879100d324</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>max_num_nodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a51a5033049b63ee5ce35b531b8474f0a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_num_arcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a8aad937029f57b70efe059b908241685</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>max_end_node_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a6aecaff5cd6eea0a7c93de644b8defc6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_end_arc_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7e2435063a7df2859f5b589852a8d4d3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>LookUpArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad24fe35c2e3798137b42e1bc1c35e587</anchor>
      <arglist>(const NodeIndexType tail, const NodeIndexType head) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Head</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aac64d5e43f89177ab46f7ef763bb3c64</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>NodeDebugString</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a5d2e4d934a8eb290a6298480d48faa52</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>ArcDebugString</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a30a3055f6669fd7739fd7acea16c4571</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kFirstArc</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>a3693e89df768c2f25a31d4b4ee64ee3a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kFirstNode</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>a2c2ac42ee1d86e253e0a85f3f1321824</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kNilArc</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>a7174acc6507b365096d7122ef430b1dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kNilNode</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>a2c3f7aa31326efb573187d321679cba6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kMaxNumArcs</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a043c924d609639e1cc5bea7d4a5a7fd3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kMaxNumNodes</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>aebc1c53cc0d7242f47f0cf971a105e20</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>Initialize</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>aeda4906d548f46b28df2c0577e42b2c8</anchor>
      <arglist>(NodeIndexType max_num_nodes, ArcIndexType max_num_arcs)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>FirstOutgoingOrOppositeIncomingArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a68f767cda734319cc4f28e6a4d56b6d4</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>NextAdjacentArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a8ba11aca6997abd6ed8327ea9ea0e7b7</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>NextOutgoingArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a40da052b528623a6e0f5404547693edf</anchor>
      <arglist>(const NodeIndexType unused_node, const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndexType</type>
      <name>StartNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7cf8fa1e0a3abc8179da6309a86097f5</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>StartArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa58e05a2a30eb0b2b38cc366bba1d1ae</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndexType</type>
      <name>NextNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a8038f289c7e5558d1b4d264677c0f4be</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>NextArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a30864ddfe33af3fe52a64d35bf298503</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>FirstOutgoingArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ae83c0a0706d7c6bf85e08719e818c813</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; ArcIndexType &gt;</type>
      <name>next_adjacent_arc_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a26e62c6c3621ffd6cf953bb8e585a064</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>representation_clean_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a82e436348dd462d71b362963129d4f7f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>EbertGraphBase&lt; NodeIndexType, ArcIndexType, EbertGraph&lt; NodeIndexType, ArcIndexType &gt; &gt;</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>a73dbd9f38775b3bd2727594c935502c3</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>StarGraphBase&lt; NodeIndexType, ArcIndexType, EbertGraph&lt; NodeIndexType, ArcIndexType &gt; &gt;</name>
      <anchorfile>classoperations__research_1_1EbertGraph.html</anchorfile>
      <anchor>a8b785fb3f60a942b9d82c48073f2d03b</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::EbertGraphBase</name>
    <filename>classoperations__research_1_1EbertGraphBase.html</filename>
    <templarg>NodeIndexType</templarg>
    <templarg>ArcIndexType</templarg>
    <templarg>DerivedGraph</templarg>
    <base>operations_research::StarGraphBase</base>
    <class kind="class">operations_research::EbertGraphBase::CycleHandlerForAnnotatedArcs</class>
    <member kind="function">
      <type>bool</type>
      <name>Reserve</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a2b9709a996fb28b5572783b40e1067c5</anchor>
      <arglist>(NodeIndexType new_max_num_nodes, ArcIndexType new_max_num_arcs)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>AddArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a96bf1fdcbfa88edabb7843b6142cb3bd</anchor>
      <arglist>(NodeIndexType tail, NodeIndexType head)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GroupForwardArcsByFunctor</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a7adcbbf1af098a96abf7a3397246304d</anchor>
      <arglist>(const ArcIndexTypeStrictWeakOrderingFunctor &amp;compare, PermutationCycleHandler&lt; ArcIndexType &gt; *annotation_handler)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>end_arc_index</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a68a1d993a6371e6fbf6a8ffe5c944a45</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>ad69ccdbdbceba88bf784e2add4964b5b</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>num_nodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa8a10d242f7088bfdee282e1246e00b8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>num_arcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ac0ae30f08a54ea67bf295b446157aed3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>end_node_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a03cebcf1fdf356a8217ae2879100d324</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>end_arc_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a68a1d993a6371e6fbf6a8ffe5c944a45</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>max_num_nodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a51a5033049b63ee5ce35b531b8474f0a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_num_arcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a8aad937029f57b70efe059b908241685</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>max_end_node_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a6aecaff5cd6eea0a7c93de644b8defc6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_end_arc_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7e2435063a7df2859f5b589852a8d4d3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad69ccdbdbceba88bf784e2add4964b5b</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>LookUpArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad24fe35c2e3798137b42e1bc1c35e587</anchor>
      <arglist>(const NodeIndexType tail, const NodeIndexType head) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Head</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aac64d5e43f89177ab46f7ef763bb3c64</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>NodeDebugString</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a5d2e4d934a8eb290a6298480d48faa52</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>ArcDebugString</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a30a3055f6669fd7739fd7acea16c4571</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kFirstArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a3693e89df768c2f25a31d4b4ee64ee3a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kFirstNode</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a2c2ac42ee1d86e253e0a85f3f1321824</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kMaxNumArcs</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a043c924d609639e1cc5bea7d4a5a7fd3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kMaxNumNodes</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>aebc1c53cc0d7242f47f0cf971a105e20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kNilArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a7174acc6507b365096d7122ef430b1dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kNilNode</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a2c3f7aa31326efb573187d321679cba6</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type></type>
      <name>EbertGraphBase</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>aef662883e2f42f46cf31255c12169c3e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type></type>
      <name>~EbertGraphBase</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>adb189f1f683a66f4fa283204a5db05e3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>Initialize</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>aeda4906d548f46b28df2c0577e42b2c8</anchor>
      <arglist>(NodeIndexType max_num_nodes, ArcIndexType max_num_arcs)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>FirstOutgoingOrOppositeIncomingArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a68f767cda734319cc4f28e6a4d56b6d4</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>NextAdjacentArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a8ba11aca6997abd6ed8327ea9ea0e7b7</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>NextOutgoingArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a40da052b528623a6e0f5404547693edf</anchor>
      <arglist>(const NodeIndexType unused_node, const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndexType</type>
      <name>StartNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7cf8fa1e0a3abc8179da6309a86097f5</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>StartArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa58e05a2a30eb0b2b38cc366bba1d1ae</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndexType</type>
      <name>NextNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a8038f289c7e5558d1b4d264677c0f4be</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>NextArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a30864ddfe33af3fe52a64d35bf298503</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>FirstOutgoingArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ae83c0a0706d7c6bf85e08719e818c813</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; ArcIndexType &gt;</type>
      <name>next_adjacent_arc_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a26e62c6c3621ffd6cf953bb8e585a064</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>representation_clean_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a82e436348dd462d71b362963129d4f7f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; ArcIndexType &gt;</type>
      <name>first_incident_arc_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>af1a0166faa8ccde14511e9fc547febac</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; NodeIndexType &gt;</type>
      <name>head_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a1d032dcb17aa589d9a386fe04499654b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>max_num_arcs_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>aa219b1703090300ed69dc9f5f6f54ded</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>max_num_nodes_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a539adfb6a1003d4270e0f2c8fe6705b9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>num_arcs_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a76da9f41f6215acb4ec4dd8da00c0e28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>num_nodes_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>ad9877f4f6b0822e2cb6aa9c4fe60ceb9</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>StarGraphBase&lt; NodeIndexType, ArcIndexType, DerivedGraph &gt;</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a89aa87fdbc5337e9edcd79499d24fc1d</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>EbertGraphBase&lt; NodeIndexType, ArcIndexType, EbertGraph&lt; NodeIndexType, ArcIndexType &gt; &gt;</name>
    <filename>classoperations__research_1_1EbertGraphBase.html</filename>
    <base>StarGraphBase&lt; NodeIndexType, ArcIndexType, EbertGraph&lt; NodeIndexType, ArcIndexType &gt; &gt;</base>
    <member kind="function">
      <type>bool</type>
      <name>Reserve</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a2b9709a996fb28b5572783b40e1067c5</anchor>
      <arglist>(NodeIndexType new_max_num_nodes, ArcIndexType new_max_num_arcs)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>AddArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a96bf1fdcbfa88edabb7843b6142cb3bd</anchor>
      <arglist>(NodeIndexType tail, NodeIndexType head)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GroupForwardArcsByFunctor</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a7adcbbf1af098a96abf7a3397246304d</anchor>
      <arglist>(const ArcIndexTypeStrictWeakOrderingFunctor &amp;compare, PermutationCycleHandler&lt; ArcIndexType &gt; *annotation_handler)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>end_arc_index</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a68a1d993a6371e6fbf6a8ffe5c944a45</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>ad69ccdbdbceba88bf784e2add4964b5b</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>num_nodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa8a10d242f7088bfdee282e1246e00b8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>num_arcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ac0ae30f08a54ea67bf295b446157aed3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>end_node_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a03cebcf1fdf356a8217ae2879100d324</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>end_arc_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a68a1d993a6371e6fbf6a8ffe5c944a45</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>max_num_nodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a51a5033049b63ee5ce35b531b8474f0a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_num_arcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a8aad937029f57b70efe059b908241685</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>max_end_node_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a6aecaff5cd6eea0a7c93de644b8defc6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_end_arc_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7e2435063a7df2859f5b589852a8d4d3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad69ccdbdbceba88bf784e2add4964b5b</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>LookUpArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad24fe35c2e3798137b42e1bc1c35e587</anchor>
      <arglist>(const NodeIndexType tail, const NodeIndexType head) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Head</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aac64d5e43f89177ab46f7ef763bb3c64</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>NodeDebugString</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a5d2e4d934a8eb290a6298480d48faa52</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>ArcDebugString</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a30a3055f6669fd7739fd7acea16c4571</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kFirstArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a3693e89df768c2f25a31d4b4ee64ee3a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kFirstNode</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a2c2ac42ee1d86e253e0a85f3f1321824</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kMaxNumArcs</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a043c924d609639e1cc5bea7d4a5a7fd3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kMaxNumNodes</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>aebc1c53cc0d7242f47f0cf971a105e20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kNilArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a7174acc6507b365096d7122ef430b1dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kNilNode</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a2c3f7aa31326efb573187d321679cba6</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type></type>
      <name>EbertGraphBase</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>aef662883e2f42f46cf31255c12169c3e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type></type>
      <name>~EbertGraphBase</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>adb189f1f683a66f4fa283204a5db05e3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>Initialize</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>aeda4906d548f46b28df2c0577e42b2c8</anchor>
      <arglist>(NodeIndexType max_num_nodes, ArcIndexType max_num_arcs)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>FirstOutgoingOrOppositeIncomingArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a68f767cda734319cc4f28e6a4d56b6d4</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>NextAdjacentArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a8ba11aca6997abd6ed8327ea9ea0e7b7</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>NextOutgoingArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a40da052b528623a6e0f5404547693edf</anchor>
      <arglist>(const NodeIndexType unused_node, const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndexType</type>
      <name>StartNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7cf8fa1e0a3abc8179da6309a86097f5</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>StartArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa58e05a2a30eb0b2b38cc366bba1d1ae</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndexType</type>
      <name>NextNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a8038f289c7e5558d1b4d264677c0f4be</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>NextArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a30864ddfe33af3fe52a64d35bf298503</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>FirstOutgoingArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ae83c0a0706d7c6bf85e08719e818c813</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; ArcIndexType &gt;</type>
      <name>next_adjacent_arc_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a26e62c6c3621ffd6cf953bb8e585a064</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>representation_clean_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a82e436348dd462d71b362963129d4f7f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; ArcIndexType &gt;</type>
      <name>first_incident_arc_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>af1a0166faa8ccde14511e9fc547febac</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; NodeIndexType &gt;</type>
      <name>head_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a1d032dcb17aa589d9a386fe04499654b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>max_num_arcs_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>aa219b1703090300ed69dc9f5f6f54ded</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>max_num_nodes_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a539adfb6a1003d4270e0f2c8fe6705b9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>num_arcs_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a76da9f41f6215acb4ec4dd8da00c0e28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>num_nodes_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>ad9877f4f6b0822e2cb6aa9c4fe60ceb9</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>StarGraphBase&lt; NodeIndexType, ArcIndexType, DerivedGraph &gt;</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a89aa87fdbc5337e9edcd79499d24fc1d</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>EbertGraphBase&lt; NodeIndexType, ArcIndexType, ForwardEbertGraph&lt; NodeIndexType, ArcIndexType &gt; &gt;</name>
    <filename>classoperations__research_1_1EbertGraphBase.html</filename>
    <base>StarGraphBase&lt; NodeIndexType, ArcIndexType, ForwardEbertGraph&lt; NodeIndexType, ArcIndexType &gt; &gt;</base>
    <member kind="function">
      <type>bool</type>
      <name>Reserve</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a2b9709a996fb28b5572783b40e1067c5</anchor>
      <arglist>(NodeIndexType new_max_num_nodes, ArcIndexType new_max_num_arcs)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>AddArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a96bf1fdcbfa88edabb7843b6142cb3bd</anchor>
      <arglist>(NodeIndexType tail, NodeIndexType head)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GroupForwardArcsByFunctor</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a7adcbbf1af098a96abf7a3397246304d</anchor>
      <arglist>(const ArcIndexTypeStrictWeakOrderingFunctor &amp;compare, PermutationCycleHandler&lt; ArcIndexType &gt; *annotation_handler)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>end_arc_index</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a68a1d993a6371e6fbf6a8ffe5c944a45</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>ad69ccdbdbceba88bf784e2add4964b5b</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>num_nodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa8a10d242f7088bfdee282e1246e00b8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>num_arcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ac0ae30f08a54ea67bf295b446157aed3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>end_node_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a03cebcf1fdf356a8217ae2879100d324</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>end_arc_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a68a1d993a6371e6fbf6a8ffe5c944a45</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>max_num_nodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a51a5033049b63ee5ce35b531b8474f0a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_num_arcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a8aad937029f57b70efe059b908241685</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>max_end_node_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a6aecaff5cd6eea0a7c93de644b8defc6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_end_arc_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7e2435063a7df2859f5b589852a8d4d3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad69ccdbdbceba88bf784e2add4964b5b</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>LookUpArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad24fe35c2e3798137b42e1bc1c35e587</anchor>
      <arglist>(const NodeIndexType tail, const NodeIndexType head) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Head</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aac64d5e43f89177ab46f7ef763bb3c64</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>NodeDebugString</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a5d2e4d934a8eb290a6298480d48faa52</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>ArcDebugString</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a30a3055f6669fd7739fd7acea16c4571</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kFirstArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a3693e89df768c2f25a31d4b4ee64ee3a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kFirstNode</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a2c2ac42ee1d86e253e0a85f3f1321824</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kMaxNumArcs</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a043c924d609639e1cc5bea7d4a5a7fd3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kMaxNumNodes</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>aebc1c53cc0d7242f47f0cf971a105e20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kNilArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a7174acc6507b365096d7122ef430b1dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kNilNode</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a2c3f7aa31326efb573187d321679cba6</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type></type>
      <name>EbertGraphBase</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>aef662883e2f42f46cf31255c12169c3e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type></type>
      <name>~EbertGraphBase</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>adb189f1f683a66f4fa283204a5db05e3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>Initialize</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>aeda4906d548f46b28df2c0577e42b2c8</anchor>
      <arglist>(NodeIndexType max_num_nodes, ArcIndexType max_num_arcs)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>FirstOutgoingOrOppositeIncomingArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a68f767cda734319cc4f28e6a4d56b6d4</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>NextAdjacentArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a8ba11aca6997abd6ed8327ea9ea0e7b7</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>NextOutgoingArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a40da052b528623a6e0f5404547693edf</anchor>
      <arglist>(const NodeIndexType unused_node, const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndexType</type>
      <name>StartNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7cf8fa1e0a3abc8179da6309a86097f5</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>StartArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa58e05a2a30eb0b2b38cc366bba1d1ae</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndexType</type>
      <name>NextNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a8038f289c7e5558d1b4d264677c0f4be</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>NextArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a30864ddfe33af3fe52a64d35bf298503</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>FirstOutgoingArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ae83c0a0706d7c6bf85e08719e818c813</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; ArcIndexType &gt;</type>
      <name>next_adjacent_arc_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a26e62c6c3621ffd6cf953bb8e585a064</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>representation_clean_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a82e436348dd462d71b362963129d4f7f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; ArcIndexType &gt;</type>
      <name>first_incident_arc_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>af1a0166faa8ccde14511e9fc547febac</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; NodeIndexType &gt;</type>
      <name>head_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a1d032dcb17aa589d9a386fe04499654b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>max_num_arcs_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>aa219b1703090300ed69dc9f5f6f54ded</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>max_num_nodes_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a539adfb6a1003d4270e0f2c8fe6705b9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>num_arcs_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a76da9f41f6215acb4ec4dd8da00c0e28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>num_nodes_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>ad9877f4f6b0822e2cb6aa9c4fe60ceb9</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>StarGraphBase&lt; NodeIndexType, ArcIndexType, DerivedGraph &gt;</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a89aa87fdbc5337e9edcd79499d24fc1d</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::ElementIterator</name>
    <filename>classoperations__research_1_1ElementIterator.html</filename>
    <templarg></templarg>
    <member kind="function">
      <type></type>
      <name>ElementIterator</name>
      <anchorfile>classoperations__research_1_1ElementIterator.html</anchorfile>
      <anchor>a6d122c3ffab9e07e631ac9bb37847bbd</anchor>
      <arglist>(Set set)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>operator!=</name>
      <anchorfile>classoperations__research_1_1ElementIterator.html</anchorfile>
      <anchor>a423bc77da7c559bad4f7da6275a1ed3b</anchor>
      <arglist>(const ElementIterator &amp;other) const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>operator *</name>
      <anchorfile>classoperations__research_1_1ElementIterator.html</anchorfile>
      <anchor>a9028f0663e576b3eb972983df3967b6f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const ElementIterator &amp;</type>
      <name>operator++</name>
      <anchorfile>classoperations__research_1_1ElementIterator.html</anchorfile>
      <anchor>aac756fa71bfc5938f266bf1abeb76ffe</anchor>
      <arglist>()</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::ForwardEbertGraph</name>
    <filename>classoperations__research_1_1ForwardEbertGraph.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <base>EbertGraphBase&lt; NodeIndexType, ArcIndexType, ForwardEbertGraph&lt; NodeIndexType, ArcIndexType &gt; &gt;</base>
    <member kind="typedef">
      <type>NodeIndexType</type>
      <name>NodeIndex</name>
      <anchorfile>classoperations__research_1_1ForwardEbertGraph.html</anchorfile>
      <anchor>abe6bf9da3d9867dfb88d65d99ce657ad</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ArcIndexType</type>
      <name>ArcIndex</name>
      <anchorfile>classoperations__research_1_1ForwardEbertGraph.html</anchorfile>
      <anchor>abd24e74c9c077d01704ea898863e35e6</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>ForwardEbertGraph</name>
      <anchorfile>classoperations__research_1_1ForwardEbertGraph.html</anchorfile>
      <anchor>acd1b48d245b0892f4427a90011976e6c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>ForwardEbertGraph</name>
      <anchorfile>classoperations__research_1_1ForwardEbertGraph.html</anchorfile>
      <anchor>a40c24da8c720bc8b18274f53c62b24b9</anchor>
      <arglist>(NodeIndexType max_num_nodes, ArcIndexType max_num_arcs)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>~ForwardEbertGraph</name>
      <anchorfile>classoperations__research_1_1ForwardEbertGraph.html</anchorfile>
      <anchor>a6c8c4982af7ce7f58794956f3267d488</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CheckArcBounds</name>
      <anchorfile>classoperations__research_1_1ForwardEbertGraph.html</anchorfile>
      <anchor>ab457a2818b4a5163977eb7547ff718ae</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CheckArcValidity</name>
      <anchorfile>classoperations__research_1_1ForwardEbertGraph.html</anchorfile>
      <anchor>a2bf3cfd1f85aa0ff86d1094b34a1301e</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CheckTailIndexValidity</name>
      <anchorfile>classoperations__research_1_1ForwardEbertGraph.html</anchorfile>
      <anchor>a519e28b63106845b55ceb18c5011c4d7</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Tail</name>
      <anchorfile>classoperations__research_1_1ForwardEbertGraph.html</anchorfile>
      <anchor>a4d193a58da3c21d1cb21bc6c46d53dfc</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsIncoming</name>
      <anchorfile>classoperations__research_1_1ForwardEbertGraph.html</anchorfile>
      <anchor>a285687129f5108116d7cc1da74e8b83b</anchor>
      <arglist>(ArcIndexType arc, NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>BuildRepresentation</name>
      <anchorfile>classoperations__research_1_1ForwardEbertGraph.html</anchorfile>
      <anchor>a1d533b7e0f3527495348492c87f7b16a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>BuildTailArray</name>
      <anchorfile>classoperations__research_1_1ForwardEbertGraph.html</anchorfile>
      <anchor>a9c2f42b8fc8a879755cb5c0f9a2bbfb8</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ReleaseTailArray</name>
      <anchorfile>classoperations__research_1_1ForwardEbertGraph.html</anchorfile>
      <anchor>abe6ad10691df5e73fce5c0cae0570c96</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>TailArrayComplete</name>
      <anchorfile>classoperations__research_1_1ForwardEbertGraph.html</anchorfile>
      <anchor>aa3d8b56d047ccfa87eb083046fac40c6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>DebugString</name>
      <anchorfile>classoperations__research_1_1ForwardEbertGraph.html</anchorfile>
      <anchor>a10201023ae78b259beadb129e3aa85f0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Reserve</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a2b9709a996fb28b5572783b40e1067c5</anchor>
      <arglist>(NodeIndexType new_max_num_nodes, ArcIndexType new_max_num_arcs)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>AddArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a96bf1fdcbfa88edabb7843b6142cb3bd</anchor>
      <arglist>(NodeIndexType tail, NodeIndexType head)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GroupForwardArcsByFunctor</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a7adcbbf1af098a96abf7a3397246304d</anchor>
      <arglist>(const ArcIndexTypeStrictWeakOrderingFunctor &amp;compare, PermutationCycleHandler&lt; ArcIndexType &gt; *annotation_handler)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>end_arc_index</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a68a1d993a6371e6fbf6a8ffe5c944a45</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>end_arc_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a68a1d993a6371e6fbf6a8ffe5c944a45</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>ad69ccdbdbceba88bf784e2add4964b5b</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad69ccdbdbceba88bf784e2add4964b5b</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>num_nodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa8a10d242f7088bfdee282e1246e00b8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>num_arcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ac0ae30f08a54ea67bf295b446157aed3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>end_node_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a03cebcf1fdf356a8217ae2879100d324</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>max_num_nodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a51a5033049b63ee5ce35b531b8474f0a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_num_arcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a8aad937029f57b70efe059b908241685</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>max_end_node_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a6aecaff5cd6eea0a7c93de644b8defc6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_end_arc_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7e2435063a7df2859f5b589852a8d4d3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>LookUpArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad24fe35c2e3798137b42e1bc1c35e587</anchor>
      <arglist>(const NodeIndexType tail, const NodeIndexType head) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Head</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aac64d5e43f89177ab46f7ef763bb3c64</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>NodeDebugString</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a5d2e4d934a8eb290a6298480d48faa52</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>ArcDebugString</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a30a3055f6669fd7739fd7acea16c4571</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kFirstArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a3693e89df768c2f25a31d4b4ee64ee3a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kFirstNode</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a2c2ac42ee1d86e253e0a85f3f1321824</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kMaxNumArcs</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a043c924d609639e1cc5bea7d4a5a7fd3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kMaxNumNodes</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>aebc1c53cc0d7242f47f0cf971a105e20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kNilArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a7174acc6507b365096d7122ef430b1dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kNilNode</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a2c3f7aa31326efb573187d321679cba6</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>Initialize</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>aeda4906d548f46b28df2c0577e42b2c8</anchor>
      <arglist>(NodeIndexType max_num_nodes, ArcIndexType max_num_arcs)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>FirstOutgoingOrOppositeIncomingArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a68f767cda734319cc4f28e6a4d56b6d4</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>NextAdjacentArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a8ba11aca6997abd6ed8327ea9ea0e7b7</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>NextOutgoingArc</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a40da052b528623a6e0f5404547693edf</anchor>
      <arglist>(const NodeIndexType unused_node, const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndexType</type>
      <name>StartNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7cf8fa1e0a3abc8179da6309a86097f5</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>StartArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa58e05a2a30eb0b2b38cc366bba1d1ae</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndexType</type>
      <name>NextNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a8038f289c7e5558d1b4d264677c0f4be</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>NextArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a30864ddfe33af3fe52a64d35bf298503</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>FirstOutgoingArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ae83c0a0706d7c6bf85e08719e818c813</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; ArcIndexType &gt;</type>
      <name>next_adjacent_arc_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a26e62c6c3621ffd6cf953bb8e585a064</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>representation_clean_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a82e436348dd462d71b362963129d4f7f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; ArcIndexType &gt;</type>
      <name>first_incident_arc_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>af1a0166faa8ccde14511e9fc547febac</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; NodeIndexType &gt;</type>
      <name>head_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a1d032dcb17aa589d9a386fe04499654b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>max_num_arcs_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>aa219b1703090300ed69dc9f5f6f54ded</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>max_num_nodes_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a539adfb6a1003d4270e0f2c8fe6705b9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>num_arcs_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>a76da9f41f6215acb4ec4dd8da00c0e28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>num_nodes_</name>
      <anchorfile>classoperations__research_1_1EbertGraphBase.html</anchorfile>
      <anchor>ad9877f4f6b0822e2cb6aa9c4fe60ceb9</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>EbertGraphBase&lt; NodeIndexType, ArcIndexType, ForwardEbertGraph&lt; NodeIndexType, ArcIndexType &gt; &gt;</name>
      <anchorfile>classoperations__research_1_1ForwardEbertGraph.html</anchorfile>
      <anchor>a8fdd2d0396c7ec0609a4351f716aef5f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>StarGraphBase&lt; NodeIndexType, ArcIndexType, ForwardEbertGraph&lt; NodeIndexType, ArcIndexType &gt; &gt;</name>
      <anchorfile>classoperations__research_1_1ForwardEbertGraph.html</anchorfile>
      <anchor>a693f3f74a8aa4a10dbc5a040fd6c31eb</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::ForwardStaticGraph</name>
    <filename>classoperations__research_1_1ForwardStaticGraph.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <base>StarGraphBase&lt; NodeIndexType, ArcIndexType, ForwardStaticGraph&lt; NodeIndexType, ArcIndexType &gt; &gt;</base>
    <class kind="class">operations_research::ForwardStaticGraph::CycleHandlerForAnnotatedArcs</class>
    <member kind="typedef">
      <type>NodeIndexType</type>
      <name>NodeIndex</name>
      <anchorfile>classoperations__research_1_1ForwardStaticGraph.html</anchorfile>
      <anchor>add943be8b26c7537cf53b9dbfc32fd99</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ArcIndexType</type>
      <name>ArcIndex</name>
      <anchorfile>classoperations__research_1_1ForwardStaticGraph.html</anchorfile>
      <anchor>adb472ecf630f43f292ce28d0de542ac5</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>ForwardStaticGraph</name>
      <anchorfile>classoperations__research_1_1ForwardStaticGraph.html</anchorfile>
      <anchor>a299d30373ebbfcb70fc84ecc66d95b92</anchor>
      <arglist>(const NodeIndexType num_nodes, const ArcIndexType num_arcs, const bool sort_arcs_by_head, std::vector&lt; std::pair&lt; NodeIndexType, NodeIndexType &gt; &gt; *client_input_arcs, operations_research::PermutationCycleHandler&lt; ArcIndexType &gt; *const client_cycle_handler)</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Tail</name>
      <anchorfile>classoperations__research_1_1ForwardStaticGraph.html</anchorfile>
      <anchor>ac2d2363b40be6da86e1dffe794ab9eba</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsIncoming</name>
      <anchorfile>classoperations__research_1_1ForwardStaticGraph.html</anchorfile>
      <anchor>acb00f0b91f228903023d4417a98caf54</anchor>
      <arglist>(ArcIndexType arc, NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CheckArcBounds</name>
      <anchorfile>classoperations__research_1_1ForwardStaticGraph.html</anchorfile>
      <anchor>a325685db5b8c282db5b30a78206edd30</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CheckArcValidity</name>
      <anchorfile>classoperations__research_1_1ForwardStaticGraph.html</anchorfile>
      <anchor>a27d6c9477d9c5474c08286c890aad441</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CheckTailIndexValidity</name>
      <anchorfile>classoperations__research_1_1ForwardStaticGraph.html</anchorfile>
      <anchor>a4b7af21d9283d9ef77c21646e0c999b9</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>NextOutgoingArc</name>
      <anchorfile>classoperations__research_1_1ForwardStaticGraph.html</anchorfile>
      <anchor>ab3af0a7025ae41572475ca9942d1db45</anchor>
      <arglist>(const NodeIndexType node, ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>DebugString</name>
      <anchorfile>classoperations__research_1_1ForwardStaticGraph.html</anchorfile>
      <anchor>a70997849a276cf0a75513d916753161f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>BuildTailArray</name>
      <anchorfile>classoperations__research_1_1ForwardStaticGraph.html</anchorfile>
      <anchor>afff790414356268c9a40c7fa5316eebf</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ReleaseTailArray</name>
      <anchorfile>classoperations__research_1_1ForwardStaticGraph.html</anchorfile>
      <anchor>a541264ae2a3f5c83efae38bc9335372d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>TailArrayComplete</name>
      <anchorfile>classoperations__research_1_1ForwardStaticGraph.html</anchorfile>
      <anchor>a6f09eb68a93e2cb8fd50c90e48868b3c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>num_nodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa8a10d242f7088bfdee282e1246e00b8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>num_arcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ac0ae30f08a54ea67bf295b446157aed3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>end_node_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a03cebcf1fdf356a8217ae2879100d324</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>end_arc_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a68a1d993a6371e6fbf6a8ffe5c944a45</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>max_num_nodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a51a5033049b63ee5ce35b531b8474f0a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_num_arcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a8aad937029f57b70efe059b908241685</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>max_end_node_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a6aecaff5cd6eea0a7c93de644b8defc6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_end_arc_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7e2435063a7df2859f5b589852a8d4d3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad69ccdbdbceba88bf784e2add4964b5b</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>LookUpArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad24fe35c2e3798137b42e1bc1c35e587</anchor>
      <arglist>(const NodeIndexType tail, const NodeIndexType head) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Head</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aac64d5e43f89177ab46f7ef763bb3c64</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>NodeDebugString</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a5d2e4d934a8eb290a6298480d48faa52</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>ArcDebugString</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a30a3055f6669fd7739fd7acea16c4571</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kNilNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a2c3f7aa31326efb573187d321679cba6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kNilArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7174acc6507b365096d7122ef430b1dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kFirstNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a2c2ac42ee1d86e253e0a85f3f1321824</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kFirstArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a3693e89df768c2f25a31d4b4ee64ee3a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kMaxNumNodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aebc1c53cc0d7242f47f0cf971a105e20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kMaxNumArcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a043c924d609639e1cc5bea7d4a5a7fd3</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndexType</type>
      <name>StartNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7cf8fa1e0a3abc8179da6309a86097f5</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>StartArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa58e05a2a30eb0b2b38cc366bba1d1ae</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndexType</type>
      <name>NextNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a8038f289c7e5558d1b4d264677c0f4be</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>NextArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a30864ddfe33af3fe52a64d35bf298503</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>FirstOutgoingArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ae83c0a0706d7c6bf85e08719e818c813</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>max_num_nodes_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a539adfb6a1003d4270e0f2c8fe6705b9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>max_num_arcs_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa219b1703090300ed69dc9f5f6f54ded</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>num_nodes_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad9877f4f6b0822e2cb6aa9c4fe60ceb9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>num_arcs_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a76da9f41f6215acb4ec4dd8da00c0e28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; NodeIndexType &gt;</type>
      <name>head_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a1d032dcb17aa589d9a386fe04499654b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; ArcIndexType &gt;</type>
      <name>first_incident_arc_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>af1a0166faa8ccde14511e9fc547febac</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>StarGraphBase&lt; NodeIndexType, ArcIndexType, ForwardStaticGraph&lt; NodeIndexType, ArcIndexType &gt; &gt;</name>
      <anchorfile>classoperations__research_1_1ForwardStaticGraph.html</anchorfile>
      <anchor>ad350bf8d35134f9fe0208292360cf2a5</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::GenericMaxFlow</name>
    <filename>classoperations__research_1_1GenericMaxFlow.html</filename>
    <templarg>Graph</templarg>
    <base>operations_research::MaxFlowStatusClass</base>
    <member kind="typedef">
      <type>Graph::NodeIndex</type>
      <name>NodeIndex</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>af29cd6d6e7b334690d8ebe90ed0be410</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>Graph::ArcIndex</type>
      <name>ArcIndex</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>abaf68b768e58ec738030f02c288d4ee2</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>Graph::OutgoingArcIterator</type>
      <name>OutgoingArcIterator</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a41c3325e4b03cd0763ccf20aa857f7a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>Graph::OutgoingOrOppositeIncomingArcIterator</type>
      <name>OutgoingOrOppositeIncomingArcIterator</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ab6aa151eb19acac7bcbeced26a82f355</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>Graph::IncomingArcIterator</type>
      <name>IncomingArcIterator</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>abaa264696c6c702be6b4a54c5b7947b8</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ZVector&lt; ArcIndex &gt;</type>
      <name>ArcIndexArray</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a11a3c0e3b614e504cbb174cf0c4a90db</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>NodeIndex</type>
      <name>NodeHeight</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a4bfa3888f90a91610caa02c8498c6f67</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ZVector&lt; NodeHeight &gt;</type>
      <name>NodeHeightArray</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a93990798ae28a85494473072c95072d2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>Status</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfb</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>NOT_SOLVED</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfbac3c19ea88d51e9ddc44a20cc13e4fb74</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba521cd576d678a5c22f21b4a7ec2ff02b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INT_OVERFLOW</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba288fa96697726e05e63cc28b56c57d36</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_INPUT</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba9b4b284d9cef8bc7ea112971c14584df</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_RESULT</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba64972e5527eb00cc4e60ce5b2f898193</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>NOT_SOLVED</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfbac3c19ea88d51e9ddc44a20cc13e4fb74</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba521cd576d678a5c22f21b4a7ec2ff02b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INT_OVERFLOW</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba288fa96697726e05e63cc28b56c57d36</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_INPUT</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba9b4b284d9cef8bc7ea112971c14584df</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_RESULT</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba64972e5527eb00cc4e60ce5b2f898193</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>GenericMaxFlow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ae78f6137700a6942cee3b8b5bd40817b</anchor>
      <arglist>(const Graph *graph, NodeIndex source, NodeIndex sink)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~GenericMaxFlow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a6296328813067e9419925f40981f0867</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const Graph *</type>
      <name>graph</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a8f83ce5b69d2e3d4cbf314ed6b3d01a6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>Status</type>
      <name>status</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a9662fa5cf2007a62968e6c22fb8a4564</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>GetSourceNodeIndex</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aeeb216a2384f75c7a46cf54de35027d2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>GetSinkNodeIndex</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a8c16f1156b2e038ca0d3f8ba96490ab5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetArcCapacity</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a05f2661c573eb445212f4eddd694fc2f</anchor>
      <arglist>(ArcIndex arc, FlowQuantity new_capacity)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetArcFlow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a9a357ac0cc6e451b5b1b81a9abdeb49b</anchor>
      <arglist>(ArcIndex arc, FlowQuantity new_flow)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Solve</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a1da59e63f4d617578a0dc218d7f2f3e9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>GetOptimalFlow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a90f1c23703ab4e69d7e42549ea005464</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>Flow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aef5b9fc304666691405861f4caf35f45</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>Capacity</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aa094beccfd146238de41da6f8a2b2e4a</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GetSourceSideMinCut</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a022da70970a497438cd0304cf1c6efd9</anchor>
      <arglist>(std::vector&lt; NodeIndex &gt; *result)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GetSinkSideMinCut</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ad7892ad5aa8338015f320267fb7f298f</anchor>
      <arglist>(std::vector&lt; NodeIndex &gt; *result)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CheckInputConsistency</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a6a160e49372bec143572964e6b19f444</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CheckResult</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a5ceac4e6ee8eedd556f1cdec11a2b665</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>AugmentingPathExists</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a75c3aeba25a7c62b4e237d2b34594b0e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetUseGlobalUpdate</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a1055514c9f93a7ffeea0ae9e8a6a7f58</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetUseTwoPhaseAlgorithm</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a0497a8b934a2aa9f7307428736f72522</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetCheckInput</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a3aaa8be2e8b64b746e6b46fa6463bbde</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetCheckResult</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ad80ce21a3088b4798da0eae774b0bab6</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ProcessNodeByHeight</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>acc608bd04efcbcc5b72b76795f5d49d8</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>FlowModel</type>
      <name>CreateFlowModel</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a5b1cf7943417c669c41dc29547f8cd00</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>IsAdmissible</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aecd4716fa878055b30386f0f97a0d907</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>IsActive</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a5b535e0a581ca57102d6c495a10bc911</anchor>
      <arglist>(NodeIndex node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>SetCapacityAndClearFlow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a0c56ff7a0c8bac88e8f6e0da3689e838</anchor>
      <arglist>(ArcIndex arc, FlowQuantity capacity)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>CheckRelabelPrecondition</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a9dbc134de4c5e1d424b49f7000c713f4</anchor>
      <arglist>(NodeIndex node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>std::string</type>
      <name>DebugString</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a57cf9ba195eb368ccd5856ef7de4dcca</anchor>
      <arglist>(const std::string &amp;context, ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>InitializeActiveNodeContainer</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a3b47bff3c2733b198d34aeaa0cbcfa19</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndex</type>
      <name>GetAndRemoveFirstActiveNode</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ad411ee050c1c6a25b5b2abc42a2f0491</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>PushActiveNode</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a4cc032f9987eb1ae9eaa8ebb013f671b</anchor>
      <arglist>(const NodeIndex &amp;node)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>IsEmptyActiveNodeContainer</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a3653a3df1b94e4150486b5149d8eaae7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>Refine</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a1061c1ce94ccc0d379390b8542bfaa23</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>RefineWithGlobalUpdate</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a91d754521a7d43dd215e5d6200ec1062</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>Discharge</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a5a2c545458610cc9b1486ae083708574</anchor>
      <arglist>(NodeIndex node)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>InitializePreflow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a20139b839cd5764939afc8df968a2484</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>PushFlowExcessBackToSource</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a05eb488b184996513248b0dffca59600</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>GlobalUpdate</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ad3925f2137b18b1555563ed149ada740</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>SaturateOutgoingArcsFromSource</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a2d2ffca7a04ecd2975025cb34a3898cb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>PushFlow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a1c31c96effb1f91f800895be2339045b</anchor>
      <arglist>(FlowQuantity flow, ArcIndex arc)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>Relabel</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aa70e526f9be229e52bf598d9cd0e7406</anchor>
      <arglist>(NodeIndex node)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndex</type>
      <name>Head</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ad4a6ec3deadedd6c027b8c0fbbdac88d</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndex</type>
      <name>Tail</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ac28ab380716ec139afa31d485bbbd068</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndex</type>
      <name>Opposite</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a15c425face4a4d1697c7ba298a727192</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>IsArcDirect</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aab64a5e41b9e15ab0953a59ef9fd2f49</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>IsArcValid</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ace677adc9ba14e72018a7aa78955df18</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>ComputeReachableNodes</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ade3632560edb912b1758628f327dfe3d</anchor>
      <arglist>(NodeIndex start, std::vector&lt; NodeIndex &gt; *result)</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>const Graph *</type>
      <name>graph_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a27da86de84ba65849c8aebf7aa153f91</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>QuantityArray</type>
      <name>node_excess_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a6881e220a50b6ab95192f7f263b5eee6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeHeightArray</type>
      <name>node_potential_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a65afde65cda262fce8107f3a15d657c4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>QuantityArray</type>
      <name>residual_arc_capacity_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a4c09a4799a59a5e2947b2da44c7d0ba3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexArray</type>
      <name>first_admissible_arc_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a5421b464b61e322c676935f312501af8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>std::vector&lt; NodeIndex &gt;</type>
      <name>active_nodes_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ad52713e66db4d174006e1c3cac7d9d09</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>PriorityQueueWithRestrictedPush&lt; NodeIndex, NodeHeight &gt;</type>
      <name>active_node_by_height_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a7cc8b8045738632185c7c0f82eb9791f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndex</type>
      <name>source_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a4a6b55b37ed5a95debcd86aa40370e28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndex</type>
      <name>sink_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a7f228bb0707a9ebf78dd67ee15746030</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>Status</type>
      <name>status_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a784643a086180b1755f704d652f564a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>std::vector&lt; bool &gt;</type>
      <name>node_in_bfs_queue_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aaf724f9c00dee72b9b2e510e9e88e13e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>std::vector&lt; NodeIndex &gt;</type>
      <name>bfs_queue_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ac0e27ffc0885628dfcc430197434c311</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>use_global_update_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ab3915b57ac3d28863ad3372f1598a494</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>use_two_phase_algorithm_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a323ba66c6b6e63c3c7165081614e2689</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>process_node_by_height_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aa44b5dfa3bad855a7a7b19750b6db748</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>check_input_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a643e76aab33e0ee6c13b0b336af05352</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>check_result_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a92fbc9576b457ceddeb21395181b3273</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>StatsGroup</type>
      <name>stats_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a3aae9d11f361dfd5c98dcca64987cfb9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected" static="yes">
      <type>static const FlowQuantity</type>
      <name>kMaxFlowQuantity</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ac4278804a3c23b3a7f340930a81ff15f</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>GenericMaxFlow&lt; StarGraph &gt;</name>
    <filename>classoperations__research_1_1GenericMaxFlow.html</filename>
    <base>operations_research::MaxFlowStatusClass</base>
    <member kind="typedef">
      <type>StarGraph ::NodeIndex</type>
      <name>NodeIndex</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>af29cd6d6e7b334690d8ebe90ed0be410</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>StarGraph ::ArcIndex</type>
      <name>ArcIndex</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>abaf68b768e58ec738030f02c288d4ee2</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>StarGraph ::OutgoingArcIterator</type>
      <name>OutgoingArcIterator</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a41c3325e4b03cd0763ccf20aa857f7a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>StarGraph ::OutgoingOrOppositeIncomingArcIterator</type>
      <name>OutgoingOrOppositeIncomingArcIterator</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ab6aa151eb19acac7bcbeced26a82f355</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>StarGraph ::IncomingArcIterator</type>
      <name>IncomingArcIterator</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>abaa264696c6c702be6b4a54c5b7947b8</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ZVector&lt; ArcIndex &gt;</type>
      <name>ArcIndexArray</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a11a3c0e3b614e504cbb174cf0c4a90db</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>NodeIndex</type>
      <name>NodeHeight</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a4bfa3888f90a91610caa02c8498c6f67</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ZVector&lt; NodeHeight &gt;</type>
      <name>NodeHeightArray</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a93990798ae28a85494473072c95072d2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>Status</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfb</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>NOT_SOLVED</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfbac3c19ea88d51e9ddc44a20cc13e4fb74</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba521cd576d678a5c22f21b4a7ec2ff02b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INT_OVERFLOW</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba288fa96697726e05e63cc28b56c57d36</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_INPUT</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba9b4b284d9cef8bc7ea112971c14584df</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_RESULT</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba64972e5527eb00cc4e60ce5b2f898193</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>NOT_SOLVED</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfbac3c19ea88d51e9ddc44a20cc13e4fb74</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba521cd576d678a5c22f21b4a7ec2ff02b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INT_OVERFLOW</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba288fa96697726e05e63cc28b56c57d36</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_INPUT</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba9b4b284d9cef8bc7ea112971c14584df</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_RESULT</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba64972e5527eb00cc4e60ce5b2f898193</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>GenericMaxFlow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ae78f6137700a6942cee3b8b5bd40817b</anchor>
      <arglist>(const StarGraph *graph, NodeIndex source, NodeIndex sink)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~GenericMaxFlow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a6296328813067e9419925f40981f0867</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const StarGraph *</type>
      <name>graph</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a8f83ce5b69d2e3d4cbf314ed6b3d01a6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>Status</type>
      <name>status</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a9662fa5cf2007a62968e6c22fb8a4564</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>GetSourceNodeIndex</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aeeb216a2384f75c7a46cf54de35027d2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>GetSinkNodeIndex</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a8c16f1156b2e038ca0d3f8ba96490ab5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetArcCapacity</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a05f2661c573eb445212f4eddd694fc2f</anchor>
      <arglist>(ArcIndex arc, FlowQuantity new_capacity)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetArcFlow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a9a357ac0cc6e451b5b1b81a9abdeb49b</anchor>
      <arglist>(ArcIndex arc, FlowQuantity new_flow)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Solve</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a1da59e63f4d617578a0dc218d7f2f3e9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>GetOptimalFlow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a90f1c23703ab4e69d7e42549ea005464</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>Flow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aef5b9fc304666691405861f4caf35f45</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>Capacity</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aa094beccfd146238de41da6f8a2b2e4a</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GetSourceSideMinCut</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a022da70970a497438cd0304cf1c6efd9</anchor>
      <arglist>(std::vector&lt; NodeIndex &gt; *result)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GetSinkSideMinCut</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ad7892ad5aa8338015f320267fb7f298f</anchor>
      <arglist>(std::vector&lt; NodeIndex &gt; *result)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CheckInputConsistency</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a6a160e49372bec143572964e6b19f444</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CheckResult</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a5ceac4e6ee8eedd556f1cdec11a2b665</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>AugmentingPathExists</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a75c3aeba25a7c62b4e237d2b34594b0e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetUseGlobalUpdate</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a1055514c9f93a7ffeea0ae9e8a6a7f58</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetUseTwoPhaseAlgorithm</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a0497a8b934a2aa9f7307428736f72522</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetCheckInput</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a3aaa8be2e8b64b746e6b46fa6463bbde</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetCheckResult</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ad80ce21a3088b4798da0eae774b0bab6</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ProcessNodeByHeight</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>acc608bd04efcbcc5b72b76795f5d49d8</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>FlowModel</type>
      <name>CreateFlowModel</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a5b1cf7943417c669c41dc29547f8cd00</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>IsAdmissible</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aecd4716fa878055b30386f0f97a0d907</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>IsActive</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a5b535e0a581ca57102d6c495a10bc911</anchor>
      <arglist>(NodeIndex node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>SetCapacityAndClearFlow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a0c56ff7a0c8bac88e8f6e0da3689e838</anchor>
      <arglist>(ArcIndex arc, FlowQuantity capacity)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>CheckRelabelPrecondition</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a9dbc134de4c5e1d424b49f7000c713f4</anchor>
      <arglist>(NodeIndex node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>std::string</type>
      <name>DebugString</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a57cf9ba195eb368ccd5856ef7de4dcca</anchor>
      <arglist>(const std::string &amp;context, ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>InitializeActiveNodeContainer</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a3b47bff3c2733b198d34aeaa0cbcfa19</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndex</type>
      <name>GetAndRemoveFirstActiveNode</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ad411ee050c1c6a25b5b2abc42a2f0491</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>PushActiveNode</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a4cc032f9987eb1ae9eaa8ebb013f671b</anchor>
      <arglist>(const NodeIndex &amp;node)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>IsEmptyActiveNodeContainer</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a3653a3df1b94e4150486b5149d8eaae7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>Refine</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a1061c1ce94ccc0d379390b8542bfaa23</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>RefineWithGlobalUpdate</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a91d754521a7d43dd215e5d6200ec1062</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>Discharge</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a5a2c545458610cc9b1486ae083708574</anchor>
      <arglist>(NodeIndex node)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>InitializePreflow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a20139b839cd5764939afc8df968a2484</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>PushFlowExcessBackToSource</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a05eb488b184996513248b0dffca59600</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>GlobalUpdate</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ad3925f2137b18b1555563ed149ada740</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>SaturateOutgoingArcsFromSource</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a2d2ffca7a04ecd2975025cb34a3898cb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>PushFlow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a1c31c96effb1f91f800895be2339045b</anchor>
      <arglist>(FlowQuantity flow, ArcIndex arc)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>Relabel</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aa70e526f9be229e52bf598d9cd0e7406</anchor>
      <arglist>(NodeIndex node)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndex</type>
      <name>Head</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ad4a6ec3deadedd6c027b8c0fbbdac88d</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndex</type>
      <name>Tail</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ac28ab380716ec139afa31d485bbbd068</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndex</type>
      <name>Opposite</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a15c425face4a4d1697c7ba298a727192</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>IsArcDirect</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aab64a5e41b9e15ab0953a59ef9fd2f49</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>IsArcValid</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ace677adc9ba14e72018a7aa78955df18</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>ComputeReachableNodes</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ade3632560edb912b1758628f327dfe3d</anchor>
      <arglist>(NodeIndex start, std::vector&lt; NodeIndex &gt; *result)</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>const StarGraph *</type>
      <name>graph_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a27da86de84ba65849c8aebf7aa153f91</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>QuantityArray</type>
      <name>node_excess_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a6881e220a50b6ab95192f7f263b5eee6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeHeightArray</type>
      <name>node_potential_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a65afde65cda262fce8107f3a15d657c4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>QuantityArray</type>
      <name>residual_arc_capacity_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a4c09a4799a59a5e2947b2da44c7d0ba3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexArray</type>
      <name>first_admissible_arc_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a5421b464b61e322c676935f312501af8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>std::vector&lt; NodeIndex &gt;</type>
      <name>active_nodes_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ad52713e66db4d174006e1c3cac7d9d09</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>PriorityQueueWithRestrictedPush&lt; NodeIndex, NodeHeight &gt;</type>
      <name>active_node_by_height_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a7cc8b8045738632185c7c0f82eb9791f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndex</type>
      <name>source_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a4a6b55b37ed5a95debcd86aa40370e28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndex</type>
      <name>sink_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a7f228bb0707a9ebf78dd67ee15746030</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>Status</type>
      <name>status_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a784643a086180b1755f704d652f564a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>std::vector&lt; bool &gt;</type>
      <name>node_in_bfs_queue_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aaf724f9c00dee72b9b2e510e9e88e13e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>std::vector&lt; NodeIndex &gt;</type>
      <name>bfs_queue_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ac0e27ffc0885628dfcc430197434c311</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>use_global_update_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ab3915b57ac3d28863ad3372f1598a494</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>use_two_phase_algorithm_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a323ba66c6b6e63c3c7165081614e2689</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>process_node_by_height_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aa44b5dfa3bad855a7a7b19750b6db748</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>check_input_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a643e76aab33e0ee6c13b0b336af05352</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>check_result_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a92fbc9576b457ceddeb21395181b3273</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>StatsGroup</type>
      <name>stats_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a3aae9d11f361dfd5c98dcca64987cfb9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected" static="yes">
      <type>static const FlowQuantity</type>
      <name>kMaxFlowQuantity</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ac4278804a3c23b3a7f340930a81ff15f</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::GenericMinCostFlow</name>
    <filename>classoperations__research_1_1GenericMinCostFlow.html</filename>
    <templarg>Graph</templarg>
    <templarg>ArcFlowType</templarg>
    <templarg>ArcScaledCostType</templarg>
    <base>operations_research::MinCostFlowBase</base>
    <member kind="typedef">
      <type>Graph::NodeIndex</type>
      <name>NodeIndex</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>aaf02e6d654d4be1a50507d9d40dd1e99</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>Graph::ArcIndex</type>
      <name>ArcIndex</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>ad66a0601a4f1c5f98079d333d5a465e8</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>Graph::OutgoingArcIterator</type>
      <name>OutgoingArcIterator</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a3c6c7677fb80955e5c7386849596043e</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>Graph::OutgoingOrOppositeIncomingArcIterator</type>
      <name>OutgoingOrOppositeIncomingArcIterator</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a0aadf64af2dbb2115943c046e3d14622</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ZVector&lt; ArcIndex &gt;</type>
      <name>ArcIndexArray</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a81e1e8d405058ffdd6aa80978345fa83</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>Status</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>NOT_SOLVED</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a44ab1b2f3c5251267bce749661e4e6de</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a7ba4907297a99141ebd2bc890d5a591c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>FEASIBLE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457ab78b8e2734f0e777e79b8e5f93948101</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INFEASIBLE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457af323941c683086f71ce1e7cae7a0a1b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UNBALANCED</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457ad79843de8fcdec97ba2ce9b83d6cf3c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_RESULT</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a030d97dcea9960e3d1f127024b0e2168</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_COST_RANGE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a41523b94852c3cb55c86ce0e291e3719</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>NOT_SOLVED</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a44ab1b2f3c5251267bce749661e4e6de</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a7ba4907297a99141ebd2bc890d5a591c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>FEASIBLE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457ab78b8e2734f0e777e79b8e5f93948101</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INFEASIBLE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457af323941c683086f71ce1e7cae7a0a1b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UNBALANCED</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457ad79843de8fcdec97ba2ce9b83d6cf3c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_RESULT</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a030d97dcea9960e3d1f127024b0e2168</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_COST_RANGE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a41523b94852c3cb55c86ce0e291e3719</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>GenericMinCostFlow</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a17aff4acf62f808db05d5c43f37efda2</anchor>
      <arglist>(const Graph *graph)</arglist>
    </member>
    <member kind="function">
      <type>const Graph *</type>
      <name>graph</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a8ea0e6e91a6965d8de090ef797bb4185</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>Status</type>
      <name>status</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>ace5a6df9ac9993c42cd091f6e9ebbd54</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetNodeSupply</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a7e4301ebf8c5f86143e390494a0a6f4f</anchor>
      <arglist>(NodeIndex node, FlowQuantity supply)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetArcUnitCost</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a0969b64c9993394fb1cc97a2404e12a4</anchor>
      <arglist>(ArcIndex arc, ArcScaledCostType unit_cost)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetArcCapacity</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a35f31d23e3f300f2e36aa46b2e854c00</anchor>
      <arglist>(ArcIndex arc, ArcFlowType new_capacity)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetArcFlow</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a60875aa935bd9594db707bfd97eab98c</anchor>
      <arglist>(ArcIndex arc, ArcFlowType new_flow)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Solve</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a17e2df29d767d03fc9fc42aee71bf5da</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CheckFeasibility</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a01f13e85a3d12e356f26d80210a6755f</anchor>
      <arglist>(std::vector&lt; NodeIndex &gt; *const infeasible_supply_node, std::vector&lt; NodeIndex &gt; *const infeasible_demand_node)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MakeFeasible</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a60da0a24c813a9ee39f96cc36f8dd9ad</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>CostValue</type>
      <name>GetOptimalCost</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a37cb4245bf39c34116d5466d7922f565</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>Flow</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>ac5a5362a674cfb2a7589bcd77c484c17</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>Capacity</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a44c22a9d4609a54cfc7034de3e541ce7</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>CostValue</type>
      <name>UnitCost</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>ae70575b0b8bbc06301778b172752958b</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>Supply</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a67cea170737362bd37bc8c8f8f024555</anchor>
      <arglist>(NodeIndex node) const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>InitialSupply</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a15963b8d1126b858e249c58658934305</anchor>
      <arglist>(NodeIndex node) const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>FeasibleSupply</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>afa58296d5399afd27f8bed235e18309d</anchor>
      <arglist>(NodeIndex node) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetUseUpdatePrices</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a816e24f3a6af98c2b3ae1854f6fc0781</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetCheckFeasibility</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a9fffb489dce981de3233b1702b801eb6</anchor>
      <arglist>(bool value)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>GenericMinCostFlow&lt; StarGraph &gt;</name>
    <filename>classoperations__research_1_1GenericMinCostFlow.html</filename>
    <base>operations_research::MinCostFlowBase</base>
    <member kind="typedef">
      <type>StarGraph ::NodeIndex</type>
      <name>NodeIndex</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>aaf02e6d654d4be1a50507d9d40dd1e99</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>StarGraph ::ArcIndex</type>
      <name>ArcIndex</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>ad66a0601a4f1c5f98079d333d5a465e8</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>StarGraph ::OutgoingArcIterator</type>
      <name>OutgoingArcIterator</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a3c6c7677fb80955e5c7386849596043e</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>StarGraph ::OutgoingOrOppositeIncomingArcIterator</type>
      <name>OutgoingOrOppositeIncomingArcIterator</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a0aadf64af2dbb2115943c046e3d14622</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ZVector&lt; ArcIndex &gt;</type>
      <name>ArcIndexArray</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a81e1e8d405058ffdd6aa80978345fa83</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>Status</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>NOT_SOLVED</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a44ab1b2f3c5251267bce749661e4e6de</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a7ba4907297a99141ebd2bc890d5a591c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>FEASIBLE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457ab78b8e2734f0e777e79b8e5f93948101</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INFEASIBLE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457af323941c683086f71ce1e7cae7a0a1b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UNBALANCED</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457ad79843de8fcdec97ba2ce9b83d6cf3c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_RESULT</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a030d97dcea9960e3d1f127024b0e2168</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_COST_RANGE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a41523b94852c3cb55c86ce0e291e3719</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>NOT_SOLVED</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a44ab1b2f3c5251267bce749661e4e6de</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a7ba4907297a99141ebd2bc890d5a591c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>FEASIBLE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457ab78b8e2734f0e777e79b8e5f93948101</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INFEASIBLE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457af323941c683086f71ce1e7cae7a0a1b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UNBALANCED</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457ad79843de8fcdec97ba2ce9b83d6cf3c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_RESULT</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a030d97dcea9960e3d1f127024b0e2168</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_COST_RANGE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a41523b94852c3cb55c86ce0e291e3719</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>GenericMinCostFlow</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a17aff4acf62f808db05d5c43f37efda2</anchor>
      <arglist>(const StarGraph *graph)</arglist>
    </member>
    <member kind="function">
      <type>const StarGraph *</type>
      <name>graph</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a8ea0e6e91a6965d8de090ef797bb4185</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>Status</type>
      <name>status</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>ace5a6df9ac9993c42cd091f6e9ebbd54</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetNodeSupply</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a7e4301ebf8c5f86143e390494a0a6f4f</anchor>
      <arglist>(NodeIndex node, FlowQuantity supply)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetArcUnitCost</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a0969b64c9993394fb1cc97a2404e12a4</anchor>
      <arglist>(ArcIndex arc, CostValue unit_cost)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetArcCapacity</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a35f31d23e3f300f2e36aa46b2e854c00</anchor>
      <arglist>(ArcIndex arc, FlowQuantity new_capacity)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetArcFlow</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a60875aa935bd9594db707bfd97eab98c</anchor>
      <arglist>(ArcIndex arc, FlowQuantity new_flow)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Solve</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a17e2df29d767d03fc9fc42aee71bf5da</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CheckFeasibility</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a01f13e85a3d12e356f26d80210a6755f</anchor>
      <arglist>(std::vector&lt; NodeIndex &gt; *const infeasible_supply_node, std::vector&lt; NodeIndex &gt; *const infeasible_demand_node)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MakeFeasible</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a60da0a24c813a9ee39f96cc36f8dd9ad</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>CostValue</type>
      <name>GetOptimalCost</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a37cb4245bf39c34116d5466d7922f565</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>Flow</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>ac5a5362a674cfb2a7589bcd77c484c17</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>Capacity</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a44c22a9d4609a54cfc7034de3e541ce7</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>CostValue</type>
      <name>UnitCost</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>ae70575b0b8bbc06301778b172752958b</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>Supply</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a67cea170737362bd37bc8c8f8f024555</anchor>
      <arglist>(NodeIndex node) const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>InitialSupply</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a15963b8d1126b858e249c58658934305</anchor>
      <arglist>(NodeIndex node) const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>FeasibleSupply</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>afa58296d5399afd27f8bed235e18309d</anchor>
      <arglist>(NodeIndex node) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetUseUpdatePrices</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a816e24f3a6af98c2b3ae1854f6fc0781</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetCheckFeasibility</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a9fffb489dce981de3233b1702b801eb6</anchor>
      <arglist>(bool value)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>operations_research::graph_traits</name>
    <filename>structoperations__research_1_1graph__traits.html</filename>
    <templarg></templarg>
    <member kind="variable" static="yes">
      <type>static const bool</type>
      <name>has_reverse_arcs</name>
      <anchorfile>structoperations__research_1_1graph__traits.html</anchorfile>
      <anchor>ad10f7357fd25a9c96d148118612b3256</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const bool</type>
      <name>is_dynamic</name>
      <anchorfile>structoperations__research_1_1graph__traits.html</anchorfile>
      <anchor>ad5cd1b2fa4ab46bf534094f197895989</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>operations_research::graph_traits&lt; ForwardEbertGraph&lt; NodeIndexType, ArcIndexType &gt; &gt;</name>
    <filename>structoperations__research_1_1graph__traits_3_01ForwardEbertGraph_3_01NodeIndexType_00_01ArcIndexType_01_4_01_4.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <member kind="variable" static="yes">
      <type>static const bool</type>
      <name>has_reverse_arcs</name>
      <anchorfile>structoperations__research_1_1graph__traits_3_01ForwardEbertGraph_3_01NodeIndexType_00_01ArcIndexType_01_4_01_4.html</anchorfile>
      <anchor>a64a8352a30e4dd38414813ef444236e6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const bool</type>
      <name>is_dynamic</name>
      <anchorfile>structoperations__research_1_1graph__traits_3_01ForwardEbertGraph_3_01NodeIndexType_00_01ArcIndexType_01_4_01_4.html</anchorfile>
      <anchor>a013b3e9341d6c84014d7dc15ded99396</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>operations_research::graph_traits&lt; ForwardStaticGraph&lt; NodeIndexType, ArcIndexType &gt; &gt;</name>
    <filename>structoperations__research_1_1graph__traits_3_01ForwardStaticGraph_3_01NodeIndexType_00_01ArcIndexType_01_4_01_4.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <member kind="variable" static="yes">
      <type>static const bool</type>
      <name>has_reverse_arcs</name>
      <anchorfile>structoperations__research_1_1graph__traits_3_01ForwardStaticGraph_3_01NodeIndexType_00_01ArcIndexType_01_4_01_4.html</anchorfile>
      <anchor>ad92212c66a65a79b42f0d5b6b2693e69</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const bool</type>
      <name>is_dynamic</name>
      <anchorfile>structoperations__research_1_1graph__traits_3_01ForwardStaticGraph_3_01NodeIndexType_00_01ArcIndexType_01_4_01_4.html</anchorfile>
      <anchor>af0cb76b0565277bd556aa738f83534bf</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>operations_research::Graphs</name>
    <filename>structoperations__research_1_1Graphs.html</filename>
    <templarg></templarg>
    <member kind="typedef">
      <type>Graph::ArcIndex</type>
      <name>ArcIndex</name>
      <anchorfile>structoperations__research_1_1Graphs.html</anchorfile>
      <anchor>ab48fb0d55ba8e5a1f3fae0eb20c29e2d</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>Graph::NodeIndex</type>
      <name>NodeIndex</name>
      <anchorfile>structoperations__research_1_1Graphs.html</anchorfile>
      <anchor>a41cd02c6876d39529d121b0f6102d3ef</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>static ArcIndex</type>
      <name>OppositeArc</name>
      <anchorfile>structoperations__research_1_1Graphs.html</anchorfile>
      <anchor>a9f1fe8a24918ad07513b5707f589e731</anchor>
      <arglist>(const Graph &amp;graph, ArcIndex arc)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>IsArcValid</name>
      <anchorfile>structoperations__research_1_1Graphs.html</anchorfile>
      <anchor>a66b7ad7630645132130da5a258792f4d</anchor>
      <arglist>(const Graph &amp;graph, ArcIndex arc)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static NodeIndex</type>
      <name>NodeReservation</name>
      <anchorfile>structoperations__research_1_1Graphs.html</anchorfile>
      <anchor>ab2ae542c84a1a9aa685f1e6c8e2f5503</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static ArcIndex</type>
      <name>ArcReservation</name>
      <anchorfile>structoperations__research_1_1Graphs.html</anchorfile>
      <anchor>a3e2837ba11c076d455d71ff67c8ddc36</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>Build</name>
      <anchorfile>structoperations__research_1_1Graphs.html</anchorfile>
      <anchor>a1a8c52fdd8fe2db239708cb506d647e5</anchor>
      <arglist>(Graph *graph)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>Build</name>
      <anchorfile>structoperations__research_1_1Graphs.html</anchorfile>
      <anchor>ac47c4735096495d462403431e16f0940</anchor>
      <arglist>(Graph *graph, std::vector&lt; ArcIndex &gt; *permutation)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>operations_research::Graphs&lt; operations_research::StarGraph &gt;</name>
    <filename>structoperations__research_1_1Graphs_3_01operations__research_1_1StarGraph_01_4.html</filename>
    <member kind="typedef">
      <type>operations_research::StarGraph</type>
      <name>Graph</name>
      <anchorfile>structoperations__research_1_1Graphs_3_01operations__research_1_1StarGraph_01_4.html</anchorfile>
      <anchor>ac66e0eebbeabd9a4a3b24cfb734a589b</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>Graph::ArcIndex</type>
      <name>ArcIndex</name>
      <anchorfile>structoperations__research_1_1Graphs_3_01operations__research_1_1StarGraph_01_4.html</anchorfile>
      <anchor>aa6368fe9b8973e136da83e6a13825ef7</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>Graph::NodeIndex</type>
      <name>NodeIndex</name>
      <anchorfile>structoperations__research_1_1Graphs_3_01operations__research_1_1StarGraph_01_4.html</anchorfile>
      <anchor>a77ebbaf314514d539596cf23baf1b20c</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>static ArcIndex</type>
      <name>OppositeArc</name>
      <anchorfile>structoperations__research_1_1Graphs_3_01operations__research_1_1StarGraph_01_4.html</anchorfile>
      <anchor>ad901704a8b3fb67de7a634b831ad0ff7</anchor>
      <arglist>(const Graph &amp;graph, ArcIndex arc)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>IsArcValid</name>
      <anchorfile>structoperations__research_1_1Graphs_3_01operations__research_1_1StarGraph_01_4.html</anchorfile>
      <anchor>af7de014805e897326707e523754b4b4f</anchor>
      <arglist>(const Graph &amp;graph, ArcIndex arc)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static NodeIndex</type>
      <name>NodeReservation</name>
      <anchorfile>structoperations__research_1_1Graphs_3_01operations__research_1_1StarGraph_01_4.html</anchorfile>
      <anchor>adebf187f9e507da38f439cd291864903</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static ArcIndex</type>
      <name>ArcReservation</name>
      <anchorfile>structoperations__research_1_1Graphs_3_01operations__research_1_1StarGraph_01_4.html</anchorfile>
      <anchor>a92594364f4f2937129c921c372742a45</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>Build</name>
      <anchorfile>structoperations__research_1_1Graphs_3_01operations__research_1_1StarGraph_01_4.html</anchorfile>
      <anchor>a1f747b6fd23eaac5feac8cb4f3f3a798</anchor>
      <arglist>(Graph *graph)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>Build</name>
      <anchorfile>structoperations__research_1_1Graphs_3_01operations__research_1_1StarGraph_01_4.html</anchorfile>
      <anchor>ad09c8a0cdfd9a213a8d89883e794e2d1</anchor>
      <arglist>(Graph *graph, std::vector&lt; ArcIndex &gt; *permutation)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::HamiltonianPathSolver</name>
    <filename>classoperations__research_1_1HamiltonianPathSolver.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <member kind="typedef">
      <type>uint32</type>
      <name>Integer</name>
      <anchorfile>classoperations__research_1_1HamiltonianPathSolver.html</anchorfile>
      <anchor>aa5992cec63596c5d6c2ed51fc4f7c9c6</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>Set&lt; Integer &gt;</type>
      <name>NodeSet</name>
      <anchorfile>classoperations__research_1_1HamiltonianPathSolver.html</anchorfile>
      <anchor>afe5925b90aecb674fffc3910b93cc925</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>HamiltonianPathSolver</name>
      <anchorfile>classoperations__research_1_1HamiltonianPathSolver.html</anchorfile>
      <anchor>a1e754bd18247e695674bbb1a3d1187f3</anchor>
      <arglist>(CostFunction cost)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>HamiltonianPathSolver</name>
      <anchorfile>classoperations__research_1_1HamiltonianPathSolver.html</anchorfile>
      <anchor>adcd88e0846cbbfc84ecd93ab3cd5cb77</anchor>
      <arglist>(int num_nodes, CostFunction cost)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ChangeCostMatrix</name>
      <anchorfile>classoperations__research_1_1HamiltonianPathSolver.html</anchorfile>
      <anchor>a553d0693bed2f847684666077241647e</anchor>
      <arglist>(CostFunction cost)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ChangeCostMatrix</name>
      <anchorfile>classoperations__research_1_1HamiltonianPathSolver.html</anchorfile>
      <anchor>a89195d0a67dda5156b3559e82beb5d08</anchor>
      <arglist>(int num_nodes, CostFunction cost)</arglist>
    </member>
    <member kind="function">
      <type>CostType</type>
      <name>HamiltonianCost</name>
      <anchorfile>classoperations__research_1_1HamiltonianPathSolver.html</anchorfile>
      <anchor>a6415d912bcce7f3d61e502d9c0047957</anchor>
      <arglist>(int end_node)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; int &gt;</type>
      <name>HamiltonianPath</name>
      <anchorfile>classoperations__research_1_1HamiltonianPathSolver.html</anchorfile>
      <anchor>ac6c5e2c56d2c4c7cbf18d8d6cd26f1ff</anchor>
      <arglist>(int end_node)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>BestHamiltonianPathEndNode</name>
      <anchorfile>classoperations__research_1_1HamiltonianPathSolver.html</anchorfile>
      <anchor>a6d4e7053b1ca9bb72ca86712ba83ed56</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>HamiltonianPath</name>
      <anchorfile>classoperations__research_1_1HamiltonianPathSolver.html</anchorfile>
      <anchor>ac40b37d8f310c7de3d3d8ba76f9132b8</anchor>
      <arglist>(std::vector&lt; PathNodeIndex &gt; *path)</arglist>
    </member>
    <member kind="function">
      <type>CostType</type>
      <name>TravelingSalesmanCost</name>
      <anchorfile>classoperations__research_1_1HamiltonianPathSolver.html</anchorfile>
      <anchor>a488b92cff7785ca75270e3480f6b3ac2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; int &gt;</type>
      <name>TravelingSalesmanPath</name>
      <anchorfile>classoperations__research_1_1HamiltonianPathSolver.html</anchorfile>
      <anchor>ae52acd5c3aa779486356b63d3c264f8b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>TravelingSalesmanPath</name>
      <anchorfile>classoperations__research_1_1HamiltonianPathSolver.html</anchorfile>
      <anchor>a2ddabfe4e8fcf0c746ad855af62e96b7</anchor>
      <arglist>(std::vector&lt; PathNodeIndex &gt; *path)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsRobust</name>
      <anchorfile>classoperations__research_1_1HamiltonianPathSolver.html</anchorfile>
      <anchor>a2278e850be8943c1bf2ec68e5a81d5ed</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>VerifiesTriangleInequality</name>
      <anchorfile>classoperations__research_1_1HamiltonianPathSolver.html</anchorfile>
      <anchor>af71c1510258301efccfc6e948ea06a82</anchor>
      <arglist>()</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::ReverseArcListGraph::IncomingArcIterator</name>
    <filename>classutil_1_1ReverseArcListGraph_1_1IncomingArcIterator.html</filename>
    <base>util::ReverseArcListGraph::OppositeIncomingArcIterator</base>
    <member kind="function">
      <type></type>
      <name>IncomingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1IncomingArcIterator.html</anchorfile>
      <anchor>aeb78ff088b5309b3573d15bb30178212</anchor>
      <arglist>(const ReverseArcListGraph &amp;graph, NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>IncomingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1IncomingArcIterator.html</anchorfile>
      <anchor>ac346c11ba0e038e8bd19ce53000ed627</anchor>
      <arglist>(const ReverseArcListGraph &amp;graph, NodeIndexType node, ArcIndexType arc)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Index</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1IncomingArcIterator.html</anchorfile>
      <anchor>a70ac2dd4710f5ead124c6bcb47a32f16</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_STL_ITERATOR_FUNCTIONS</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1IncomingArcIterator.html</anchorfile>
      <anchor>a1544d63a165d42595e1e87c553735fb4</anchor>
      <arglist>(IncomingArcIterator)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>ab32e51a40e51e10e2b6bfb8bf63dbc87</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>af79e306f4c5d2c04b46983e8c53fca49</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_STL_ITERATOR_FUNCTIONS</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a50c4f5e91bf22e413ecc7536f918cbec</anchor>
      <arglist>(OppositeIncomingArcIterator)</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>const ReverseArcListGraph &amp;</type>
      <name>graph_</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a6cc6151c5935827350ec7b9574ad3545</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>index_</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>adadde330a68f381bab2c87d8e85faebf</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::EbertGraph::IncomingArcIterator</name>
    <filename>classoperations__research_1_1EbertGraph_1_1IncomingArcIterator.html</filename>
    <member kind="function">
      <type></type>
      <name>IncomingArcIterator</name>
      <anchorfile>classoperations__research_1_1EbertGraph_1_1IncomingArcIterator.html</anchorfile>
      <anchor>aac689ec3cadc1e597847a9d33ff20469</anchor>
      <arglist>(const EbertGraph &amp;graph, NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>IncomingArcIterator</name>
      <anchorfile>classoperations__research_1_1EbertGraph_1_1IncomingArcIterator.html</anchorfile>
      <anchor>a1b679c6f96af2d55e16058cecded849e</anchor>
      <arglist>(const EbertGraph &amp;graph, NodeIndexType node, ArcIndexType arc)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1EbertGraph_1_1IncomingArcIterator.html</anchorfile>
      <anchor>a07606fa87d8f55df7462e36f9d198b59</anchor>
      <arglist>(const IncomingArcIterator &amp;iterator)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classoperations__research_1_1EbertGraph_1_1IncomingArcIterator.html</anchorfile>
      <anchor>a6f95a964dc369fcb3c01844cb7ca0ae3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classoperations__research_1_1EbertGraph_1_1IncomingArcIterator.html</anchorfile>
      <anchor>ad5e8b4e952a5f84530fc9e907320c646</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Index</name>
      <anchorfile>classoperations__research_1_1EbertGraph_1_1IncomingArcIterator.html</anchorfile>
      <anchor>a70eef6592b4132f9efbd78e2b374693e</anchor>
      <arglist>() const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::ReverseArcStaticGraph::IncomingArcIterator</name>
    <filename>classutil_1_1ReverseArcStaticGraph_1_1IncomingArcIterator.html</filename>
    <base>util::ReverseArcStaticGraph::OppositeIncomingArcIterator</base>
    <member kind="function">
      <type></type>
      <name>IncomingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1IncomingArcIterator.html</anchorfile>
      <anchor>a287ddcb43f060b53bceef9434834e9ed</anchor>
      <arglist>(const ReverseArcStaticGraph &amp;graph, NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>IncomingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1IncomingArcIterator.html</anchorfile>
      <anchor>ae7b420ee39369471aeee86c9a6d1818a</anchor>
      <arglist>(const ReverseArcStaticGraph &amp;graph, NodeIndexType node, ArcIndexType arc)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Index</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1IncomingArcIterator.html</anchorfile>
      <anchor>a3aef6ddf066be477b4acea40ef998557</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_STL_ITERATOR_FUNCTIONS</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1IncomingArcIterator.html</anchorfile>
      <anchor>a32c77bb628b82898f954b2e1d326bda3</anchor>
      <arglist>(IncomingArcIterator)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>acc521bdcf77d31c02f46130de62abbd6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>af44b76c2269e81f5dfcc61825fbb1b9f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_STL_ITERATOR_FUNCTIONS</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a63748b2737cc861bded889ff06b03e53</anchor>
      <arglist>(OppositeIncomingArcIterator)</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>const ReverseArcStaticGraph &amp;</type>
      <name>graph_</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a0dd89a6500a8ab3018be5cbe34ad11b2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>const ArcIndexType</type>
      <name>limit_</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a3a1dd084ad6093422cffb58da43de685</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>index_</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a4c6d6e4b71f0237dffe7d4ed2541edf1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::ReverseArcMixedGraph::IncomingArcIterator</name>
    <filename>classutil_1_1ReverseArcMixedGraph_1_1IncomingArcIterator.html</filename>
    <base>util::ReverseArcMixedGraph::OppositeIncomingArcIterator</base>
    <member kind="function">
      <type></type>
      <name>IncomingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1IncomingArcIterator.html</anchorfile>
      <anchor>a50b5c385ecfc9ebc4e13b0bc667d0cff</anchor>
      <arglist>(const ReverseArcMixedGraph &amp;graph, NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>IncomingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1IncomingArcIterator.html</anchorfile>
      <anchor>aba6ef39793d4ac8a059665c6de3c1f0b</anchor>
      <arglist>(const ReverseArcMixedGraph &amp;graph, NodeIndexType node, ArcIndexType arc)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Index</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1IncomingArcIterator.html</anchorfile>
      <anchor>a6946d286256386efdfab71806f2b3122</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_STL_ITERATOR_FUNCTIONS</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1IncomingArcIterator.html</anchorfile>
      <anchor>a12345fdba683c8c79c86e728fd9dda2a</anchor>
      <arglist>(IncomingArcIterator)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>aec415bacb2abe19796f3aadee8c81443</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a8dd74217ea5f69b6e8b9ee60d630ac44</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_STL_ITERATOR_FUNCTIONS</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>aa5166577a05e8f457d906071599c5046</anchor>
      <arglist>(OppositeIncomingArcIterator)</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>const ReverseArcMixedGraph *</type>
      <name>graph_</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a53f8e0b613096680b6c1aa72b04ef02a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>index_</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a3795cf3a06e595f99910b3ff9269c4b7</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>MutableVectorIteration::Iterator</name>
    <filename>structMutableVectorIteration_1_1Iterator.html</filename>
    <member kind="function">
      <type></type>
      <name>Iterator</name>
      <anchorfile>structMutableVectorIteration_1_1Iterator.html</anchorfile>
      <anchor>a89176363ea78bbe3e9ea5c0d1542d0dd</anchor>
      <arglist>(typename std::vector&lt; T &gt;::iterator it)</arglist>
    </member>
    <member kind="function">
      <type>T *</type>
      <name>operator *</name>
      <anchorfile>structMutableVectorIteration_1_1Iterator.html</anchorfile>
      <anchor>a3c0dc6759b61b21af1e6c8e76d5ff6b2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>Iterator &amp;</type>
      <name>operator++</name>
      <anchorfile>structMutableVectorIteration_1_1Iterator.html</anchorfile>
      <anchor>a1e71d8d1b6089d432b3cfcf8a910986b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>operator!=</name>
      <anchorfile>structMutableVectorIteration_1_1Iterator.html</anchorfile>
      <anchor>a9d75c806795d253b1dabbecb84ddcaf6</anchor>
      <arglist>(const Iterator &amp;other) const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::LatticeMemoryManager</name>
    <filename>classoperations__research_1_1LatticeMemoryManager.html</filename>
    <templarg>Set</templarg>
    <templarg>CostType</templarg>
    <member kind="function">
      <type></type>
      <name>LatticeMemoryManager</name>
      <anchorfile>classoperations__research_1_1LatticeMemoryManager.html</anchorfile>
      <anchor>a4f81eab503a836cab0e35f7cffdf9331</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Init</name>
      <anchorfile>classoperations__research_1_1LatticeMemoryManager.html</anchorfile>
      <anchor>ab5f0b45dfc80e6bddba92ea7dc8c018e</anchor>
      <arglist>(int max_card)</arglist>
    </member>
    <member kind="function">
      <type>uint64</type>
      <name>Offset</name>
      <anchorfile>classoperations__research_1_1LatticeMemoryManager.html</anchorfile>
      <anchor>ac5d506873d40f552209caa36e734990d</anchor>
      <arglist>(Set s, int node) const</arglist>
    </member>
    <member kind="function">
      <type>uint64</type>
      <name>BaseOffset</name>
      <anchorfile>classoperations__research_1_1LatticeMemoryManager.html</anchorfile>
      <anchor>a5369cd69622c4264ffcf11a2d5006ecd</anchor>
      <arglist>(int card, Set s) const</arglist>
    </member>
    <member kind="function">
      <type>uint64</type>
      <name>OffsetDelta</name>
      <anchorfile>classoperations__research_1_1LatticeMemoryManager.html</anchorfile>
      <anchor>a143a5937788ddcaa33e732f98cd26b3f</anchor>
      <arglist>(int card, int added_node, int removed_node, int rank) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetValue</name>
      <anchorfile>classoperations__research_1_1LatticeMemoryManager.html</anchorfile>
      <anchor>a59f3668d8fc5dfe2b60d96892c080229</anchor>
      <arglist>(Set s, int node, CostType value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetValueAtOffset</name>
      <anchorfile>classoperations__research_1_1LatticeMemoryManager.html</anchorfile>
      <anchor>a7c8e0f6e664b391264f519adfeb7eae2</anchor>
      <arglist>(uint64 offset, CostType value)</arglist>
    </member>
    <member kind="function">
      <type>CostType</type>
      <name>Value</name>
      <anchorfile>classoperations__research_1_1LatticeMemoryManager.html</anchorfile>
      <anchor>a084e48121cdd1ab05c9d60f8f45114e5</anchor>
      <arglist>(Set s, int node) const</arglist>
    </member>
    <member kind="function">
      <type>CostType</type>
      <name>ValueAtOffset</name>
      <anchorfile>classoperations__research_1_1LatticeMemoryManager.html</anchorfile>
      <anchor>ad91d511dbe7e638fd8a46171f906f7e6</anchor>
      <arglist>(uint64 offset) const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>LatticeMemoryManager&lt; operations_research::Set, CostType &gt;</name>
    <filename>classoperations__research_1_1LatticeMemoryManager.html</filename>
    <member kind="function">
      <type></type>
      <name>LatticeMemoryManager</name>
      <anchorfile>classoperations__research_1_1LatticeMemoryManager.html</anchorfile>
      <anchor>a4f81eab503a836cab0e35f7cffdf9331</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Init</name>
      <anchorfile>classoperations__research_1_1LatticeMemoryManager.html</anchorfile>
      <anchor>ab5f0b45dfc80e6bddba92ea7dc8c018e</anchor>
      <arglist>(int max_card)</arglist>
    </member>
    <member kind="function">
      <type>uint64</type>
      <name>Offset</name>
      <anchorfile>classoperations__research_1_1LatticeMemoryManager.html</anchorfile>
      <anchor>ac5d506873d40f552209caa36e734990d</anchor>
      <arglist>(operations_research::Set s, int node) const</arglist>
    </member>
    <member kind="function">
      <type>uint64</type>
      <name>BaseOffset</name>
      <anchorfile>classoperations__research_1_1LatticeMemoryManager.html</anchorfile>
      <anchor>a5369cd69622c4264ffcf11a2d5006ecd</anchor>
      <arglist>(int card, operations_research::Set s) const</arglist>
    </member>
    <member kind="function">
      <type>uint64</type>
      <name>OffsetDelta</name>
      <anchorfile>classoperations__research_1_1LatticeMemoryManager.html</anchorfile>
      <anchor>a143a5937788ddcaa33e732f98cd26b3f</anchor>
      <arglist>(int card, int added_node, int removed_node, int rank) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetValue</name>
      <anchorfile>classoperations__research_1_1LatticeMemoryManager.html</anchorfile>
      <anchor>a59f3668d8fc5dfe2b60d96892c080229</anchor>
      <arglist>(operations_research::Set s, int node, CostType value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetValueAtOffset</name>
      <anchorfile>classoperations__research_1_1LatticeMemoryManager.html</anchorfile>
      <anchor>a7c8e0f6e664b391264f519adfeb7eae2</anchor>
      <arglist>(uint64 offset, CostType value)</arglist>
    </member>
    <member kind="function">
      <type>CostType</type>
      <name>Value</name>
      <anchorfile>classoperations__research_1_1LatticeMemoryManager.html</anchorfile>
      <anchor>a084e48121cdd1ab05c9d60f8f45114e5</anchor>
      <arglist>(operations_research::Set s, int node) const</arglist>
    </member>
    <member kind="function">
      <type>CostType</type>
      <name>ValueAtOffset</name>
      <anchorfile>classoperations__research_1_1LatticeMemoryManager.html</anchorfile>
      <anchor>ad91d511dbe7e638fd8a46171f906f7e6</anchor>
      <arglist>(uint64 offset) const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::LinearSumAssignment</name>
    <filename>classoperations__research_1_1LinearSumAssignment.html</filename>
    <templarg></templarg>
    <class kind="class">operations_research::LinearSumAssignment::BipartiteLeftNodeIterator</class>
    <member kind="typedef">
      <type>GraphType::NodeIndex</type>
      <name>NodeIndex</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>a1780a52b2a406fbcd615241ab7705824</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>GraphType::ArcIndex</type>
      <name>ArcIndex</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>a0e8e2b115674cbc0c82b83a950839e2d</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>LinearSumAssignment</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>a1eae4227ff347a3d9b66cc705118f092</anchor>
      <arglist>(const GraphType &amp;graph, NodeIndex num_left_nodes)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>LinearSumAssignment</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>aeab56b5cf9d29dd479588f6ae9905df6</anchor>
      <arglist>(NodeIndex num_left_nodes, ArcIndex num_arcs)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>~LinearSumAssignment</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>a92753d803a93847c06ad188d1532b99c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetGraph</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>a505cde11a037301682460196f8c6b93b</anchor>
      <arglist>(const GraphType *graph)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetCostScalingDivisor</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>ab0da9fe270ac009766835e156e88691d</anchor>
      <arglist>(CostValue factor)</arglist>
    </member>
    <member kind="function">
      <type>operations_research::PermutationCycleHandler&lt; typename GraphType::ArcIndex &gt; *</type>
      <name>ArcAnnotationCycleHandler</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>af6bc9d1acdf2224915aa2441e11e0012</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>OptimizeGraphLayout</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>a15cfa33783e509c9ab447858abf6cdab</anchor>
      <arglist>(GraphType *graph)</arglist>
    </member>
    <member kind="function">
      <type>const GraphType &amp;</type>
      <name>Graph</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>ae3b69f5e71ec0fd7d7afa3b8f4362c60</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>Head</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>a0ab1092db058f93c6baf0d4ea44f15a8</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>CostValue</type>
      <name>ArcCost</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>acad00acac7169537c5fa2a2c52ff9d02</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetArcCost</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>a501362f6a94f5df6b53e401f0d179609</anchor>
      <arglist>(ArcIndex arc, CostValue cost)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>FinalizeSetup</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>a43439fdb497f05396053b387ea880ebc</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>ComputeAssignment</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>a5784b0e0c525622091351e1ee4d4e73d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>CostValue</type>
      <name>GetCost</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>a5b4c65750d20e6a8139f56d9ec0b8110</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>NumNodes</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>a16dcc92e05be8b51c348b1b3f0aac5fc</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>NumLeftNodes</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>a0bde059a6043623c8a208dbedb44185d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndex</type>
      <name>GetAssignmentArc</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>ac2682c67a5799b2f76105159621a971a</anchor>
      <arglist>(NodeIndex left_node) const</arglist>
    </member>
    <member kind="function">
      <type>CostValue</type>
      <name>GetAssignmentCost</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>aba97b69de699d2b370cf10430f6ea49e</anchor>
      <arglist>(NodeIndex node) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>GetMate</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>ab369ec930d05edbccaef63529b852ffc</anchor>
      <arglist>(NodeIndex left_node) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>StatsString</name>
      <anchorfile>classoperations__research_1_1LinearSumAssignment.html</anchorfile>
      <anchor>a2a115d652d12c1cb1f061afa6b0a2439</anchor>
      <arglist>() const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::ListGraph</name>
    <filename>classutil_1_1ListGraph.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <base>BaseGraph&lt; NodeIndexType, ArcIndexType, false &gt;</base>
    <class kind="class">util::ListGraph::OutgoingArcIterator</class>
    <class kind="class">util::ListGraph::OutgoingHeadIterator</class>
    <member kind="typedef">
      <type>NodeIndexType</type>
      <name>NodeIndex</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>adb271cf4bcf2de5b5bbe300d7054af29</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ArcIndexType</type>
      <name>ArcIndex</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a0eba6e5899924388644dfa2258ae8929</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>ListGraph</name>
      <anchorfile>classutil_1_1ListGraph.html</anchorfile>
      <anchor>a4b5be895dae4a52e7bd97a7ef47267a6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>ListGraph</name>
      <anchorfile>classutil_1_1ListGraph.html</anchorfile>
      <anchor>a184d6ce1a3d943810039ac9b97b860ef</anchor>
      <arglist>(NodeIndexType num_nodes, ArcIndexType arc_capacity)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddNode</name>
      <anchorfile>classutil_1_1ListGraph.html</anchorfile>
      <anchor>a6af38e88610400b54e8f28ab6f880908</anchor>
      <arglist>(NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>AddArc</name>
      <anchorfile>classutil_1_1ListGraph.html</anchorfile>
      <anchor>ac1aa8c7591bb033a49bab79c21c9f496</anchor>
      <arglist>(NodeIndexType tail, NodeIndexType head)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Build</name>
      <anchorfile>classutil_1_1ListGraph.html</anchorfile>
      <anchor>a777ff917f03a31da77f275d536578afa</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Build</name>
      <anchorfile>classutil_1_1ListGraph.html</anchorfile>
      <anchor>a55c0cd4ab76f4501d0a2b9bbdb308d00</anchor>
      <arglist>(std::vector&lt; ArcIndexType &gt; *permutation)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>OutDegree</name>
      <anchorfile>classutil_1_1ListGraph.html</anchorfile>
      <anchor>ac52db792129da62b7ab25372ab90647c</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OutgoingArcIterator &gt;</type>
      <name>OutgoingArcs</name>
      <anchorfile>classutil_1_1ListGraph.html</anchorfile>
      <anchor>ab63770fa993b9347586d2852b841bdc2</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OutgoingArcIterator &gt;</type>
      <name>OutgoingArcsStartingFrom</name>
      <anchorfile>classutil_1_1ListGraph.html</anchorfile>
      <anchor>ac52a095c1ebbd1ca89902168350bbb75</anchor>
      <arglist>(NodeIndexType node, ArcIndexType from) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OutgoingHeadIterator &gt;</type>
      <name>operator[]</name>
      <anchorfile>classutil_1_1ListGraph.html</anchorfile>
      <anchor>a1565480b94bf179067e81d762916ed52</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Tail</name>
      <anchorfile>classutil_1_1ListGraph.html</anchorfile>
      <anchor>aba03f198f690155bca2ab23b039cea54</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Head</name>
      <anchorfile>classutil_1_1ListGraph.html</anchorfile>
      <anchor>ad7d3d076d3c154d89f1eda855a34b487</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ReserveNodes</name>
      <anchorfile>classutil_1_1ListGraph.html</anchorfile>
      <anchor>aefd885a0eba3ec33568137533190ba5b</anchor>
      <arglist>(NodeIndexType bound) override</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ReserveArcs</name>
      <anchorfile>classutil_1_1ListGraph.html</anchorfile>
      <anchor>af637e16c3c7604f0354a6fa317358a15</anchor>
      <arglist>(ArcIndexType bound) override</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>num_nodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a0f551c921fa0b5aaa334a6e36f61db4a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>num_arcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aefb468e4d4a3128c91b3bad9f5b314c8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; NodeIndex &gt;</type>
      <name>AllNodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a32ba4a5ca9a4b89f750eb2dc56518b02</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; ArcIndex &gt;</type>
      <name>AllForwardArcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>abcfd7c21143e5ed38573c0dd60826dd8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>abf853acea86e01356f53055f77661770</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsArcValid</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a2097ccda3b1ad27e2c82166979018bda</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>node_capacity</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a463d57480c9563a7a707c5d0928c9946</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>arc_capacity</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a92ffd852b2ab2e5241f9832e71a2de71</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Reserve</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab731ca5c638e7b1b0a2c459c94a90f55</anchor>
      <arglist>(NodeIndexType node_capacity, ArcIndexType arc_capacity)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>FreezeCapacities</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aa3bd051d1e141b09dda17aa9b5f24f69</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GroupForwardArcsByFunctor</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a07214b96597069d781e27b1dd17ef83e</anchor>
      <arglist>(const A &amp;a, B *b)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_end_arc_index</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a346b8a5811f0e287e1ebce2de2c1ad28</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kNilNode</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab1292c82a3f43be3bd57b63a05fe0214</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kNilArc</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ad5b77846f77c2771e840820812ad5521</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>ComputeCumulativeSum</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ac47030fcb75a3220f8cf9ed79697056e</anchor>
      <arglist>(std::vector&lt; ArcIndexType &gt; *v)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>BuildStartAndForwardHead</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a63fd586eed6c345866317e2f0faf377e</anchor>
      <arglist>(SVector&lt; NodeIndexType &gt; *head, std::vector&lt; ArcIndexType &gt; *start, std::vector&lt; ArcIndexType &gt; *permutation)</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>num_nodes_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a473db46afec1eabf0762411830dee30f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>node_capacity_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a424bd34a9767e7edeaf3a60ecd3cb000</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>num_arcs_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>af16f99d41856a7b22ae8a226ef09abff</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>arc_capacity_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a895e891d1ad52ce3efcfeb7ba11194c7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>const_capacities_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aa980e5526b9ded17a83928fc339c71e4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::MaxFlow</name>
    <filename>classoperations__research_1_1MaxFlow.html</filename>
    <base>GenericMaxFlow&lt; StarGraph &gt;</base>
    <member kind="typedef">
      <type>StarGraph ::NodeIndex</type>
      <name>NodeIndex</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>af29cd6d6e7b334690d8ebe90ed0be410</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>StarGraph ::ArcIndex</type>
      <name>ArcIndex</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>abaf68b768e58ec738030f02c288d4ee2</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>StarGraph ::OutgoingArcIterator</type>
      <name>OutgoingArcIterator</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a41c3325e4b03cd0763ccf20aa857f7a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>StarGraph ::OutgoingOrOppositeIncomingArcIterator</type>
      <name>OutgoingOrOppositeIncomingArcIterator</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ab6aa151eb19acac7bcbeced26a82f355</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>StarGraph ::IncomingArcIterator</type>
      <name>IncomingArcIterator</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>abaa264696c6c702be6b4a54c5b7947b8</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ZVector&lt; ArcIndex &gt;</type>
      <name>ArcIndexArray</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a11a3c0e3b614e504cbb174cf0c4a90db</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>NodeIndex</type>
      <name>NodeHeight</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a4bfa3888f90a91610caa02c8498c6f67</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ZVector&lt; NodeHeight &gt;</type>
      <name>NodeHeightArray</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a93990798ae28a85494473072c95072d2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>Status</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfb</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>NOT_SOLVED</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfbac3c19ea88d51e9ddc44a20cc13e4fb74</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba521cd576d678a5c22f21b4a7ec2ff02b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INT_OVERFLOW</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba288fa96697726e05e63cc28b56c57d36</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_INPUT</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba9b4b284d9cef8bc7ea112971c14584df</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_RESULT</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba64972e5527eb00cc4e60ce5b2f898193</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>NOT_SOLVED</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfbac3c19ea88d51e9ddc44a20cc13e4fb74</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba521cd576d678a5c22f21b4a7ec2ff02b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INT_OVERFLOW</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba288fa96697726e05e63cc28b56c57d36</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_INPUT</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba9b4b284d9cef8bc7ea112971c14584df</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_RESULT</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba64972e5527eb00cc4e60ce5b2f898193</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MaxFlow</name>
      <anchorfile>classoperations__research_1_1MaxFlow.html</anchorfile>
      <anchor>a13a87ef36714bce2a634ff7b45b11e12</anchor>
      <arglist>(const StarGraph *graph, NodeIndex source, NodeIndex target)</arglist>
    </member>
    <member kind="function">
      <type>const StarGraph *</type>
      <name>graph</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a8f83ce5b69d2e3d4cbf314ed6b3d01a6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>Status</type>
      <name>status</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a9662fa5cf2007a62968e6c22fb8a4564</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>GetSourceNodeIndex</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aeeb216a2384f75c7a46cf54de35027d2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>GetSinkNodeIndex</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a8c16f1156b2e038ca0d3f8ba96490ab5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetArcCapacity</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a05f2661c573eb445212f4eddd694fc2f</anchor>
      <arglist>(ArcIndex arc, FlowQuantity new_capacity)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetArcFlow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a9a357ac0cc6e451b5b1b81a9abdeb49b</anchor>
      <arglist>(ArcIndex arc, FlowQuantity new_flow)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Solve</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a1da59e63f4d617578a0dc218d7f2f3e9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>GetOptimalFlow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a90f1c23703ab4e69d7e42549ea005464</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>Flow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aef5b9fc304666691405861f4caf35f45</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>Capacity</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aa094beccfd146238de41da6f8a2b2e4a</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GetSourceSideMinCut</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a022da70970a497438cd0304cf1c6efd9</anchor>
      <arglist>(std::vector&lt; NodeIndex &gt; *result)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GetSinkSideMinCut</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ad7892ad5aa8338015f320267fb7f298f</anchor>
      <arglist>(std::vector&lt; NodeIndex &gt; *result)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CheckInputConsistency</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a6a160e49372bec143572964e6b19f444</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CheckResult</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a5ceac4e6ee8eedd556f1cdec11a2b665</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>AugmentingPathExists</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a75c3aeba25a7c62b4e237d2b34594b0e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetUseGlobalUpdate</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a1055514c9f93a7ffeea0ae9e8a6a7f58</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetUseTwoPhaseAlgorithm</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a0497a8b934a2aa9f7307428736f72522</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetCheckInput</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a3aaa8be2e8b64b746e6b46fa6463bbde</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetCheckResult</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ad80ce21a3088b4798da0eae774b0bab6</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ProcessNodeByHeight</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>acc608bd04efcbcc5b72b76795f5d49d8</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>FlowModel</type>
      <name>CreateFlowModel</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a5b1cf7943417c669c41dc29547f8cd00</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>IsAdmissible</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aecd4716fa878055b30386f0f97a0d907</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>IsActive</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a5b535e0a581ca57102d6c495a10bc911</anchor>
      <arglist>(NodeIndex node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>SetCapacityAndClearFlow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a0c56ff7a0c8bac88e8f6e0da3689e838</anchor>
      <arglist>(ArcIndex arc, FlowQuantity capacity)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>CheckRelabelPrecondition</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a9dbc134de4c5e1d424b49f7000c713f4</anchor>
      <arglist>(NodeIndex node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>std::string</type>
      <name>DebugString</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a57cf9ba195eb368ccd5856ef7de4dcca</anchor>
      <arglist>(const std::string &amp;context, ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>InitializeActiveNodeContainer</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a3b47bff3c2733b198d34aeaa0cbcfa19</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndex</type>
      <name>GetAndRemoveFirstActiveNode</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ad411ee050c1c6a25b5b2abc42a2f0491</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>PushActiveNode</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a4cc032f9987eb1ae9eaa8ebb013f671b</anchor>
      <arglist>(const NodeIndex &amp;node)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>IsEmptyActiveNodeContainer</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a3653a3df1b94e4150486b5149d8eaae7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>Refine</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a1061c1ce94ccc0d379390b8542bfaa23</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>RefineWithGlobalUpdate</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a91d754521a7d43dd215e5d6200ec1062</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>Discharge</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a5a2c545458610cc9b1486ae083708574</anchor>
      <arglist>(NodeIndex node)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>InitializePreflow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a20139b839cd5764939afc8df968a2484</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>PushFlowExcessBackToSource</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a05eb488b184996513248b0dffca59600</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>GlobalUpdate</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ad3925f2137b18b1555563ed149ada740</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>SaturateOutgoingArcsFromSource</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a2d2ffca7a04ecd2975025cb34a3898cb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>PushFlow</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a1c31c96effb1f91f800895be2339045b</anchor>
      <arglist>(FlowQuantity flow, ArcIndex arc)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>Relabel</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aa70e526f9be229e52bf598d9cd0e7406</anchor>
      <arglist>(NodeIndex node)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndex</type>
      <name>Head</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ad4a6ec3deadedd6c027b8c0fbbdac88d</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndex</type>
      <name>Tail</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ac28ab380716ec139afa31d485bbbd068</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndex</type>
      <name>Opposite</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a15c425face4a4d1697c7ba298a727192</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>IsArcDirect</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aab64a5e41b9e15ab0953a59ef9fd2f49</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>IsArcValid</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ace677adc9ba14e72018a7aa78955df18</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>ComputeReachableNodes</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ade3632560edb912b1758628f327dfe3d</anchor>
      <arglist>(NodeIndex start, std::vector&lt; NodeIndex &gt; *result)</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>const StarGraph *</type>
      <name>graph_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a27da86de84ba65849c8aebf7aa153f91</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>QuantityArray</type>
      <name>node_excess_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a6881e220a50b6ab95192f7f263b5eee6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeHeightArray</type>
      <name>node_potential_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a65afde65cda262fce8107f3a15d657c4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>QuantityArray</type>
      <name>residual_arc_capacity_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a4c09a4799a59a5e2947b2da44c7d0ba3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexArray</type>
      <name>first_admissible_arc_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a5421b464b61e322c676935f312501af8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>std::vector&lt; NodeIndex &gt;</type>
      <name>active_nodes_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ad52713e66db4d174006e1c3cac7d9d09</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>PriorityQueueWithRestrictedPush&lt; NodeIndex, NodeHeight &gt;</type>
      <name>active_node_by_height_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a7cc8b8045738632185c7c0f82eb9791f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndex</type>
      <name>source_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a4a6b55b37ed5a95debcd86aa40370e28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndex</type>
      <name>sink_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a7f228bb0707a9ebf78dd67ee15746030</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>Status</type>
      <name>status_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a784643a086180b1755f704d652f564a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>std::vector&lt; bool &gt;</type>
      <name>node_in_bfs_queue_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aaf724f9c00dee72b9b2e510e9e88e13e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>std::vector&lt; NodeIndex &gt;</type>
      <name>bfs_queue_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ac0e27ffc0885628dfcc430197434c311</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>use_global_update_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ab3915b57ac3d28863ad3372f1598a494</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>use_two_phase_algorithm_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a323ba66c6b6e63c3c7165081614e2689</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>process_node_by_height_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>aa44b5dfa3bad855a7a7b19750b6db748</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>check_input_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a643e76aab33e0ee6c13b0b336af05352</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>check_result_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a92fbc9576b457ceddeb21395181b3273</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>StatsGroup</type>
      <name>stats_</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>a3aae9d11f361dfd5c98dcca64987cfb9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected" static="yes">
      <type>static const FlowQuantity</type>
      <name>kMaxFlowQuantity</name>
      <anchorfile>classoperations__research_1_1GenericMaxFlow.html</anchorfile>
      <anchor>ac4278804a3c23b3a7f340930a81ff15f</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::MaxFlowStatusClass</name>
    <filename>classoperations__research_1_1MaxFlowStatusClass.html</filename>
    <member kind="enumeration">
      <type></type>
      <name>Status</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfb</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>NOT_SOLVED</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfbac3c19ea88d51e9ddc44a20cc13e4fb74</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba521cd576d678a5c22f21b4a7ec2ff02b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INT_OVERFLOW</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba288fa96697726e05e63cc28b56c57d36</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_INPUT</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba9b4b284d9cef8bc7ea112971c14584df</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_RESULT</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba64972e5527eb00cc4e60ce5b2f898193</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>NOT_SOLVED</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfbac3c19ea88d51e9ddc44a20cc13e4fb74</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba521cd576d678a5c22f21b4a7ec2ff02b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INT_OVERFLOW</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba288fa96697726e05e63cc28b56c57d36</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_INPUT</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba9b4b284d9cef8bc7ea112971c14584df</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_RESULT</name>
      <anchorfile>classoperations__research_1_1MaxFlowStatusClass.html</anchorfile>
      <anchor>aa3fbead787cfdfac0b9e7b217e06cbfba64972e5527eb00cc4e60ce5b2f898193</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::MinCostFlow</name>
    <filename>classoperations__research_1_1MinCostFlow.html</filename>
    <base>GenericMinCostFlow&lt; StarGraph &gt;</base>
    <member kind="typedef">
      <type>StarGraph ::NodeIndex</type>
      <name>NodeIndex</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>aaf02e6d654d4be1a50507d9d40dd1e99</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>StarGraph ::ArcIndex</type>
      <name>ArcIndex</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>ad66a0601a4f1c5f98079d333d5a465e8</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>StarGraph ::OutgoingArcIterator</type>
      <name>OutgoingArcIterator</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a3c6c7677fb80955e5c7386849596043e</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>StarGraph ::OutgoingOrOppositeIncomingArcIterator</type>
      <name>OutgoingOrOppositeIncomingArcIterator</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a0aadf64af2dbb2115943c046e3d14622</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ZVector&lt; ArcIndex &gt;</type>
      <name>ArcIndexArray</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a81e1e8d405058ffdd6aa80978345fa83</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>Status</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>NOT_SOLVED</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a44ab1b2f3c5251267bce749661e4e6de</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a7ba4907297a99141ebd2bc890d5a591c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>FEASIBLE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457ab78b8e2734f0e777e79b8e5f93948101</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INFEASIBLE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457af323941c683086f71ce1e7cae7a0a1b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UNBALANCED</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457ad79843de8fcdec97ba2ce9b83d6cf3c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_RESULT</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a030d97dcea9960e3d1f127024b0e2168</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_COST_RANGE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a41523b94852c3cb55c86ce0e291e3719</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>NOT_SOLVED</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a44ab1b2f3c5251267bce749661e4e6de</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a7ba4907297a99141ebd2bc890d5a591c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>FEASIBLE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457ab78b8e2734f0e777e79b8e5f93948101</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INFEASIBLE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457af323941c683086f71ce1e7cae7a0a1b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UNBALANCED</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457ad79843de8fcdec97ba2ce9b83d6cf3c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_RESULT</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a030d97dcea9960e3d1f127024b0e2168</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_COST_RANGE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a41523b94852c3cb55c86ce0e291e3719</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MinCostFlow</name>
      <anchorfile>classoperations__research_1_1MinCostFlow.html</anchorfile>
      <anchor>a53a9c622cd9bc61deece5070c7533519</anchor>
      <arglist>(const StarGraph *graph)</arglist>
    </member>
    <member kind="function">
      <type>const StarGraph *</type>
      <name>graph</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a8ea0e6e91a6965d8de090ef797bb4185</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>Status</type>
      <name>status</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>ace5a6df9ac9993c42cd091f6e9ebbd54</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetNodeSupply</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a7e4301ebf8c5f86143e390494a0a6f4f</anchor>
      <arglist>(NodeIndex node, FlowQuantity supply)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetArcUnitCost</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a0969b64c9993394fb1cc97a2404e12a4</anchor>
      <arglist>(ArcIndex arc, CostValue unit_cost)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetArcCapacity</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a35f31d23e3f300f2e36aa46b2e854c00</anchor>
      <arglist>(ArcIndex arc, FlowQuantity new_capacity)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetArcFlow</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a60875aa935bd9594db707bfd97eab98c</anchor>
      <arglist>(ArcIndex arc, FlowQuantity new_flow)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Solve</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a17e2df29d767d03fc9fc42aee71bf5da</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CheckFeasibility</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a01f13e85a3d12e356f26d80210a6755f</anchor>
      <arglist>(std::vector&lt; NodeIndex &gt; *const infeasible_supply_node, std::vector&lt; NodeIndex &gt; *const infeasible_demand_node)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MakeFeasible</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a60da0a24c813a9ee39f96cc36f8dd9ad</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>CostValue</type>
      <name>GetOptimalCost</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a37cb4245bf39c34116d5466d7922f565</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>Flow</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>ac5a5362a674cfb2a7589bcd77c484c17</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>Capacity</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a44c22a9d4609a54cfc7034de3e541ce7</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>CostValue</type>
      <name>UnitCost</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>ae70575b0b8bbc06301778b172752958b</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>Supply</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a67cea170737362bd37bc8c8f8f024555</anchor>
      <arglist>(NodeIndex node) const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>InitialSupply</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a15963b8d1126b858e249c58658934305</anchor>
      <arglist>(NodeIndex node) const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>FeasibleSupply</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>afa58296d5399afd27f8bed235e18309d</anchor>
      <arglist>(NodeIndex node) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetUseUpdatePrices</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a816e24f3a6af98c2b3ae1854f6fc0781</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetCheckFeasibility</name>
      <anchorfile>classoperations__research_1_1GenericMinCostFlow.html</anchorfile>
      <anchor>a9fffb489dce981de3233b1702b801eb6</anchor>
      <arglist>(bool value)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::MinCostFlowBase</name>
    <filename>classoperations__research_1_1MinCostFlowBase.html</filename>
    <member kind="enumeration">
      <type></type>
      <name>Status</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>NOT_SOLVED</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a44ab1b2f3c5251267bce749661e4e6de</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a7ba4907297a99141ebd2bc890d5a591c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>FEASIBLE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457ab78b8e2734f0e777e79b8e5f93948101</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INFEASIBLE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457af323941c683086f71ce1e7cae7a0a1b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UNBALANCED</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457ad79843de8fcdec97ba2ce9b83d6cf3c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_RESULT</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a030d97dcea9960e3d1f127024b0e2168</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_COST_RANGE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a41523b94852c3cb55c86ce0e291e3719</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>NOT_SOLVED</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a44ab1b2f3c5251267bce749661e4e6de</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a7ba4907297a99141ebd2bc890d5a591c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>FEASIBLE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457ab78b8e2734f0e777e79b8e5f93948101</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INFEASIBLE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457af323941c683086f71ce1e7cae7a0a1b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UNBALANCED</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457ad79843de8fcdec97ba2ce9b83d6cf3c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_RESULT</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a030d97dcea9960e3d1f127024b0e2168</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_COST_RANGE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a41523b94852c3cb55c86ce0e291e3719</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::StarGraphBase::NodeIterator</name>
    <filename>classoperations__research_1_1StarGraphBase_1_1NodeIterator.html</filename>
    <member kind="function">
      <type></type>
      <name>NodeIterator</name>
      <anchorfile>classoperations__research_1_1StarGraphBase_1_1NodeIterator.html</anchorfile>
      <anchor>aa88b83a7d4d58f14c22985f65b826036</anchor>
      <arglist>(const DerivedGraph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classoperations__research_1_1StarGraphBase_1_1NodeIterator.html</anchorfile>
      <anchor>a53bbc98b79bbf00e3a43504e2f3c8e1c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classoperations__research_1_1StarGraphBase_1_1NodeIterator.html</anchorfile>
      <anchor>aa5e4893b57578a159709ea47eaddcad6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase_1_1NodeIterator.html</anchorfile>
      <anchor>a75b2eefd123f27761920019ef173b310</anchor>
      <arglist>() const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::ReverseArcListGraph::OppositeIncomingArcIterator</name>
    <filename>classutil_1_1ReverseArcListGraph_1_1OppositeIncomingArcIterator.html</filename>
    <member kind="function">
      <type></type>
      <name>OppositeIncomingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>af1777cce54ddf621ce58c23b07f04644</anchor>
      <arglist>(const ReverseArcListGraph &amp;graph, NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>OppositeIncomingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a93441dfe63ef7f8ea19c28b90b363472</anchor>
      <arglist>(const ReverseArcListGraph &amp;graph, NodeIndexType node, ArcIndexType arc)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>ab32e51a40e51e10e2b6bfb8bf63dbc87</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Index</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>ab7213231061932f430f3fa421bce1d08</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>af79e306f4c5d2c04b46983e8c53fca49</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_STL_ITERATOR_FUNCTIONS</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a50c4f5e91bf22e413ecc7536f918cbec</anchor>
      <arglist>(OppositeIncomingArcIterator)</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>const ReverseArcListGraph &amp;</type>
      <name>graph_</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a6cc6151c5935827350ec7b9574ad3545</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>index_</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>adadde330a68f381bab2c87d8e85faebf</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::ReverseArcStaticGraph::OppositeIncomingArcIterator</name>
    <filename>classutil_1_1ReverseArcStaticGraph_1_1OppositeIncomingArcIterator.html</filename>
    <member kind="function">
      <type></type>
      <name>OppositeIncomingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>aad411a93142ff085a68a0d599524c3a1</anchor>
      <arglist>(const ReverseArcStaticGraph &amp;graph, NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>OppositeIncomingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>abf88ffe44a8c97009a3a14af7ac02d9d</anchor>
      <arglist>(const ReverseArcStaticGraph &amp;graph, NodeIndexType node, ArcIndexType arc)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>acc521bdcf77d31c02f46130de62abbd6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Index</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a7671d99b9b1eccee171ea381efbf6306</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>af44b76c2269e81f5dfcc61825fbb1b9f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_STL_ITERATOR_FUNCTIONS</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a63748b2737cc861bded889ff06b03e53</anchor>
      <arglist>(OppositeIncomingArcIterator)</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>const ReverseArcStaticGraph &amp;</type>
      <name>graph_</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a0dd89a6500a8ab3018be5cbe34ad11b2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>const ArcIndexType</type>
      <name>limit_</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a3a1dd084ad6093422cffb58da43de685</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>index_</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a4c6d6e4b71f0237dffe7d4ed2541edf1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::ReverseArcMixedGraph::OppositeIncomingArcIterator</name>
    <filename>classutil_1_1ReverseArcMixedGraph_1_1OppositeIncomingArcIterator.html</filename>
    <member kind="function">
      <type></type>
      <name>OppositeIncomingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>af5d41536081e936a3a2075ea225f8853</anchor>
      <arglist>(const ReverseArcMixedGraph &amp;graph, NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>OppositeIncomingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a882d40b046e4c26955b28965019df078</anchor>
      <arglist>(const ReverseArcMixedGraph &amp;graph, NodeIndexType node, ArcIndexType arc)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>aec415bacb2abe19796f3aadee8c81443</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Index</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a74bc7e56984af8afd7d65f8e77588778</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a8dd74217ea5f69b6e8b9ee60d630ac44</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_STL_ITERATOR_FUNCTIONS</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>aa5166577a05e8f457d906071599c5046</anchor>
      <arglist>(OppositeIncomingArcIterator)</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>const ReverseArcMixedGraph *</type>
      <name>graph_</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a53f8e0b613096680b6c1aa72b04ef02a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>index_</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OppositeIncomingArcIterator.html</anchorfile>
      <anchor>a3795cf3a06e595f99910b3ff9269c4b7</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::StarGraphBase::OutgoingArcIterator</name>
    <filename>classoperations__research_1_1StarGraphBase_1_1OutgoingArcIterator.html</filename>
    <member kind="function">
      <type></type>
      <name>OutgoingArcIterator</name>
      <anchorfile>classoperations__research_1_1StarGraphBase_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a1ac97e34f27cd9762c0492523e16cc01</anchor>
      <arglist>(const DerivedGraph &amp;graph, NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>OutgoingArcIterator</name>
      <anchorfile>classoperations__research_1_1StarGraphBase_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>adfc73904ca2946cf7c01c072a2e8848b</anchor>
      <arglist>(const DerivedGraph &amp;graph, NodeIndexType node, ArcIndexType arc)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1StarGraphBase_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>abe21369e05b3312ae3d43686aac340e9</anchor>
      <arglist>(const OutgoingArcIterator &amp;iterator)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classoperations__research_1_1StarGraphBase_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>af1a06e4063cb76b5babf4fc670bfb4a2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classoperations__research_1_1StarGraphBase_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>ad205c98fef9dd01f3aa32aea884844ad</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>aeef8d3d1791b5d02ee9c71bcdf38fca8</anchor>
      <arglist>() const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::ListGraph::OutgoingArcIterator</name>
    <filename>classutil_1_1ListGraph_1_1OutgoingArcIterator.html</filename>
    <member kind="function">
      <type></type>
      <name>OutgoingArcIterator</name>
      <anchorfile>classutil_1_1ListGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a9b772c46ab2530be02708f9695d6f3a3</anchor>
      <arglist>(const ListGraph &amp;graph, NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>OutgoingArcIterator</name>
      <anchorfile>classutil_1_1ListGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a6c9470ceecfb37e2aff9641ec622ed80</anchor>
      <arglist>(const ListGraph &amp;graph, NodeIndexType node, ArcIndexType arc)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classutil_1_1ListGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>afbbd6b0f0e799203943668813100794c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Index</name>
      <anchorfile>classutil_1_1ListGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a5929fff81892ec6ba4db776ca3bcae63</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classutil_1_1ListGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>aeb6459511875abb327a28591e0b5c172</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_STL_ITERATOR_FUNCTIONS</name>
      <anchorfile>classutil_1_1ListGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a7a57e14dd0b6f0b42177360c13638ec2</anchor>
      <arglist>(OutgoingArcIterator)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::ReverseArcStaticGraph::OutgoingArcIterator</name>
    <filename>classutil_1_1ReverseArcStaticGraph_1_1OutgoingArcIterator.html</filename>
    <member kind="function">
      <type></type>
      <name>OutgoingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a70d282f950c2f8e4c7aebc8809682661</anchor>
      <arglist>(const ReverseArcStaticGraph &amp;graph, NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>OutgoingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>aeaeb8e993daf64d4d0423c09b2afd501</anchor>
      <arglist>(const ReverseArcStaticGraph &amp;graph, NodeIndexType node, ArcIndexType arc)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>afed4eb7b5114e63cf61c50357ebb2e58</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Index</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a3a2d3ea00412e3914f4ec1bc2c087c0e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a63db75a3bdcd0c16af17aa319e6e1ad5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_STL_ITERATOR_FUNCTIONS</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a4f7501971b3fcc6ab34772a93c449ac6</anchor>
      <arglist>(OutgoingArcIterator)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::ReverseArcListGraph::OutgoingArcIterator</name>
    <filename>classutil_1_1ReverseArcListGraph_1_1OutgoingArcIterator.html</filename>
    <member kind="function">
      <type></type>
      <name>OutgoingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a3fdd14f693dbdbbeab976f840cb32392</anchor>
      <arglist>(const ReverseArcListGraph &amp;graph, NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>OutgoingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a78b56030574415d76b1e2d6ebb106fbd</anchor>
      <arglist>(const ReverseArcListGraph &amp;graph, NodeIndexType node, ArcIndexType arc)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>afd40fe3493c52b31693d2bbbedf840d4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Index</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>abb96a5d77ae6d00140095ec1cfb90b29</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>aad0edaa44248b5f9ac54f94dc120fcc1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_STL_ITERATOR_FUNCTIONS</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>afbbb9e7b67dfdc4dbd37f2f36b74d07c</anchor>
      <arglist>(OutgoingArcIterator)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::ReverseArcMixedGraph::OutgoingArcIterator</name>
    <filename>classutil_1_1ReverseArcMixedGraph_1_1OutgoingArcIterator.html</filename>
    <member kind="function">
      <type></type>
      <name>OutgoingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a13eb1d6a9e203dbcc707e1b226a3fc2d</anchor>
      <arglist>(const ReverseArcMixedGraph &amp;graph, NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>OutgoingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a9c3b33ea848d57f4acf0673b9e239919</anchor>
      <arglist>(const ReverseArcMixedGraph &amp;graph, NodeIndexType node, ArcIndexType arc)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a2f4eeac98c28c959041b6632f7a56d24</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Index</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a82101ea1e7f7a3c7ea7b9b12cef57caf</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a172d9883e9d4efe61c986ff0648a1f3d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_STL_ITERATOR_FUNCTIONS</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>ac265893b8e1a6db1380522442bf66c84</anchor>
      <arglist>(OutgoingArcIterator)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::StaticGraph::OutgoingArcIterator</name>
    <filename>classutil_1_1StaticGraph_1_1OutgoingArcIterator.html</filename>
    <member kind="function">
      <type></type>
      <name>OutgoingArcIterator</name>
      <anchorfile>classutil_1_1StaticGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a15d6bd250626ba7eb0c0ca869e80e82e</anchor>
      <arglist>(const StaticGraph &amp;graph, NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>OutgoingArcIterator</name>
      <anchorfile>classutil_1_1StaticGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a9daf27c5c2732ffaeb63dccfa31ffebd</anchor>
      <arglist>(const StaticGraph &amp;graph, NodeIndexType node, ArcIndexType arc)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classutil_1_1StaticGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a86560381d51296c469edd57e933ecf79</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Index</name>
      <anchorfile>classutil_1_1StaticGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>af5c520ba6fbcd2d229d7a61745ba7cf5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classutil_1_1StaticGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a2815013e93921b115683cf9c95df8bc4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_STL_ITERATOR_FUNCTIONS</name>
      <anchorfile>classutil_1_1StaticGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>af3fcc61c322d02802cf1dda0c54189ca</anchor>
      <arglist>(OutgoingArcIterator)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::CompleteBipartiteGraph::OutgoingArcIterator</name>
    <filename>classutil_1_1CompleteBipartiteGraph_1_1OutgoingArcIterator.html</filename>
    <member kind="function">
      <type></type>
      <name>OutgoingArcIterator</name>
      <anchorfile>classutil_1_1CompleteBipartiteGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a0708b60d3463601fa36d682f1429f78a</anchor>
      <arglist>(const CompleteBipartiteGraph &amp;graph, NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classutil_1_1CompleteBipartiteGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a50be2d4453f8536677b936c48251ed71</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Index</name>
      <anchorfile>classutil_1_1CompleteBipartiteGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a87f4a00a8b306480db02d93fa87bf223</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classutil_1_1CompleteBipartiteGraph_1_1OutgoingArcIterator.html</anchorfile>
      <anchor>a35eeda0546643fca60129e9fa15e2a61</anchor>
      <arglist>()</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::ListGraph::OutgoingHeadIterator</name>
    <filename>classutil_1_1ListGraph_1_1OutgoingHeadIterator.html</filename>
    <member kind="typedef">
      <type>std::input_iterator_tag</type>
      <name>iterator_category</name>
      <anchorfile>classutil_1_1ListGraph_1_1OutgoingHeadIterator.html</anchorfile>
      <anchor>a2672503e6869d2d2585047c3f5bf28ed</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ptrdiff_t</type>
      <name>difference_type</name>
      <anchorfile>classutil_1_1ListGraph_1_1OutgoingHeadIterator.html</anchorfile>
      <anchor>a31e54d4ae7d4b93c43ea64f950221328</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>const NodeIndexType *</type>
      <name>pointer</name>
      <anchorfile>classutil_1_1ListGraph_1_1OutgoingHeadIterator.html</anchorfile>
      <anchor>a6bd5b4ffde55cadc4e58c80a08066e2d</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>const NodeIndexType &amp;</type>
      <name>reference</name>
      <anchorfile>classutil_1_1ListGraph_1_1OutgoingHeadIterator.html</anchorfile>
      <anchor>ad37fb276585027d2996d3fbebc7984db</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>NodeIndexType</type>
      <name>value_type</name>
      <anchorfile>classutil_1_1ListGraph_1_1OutgoingHeadIterator.html</anchorfile>
      <anchor>ae46346c25624805c3daee4d82904afbe</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>OutgoingHeadIterator</name>
      <anchorfile>classutil_1_1ListGraph_1_1OutgoingHeadIterator.html</anchorfile>
      <anchor>a0af1c5f92e923e3a8b118b3bf6ebb2c2</anchor>
      <arglist>(const ListGraph &amp;graph, NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>OutgoingHeadIterator</name>
      <anchorfile>classutil_1_1ListGraph_1_1OutgoingHeadIterator.html</anchorfile>
      <anchor>a97a4cc7fa4bade46b2fc7c6105c0a4d8</anchor>
      <arglist>(const ListGraph &amp;graph, NodeIndexType node, ArcIndexType arc)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classutil_1_1ListGraph_1_1OutgoingHeadIterator.html</anchorfile>
      <anchor>af226be77ff7f280952f37377e12edbf5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Index</name>
      <anchorfile>classutil_1_1ListGraph_1_1OutgoingHeadIterator.html</anchorfile>
      <anchor>a37a2180b903e8a6de06b6fc7bc053ec9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classutil_1_1ListGraph_1_1OutgoingHeadIterator.html</anchorfile>
      <anchor>a32219a5b2ba68d54fdad480b0ddc85ec</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>operator!=</name>
      <anchorfile>classutil_1_1ListGraph_1_1OutgoingHeadIterator.html</anchorfile>
      <anchor>ab65b44c6c2202b62ce0010c0b0a17db2</anchor>
      <arglist>(const typename ListGraph&lt; NodeIndexType, ArcIndexType &gt;::OutgoingHeadIterator &amp;other) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>operator *</name>
      <anchorfile>classutil_1_1ListGraph_1_1OutgoingHeadIterator.html</anchorfile>
      <anchor>ad746758b0ebb63bfdd58c1717fe02f67</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>operator++</name>
      <anchorfile>classutil_1_1ListGraph_1_1OutgoingHeadIterator.html</anchorfile>
      <anchor>ac18c4a7cd6790c5f4bc86e360b27e71d</anchor>
      <arglist>()</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::ReverseArcListGraph::OutgoingHeadIterator</name>
    <filename>classutil_1_1ReverseArcListGraph_1_1OutgoingHeadIterator.html</filename>
    <member kind="function">
      <type></type>
      <name>OutgoingHeadIterator</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OutgoingHeadIterator.html</anchorfile>
      <anchor>a543ea066e34c54339beab44e7e535602</anchor>
      <arglist>(const ReverseArcListGraph &amp;graph, NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>OutgoingHeadIterator</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OutgoingHeadIterator.html</anchorfile>
      <anchor>a6cfd2d2e68032ff097430ffc24c296ec</anchor>
      <arglist>(const ReverseArcListGraph &amp;graph, NodeIndexType node, ArcIndexType arc)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OutgoingHeadIterator.html</anchorfile>
      <anchor>aebe7b896a8348608a5480a30183319b4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Index</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OutgoingHeadIterator.html</anchorfile>
      <anchor>abc53834789579a19c77f36885d3c51e4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OutgoingHeadIterator.html</anchorfile>
      <anchor>a2aaa9df08e2df6d5b3b58ff68aaa89a5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_STL_ITERATOR_FUNCTIONS</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OutgoingHeadIterator.html</anchorfile>
      <anchor>a85d66cce692a40535ff5f7b233b55553</anchor>
      <arglist>(OutgoingHeadIterator)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::EbertGraph::OutgoingOrOppositeIncomingArcIterator</name>
    <filename>classoperations__research_1_1EbertGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</filename>
    <member kind="function">
      <type></type>
      <name>OutgoingOrOppositeIncomingArcIterator</name>
      <anchorfile>classoperations__research_1_1EbertGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>a41f5402bdc39bc7c1a8e713b36bb1f6f</anchor>
      <arglist>(const EbertGraph &amp;graph, NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>OutgoingOrOppositeIncomingArcIterator</name>
      <anchorfile>classoperations__research_1_1EbertGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>abeeb4cd46875dc1553f7877efdd87ce1</anchor>
      <arglist>(const EbertGraph &amp;graph, NodeIndexType node, ArcIndexType arc)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1EbertGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>aede9dadb0a3ea190c0bfd9123d362172</anchor>
      <arglist>(const OutgoingOrOppositeIncomingArcIterator &amp;iterator)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classoperations__research_1_1EbertGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>a9d2baaf41a6958cb75efba3e2c446bb8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classoperations__research_1_1EbertGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>ae6d48e645be864754bb1efd997506550</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Index</name>
      <anchorfile>classoperations__research_1_1EbertGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>a90bb7f82e799ea13576da051837ac206</anchor>
      <arglist>() const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::ReverseArcMixedGraph::OutgoingOrOppositeIncomingArcIterator</name>
    <filename>classutil_1_1ReverseArcMixedGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</filename>
    <member kind="function">
      <type></type>
      <name>OutgoingOrOppositeIncomingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>aff95fdc437a0d963e9b4937cc4ad10b4</anchor>
      <arglist>(const ReverseArcMixedGraph &amp;graph, NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>OutgoingOrOppositeIncomingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>af353e150020f4320d272c334bcae08c5</anchor>
      <arglist>(const ReverseArcMixedGraph &amp;graph, NodeIndexType node, ArcIndexType arc)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>a2ce27aff47f5f917d2638855a11006b6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Index</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>a07692556f6a74a6ec520e943ce00e980</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>a9b465c88524b5b71c10d0d78dc3015f3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_STL_ITERATOR_FUNCTIONS</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>a3fa4faf0f235733d45830a473ca413a8</anchor>
      <arglist>(OutgoingOrOppositeIncomingArcIterator)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::ReverseArcStaticGraph::OutgoingOrOppositeIncomingArcIterator</name>
    <filename>classutil_1_1ReverseArcStaticGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</filename>
    <member kind="function">
      <type></type>
      <name>OutgoingOrOppositeIncomingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>a8e0aa831c04c3dd5c8b9a755ebc24fc5</anchor>
      <arglist>(const ReverseArcStaticGraph &amp;graph, NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>OutgoingOrOppositeIncomingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>a8c3a7b41bd5314226d4517b920525fb8</anchor>
      <arglist>(const ReverseArcStaticGraph &amp;graph, NodeIndexType node, ArcIndexType arc)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Index</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>a6ff22ab544af24017b576a3f583e4d48</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>ad1f79b2ac841f3856578d6840e2bf25a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>ac3219fdc6b75a59fcf8752219f0fa959</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_STL_ITERATOR_FUNCTIONS</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>a8540e6821655f421531f91ef3749ecbf</anchor>
      <arglist>(OutgoingOrOppositeIncomingArcIterator)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::ReverseArcListGraph::OutgoingOrOppositeIncomingArcIterator</name>
    <filename>classutil_1_1ReverseArcListGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</filename>
    <member kind="function">
      <type></type>
      <name>OutgoingOrOppositeIncomingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>af413badb2f4d154c6308a731ab45c4a5</anchor>
      <arglist>(const ReverseArcListGraph &amp;graph, NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>OutgoingOrOppositeIncomingArcIterator</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>a1ff541be19a673afa1bc0f797dcfb7ef</anchor>
      <arglist>(const ReverseArcListGraph &amp;graph, NodeIndexType node, ArcIndexType arc)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Ok</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>a1161e8aa1251ada08027562c6ed05f08</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>Index</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>a2cb0eea9e8b502a6d8313667d76ab3ef</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Next</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>ade64e3094b79d73fba19a7539e41439a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_STL_ITERATOR_FUNCTIONS</name>
      <anchorfile>classutil_1_1ReverseArcListGraph_1_1OutgoingOrOppositeIncomingArcIterator.html</anchorfile>
      <anchor>abe60ff2f3491a9d0389be8b5276391cd</anchor>
      <arglist>(OutgoingOrOppositeIncomingArcIterator)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::PermutationIndexComparisonByArcHead</name>
    <filename>classoperations__research_1_1PermutationIndexComparisonByArcHead.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <member kind="function">
      <type></type>
      <name>PermutationIndexComparisonByArcHead</name>
      <anchorfile>classoperations__research_1_1PermutationIndexComparisonByArcHead.html</anchorfile>
      <anchor>a712a93e04cd99aaef989ed89377994b6</anchor>
      <arglist>(const ZVector&lt; NodeIndexType &gt; &amp;head)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>operator()</name>
      <anchorfile>classoperations__research_1_1PermutationIndexComparisonByArcHead.html</anchorfile>
      <anchor>a4d8724da35186aab5d5122d90dee84a7</anchor>
      <arglist>(ArcIndexType a, ArcIndexType b) const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::PriorityQueueWithRestrictedPush</name>
    <filename>classoperations__research_1_1PriorityQueueWithRestrictedPush.html</filename>
    <templarg>Element</templarg>
    <templarg>IntegerPriority</templarg>
    <member kind="function">
      <type></type>
      <name>PriorityQueueWithRestrictedPush</name>
      <anchorfile>classoperations__research_1_1PriorityQueueWithRestrictedPush.html</anchorfile>
      <anchor>ab5a37cc35af8067c734810424d4f395d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsEmpty</name>
      <anchorfile>classoperations__research_1_1PriorityQueueWithRestrictedPush.html</anchorfile>
      <anchor>a0fa26c84168a6d71010556b0d6541a1f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1PriorityQueueWithRestrictedPush.html</anchorfile>
      <anchor>ad5d7012545c74d85dd938ea6bf9e9537</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Push</name>
      <anchorfile>classoperations__research_1_1PriorityQueueWithRestrictedPush.html</anchorfile>
      <anchor>a2662f061f688af21e19f3ac53027ed4b</anchor>
      <arglist>(Element element, IntegerPriority priority)</arglist>
    </member>
    <member kind="function">
      <type>Element</type>
      <name>Pop</name>
      <anchorfile>classoperations__research_1_1PriorityQueueWithRestrictedPush.html</anchorfile>
      <anchor>a9597013d76010425ffc592a32ff4f259</anchor>
      <arglist>()</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>PriorityQueueWithRestrictedPush&lt; NodeIndex, NodeHeight &gt;</name>
    <filename>classoperations__research_1_1PriorityQueueWithRestrictedPush.html</filename>
    <member kind="function">
      <type></type>
      <name>PriorityQueueWithRestrictedPush</name>
      <anchorfile>classoperations__research_1_1PriorityQueueWithRestrictedPush.html</anchorfile>
      <anchor>ab5a37cc35af8067c734810424d4f395d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsEmpty</name>
      <anchorfile>classoperations__research_1_1PriorityQueueWithRestrictedPush.html</anchorfile>
      <anchor>a0fa26c84168a6d71010556b0d6541a1f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1PriorityQueueWithRestrictedPush.html</anchorfile>
      <anchor>ad5d7012545c74d85dd938ea6bf9e9537</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Push</name>
      <anchorfile>classoperations__research_1_1PriorityQueueWithRestrictedPush.html</anchorfile>
      <anchor>a2662f061f688af21e19f3ac53027ed4b</anchor>
      <arglist>(NodeIndex element, NodeHeight priority)</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>Pop</name>
      <anchorfile>classoperations__research_1_1PriorityQueueWithRestrictedPush.html</anchorfile>
      <anchor>a9597013d76010425ffc592a32ff4f259</anchor>
      <arglist>()</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::PruningHamiltonianSolver</name>
    <filename>classoperations__research_1_1PruningHamiltonianSolver.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <member kind="typedef">
      <type>uint32</type>
      <name>Integer</name>
      <anchorfile>classoperations__research_1_1PruningHamiltonianSolver.html</anchorfile>
      <anchor>a2857abdd40d541b03ad21bd053b0ac54</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>Set&lt; Integer &gt;</type>
      <name>NodeSet</name>
      <anchorfile>classoperations__research_1_1PruningHamiltonianSolver.html</anchorfile>
      <anchor>ac958d277ef05676ff46e2a48c77e0925</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>PruningHamiltonianSolver</name>
      <anchorfile>classoperations__research_1_1PruningHamiltonianSolver.html</anchorfile>
      <anchor>a247ca43d1874d57d28e64bba65c8eeb2</anchor>
      <arglist>(CostFunction cost)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>PruningHamiltonianSolver</name>
      <anchorfile>classoperations__research_1_1PruningHamiltonianSolver.html</anchorfile>
      <anchor>ae90975766e34a4bc392f441c85ab0927</anchor>
      <arglist>(int num_nodes, CostFunction cost)</arglist>
    </member>
    <member kind="function">
      <type>CostType</type>
      <name>HamiltonianCost</name>
      <anchorfile>classoperations__research_1_1PruningHamiltonianSolver.html</anchorfile>
      <anchor>aba4aa231b4ea3f9bf71055372a847df7</anchor>
      <arglist>(int end_node)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::ReverseArcListGraph</name>
    <filename>classutil_1_1ReverseArcListGraph.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <base>BaseGraph&lt; NodeIndexType, ArcIndexType, true &gt;</base>
    <class kind="class">util::ReverseArcListGraph::IncomingArcIterator</class>
    <class kind="class">util::ReverseArcListGraph::OppositeIncomingArcIterator</class>
    <class kind="class">util::ReverseArcListGraph::OutgoingArcIterator</class>
    <class kind="class">util::ReverseArcListGraph::OutgoingHeadIterator</class>
    <class kind="class">util::ReverseArcListGraph::OutgoingOrOppositeIncomingArcIterator</class>
    <member kind="typedef">
      <type>NodeIndexType</type>
      <name>NodeIndex</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>adb271cf4bcf2de5b5bbe300d7054af29</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ArcIndexType</type>
      <name>ArcIndex</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a0eba6e5899924388644dfa2258ae8929</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>ReverseArcListGraph</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>ac553bb63399815ac609976e5a3bd8732</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>ReverseArcListGraph</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>a885571656abd56593b2edbebb451d67f</anchor>
      <arglist>(NodeIndexType num_nodes, ArcIndexType arc_capacity)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>OppositeArc</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>aa3306c75dd28ca037205f382ac832b2a</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>OutDegree</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>ad25429b667ebd6e2ac662b76f3e02eae</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>InDegree</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>aeffdaa7d0a6c7b6355ee1dab33524961</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OutgoingArcIterator &gt;</type>
      <name>OutgoingArcs</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>ac8923133838003d7a7ffb4500f0d4ffa</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; IncomingArcIterator &gt;</type>
      <name>IncomingArcs</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>ad0e7957e515d9c50f931c00166d099ce</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OutgoingOrOppositeIncomingArcIterator &gt;</type>
      <name>OutgoingOrOppositeIncomingArcs</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>a104172ed25aa4cea18cd3a5c2187d4c7</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OppositeIncomingArcIterator &gt;</type>
      <name>OppositeIncomingArcs</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>a3cd1d1d0e50aaa2f30817db44cca8881</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OutgoingArcIterator &gt;</type>
      <name>OutgoingArcsStartingFrom</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>a60d3a435f90e2c963047cc6d8c6ad363</anchor>
      <arglist>(NodeIndexType node, ArcIndexType from) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; IncomingArcIterator &gt;</type>
      <name>IncomingArcsStartingFrom</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>a1a89c1efcf87cce6cec110126eb05641</anchor>
      <arglist>(NodeIndexType node, ArcIndexType from) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OutgoingOrOppositeIncomingArcIterator &gt;</type>
      <name>OutgoingOrOppositeIncomingArcsStartingFrom</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>a96a609a53497ca38517dba5c277c7798</anchor>
      <arglist>(NodeIndexType node, ArcIndexType from) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OppositeIncomingArcIterator &gt;</type>
      <name>OppositeIncomingArcsStartingFrom</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>a1e345c79e59f71e088f4876be93a903d</anchor>
      <arglist>(NodeIndexType node, ArcIndexType from) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OutgoingHeadIterator &gt;</type>
      <name>operator[]</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>a28d6f97b2135e9d51bad632c2556fa56</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Head</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>a2fdb48f771da93712d9386c60615cf04</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Tail</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>ac7b475304d3677d4a29c676e64c3e09c</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ReserveNodes</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>ae7734e3c4b1930b2859b4813bf6e238b</anchor>
      <arglist>(NodeIndexType bound) override</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ReserveArcs</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>a6409e1c967f2062aa83b4bde26a931f1</anchor>
      <arglist>(ArcIndexType bound) override</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddNode</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>aadf05a4b2729e2844c3251d82b9e862c</anchor>
      <arglist>(NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>AddArc</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>a79291a548af693328d0e4e8f64cdc745</anchor>
      <arglist>(NodeIndexType tail, NodeIndexType head)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Build</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>a25d91ab3a2ced7900dd2f5cbcf1ed587</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Build</name>
      <anchorfile>classutil_1_1ReverseArcListGraph.html</anchorfile>
      <anchor>a52c31a4ce66ba0c4a24cbe884fda377c</anchor>
      <arglist>(std::vector&lt; ArcIndexType &gt; *permutation)</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>num_nodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a0f551c921fa0b5aaa334a6e36f61db4a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>num_arcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aefb468e4d4a3128c91b3bad9f5b314c8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; NodeIndex &gt;</type>
      <name>AllNodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a32ba4a5ca9a4b89f750eb2dc56518b02</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; ArcIndex &gt;</type>
      <name>AllForwardArcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>abcfd7c21143e5ed38573c0dd60826dd8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>abf853acea86e01356f53055f77661770</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsArcValid</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a2097ccda3b1ad27e2c82166979018bda</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>node_capacity</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a463d57480c9563a7a707c5d0928c9946</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>arc_capacity</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a92ffd852b2ab2e5241f9832e71a2de71</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Reserve</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab731ca5c638e7b1b0a2c459c94a90f55</anchor>
      <arglist>(NodeIndexType node_capacity, ArcIndexType arc_capacity)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>FreezeCapacities</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aa3bd051d1e141b09dda17aa9b5f24f69</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GroupForwardArcsByFunctor</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a07214b96597069d781e27b1dd17ef83e</anchor>
      <arglist>(const A &amp;a, B *b)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_end_arc_index</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a346b8a5811f0e287e1ebce2de2c1ad28</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kNilNode</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab1292c82a3f43be3bd57b63a05fe0214</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kNilArc</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ad5b77846f77c2771e840820812ad5521</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>ComputeCumulativeSum</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ac47030fcb75a3220f8cf9ed79697056e</anchor>
      <arglist>(std::vector&lt; ArcIndexType &gt; *v)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>BuildStartAndForwardHead</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a63fd586eed6c345866317e2f0faf377e</anchor>
      <arglist>(SVector&lt; NodeIndexType &gt; *head, std::vector&lt; ArcIndexType &gt; *start, std::vector&lt; ArcIndexType &gt; *permutation)</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>num_nodes_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a473db46afec1eabf0762411830dee30f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>node_capacity_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a424bd34a9767e7edeaf3a60ecd3cb000</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>num_arcs_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>af16f99d41856a7b22ae8a226ef09abff</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>arc_capacity_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a895e891d1ad52ce3efcfeb7ba11194c7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>const_capacities_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aa980e5526b9ded17a83928fc339c71e4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::ReverseArcMixedGraph</name>
    <filename>classutil_1_1ReverseArcMixedGraph.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <base>BaseGraph&lt; NodeIndexType, ArcIndexType, true &gt;</base>
    <class kind="class">util::ReverseArcMixedGraph::IncomingArcIterator</class>
    <class kind="class">util::ReverseArcMixedGraph::OppositeIncomingArcIterator</class>
    <class kind="class">util::ReverseArcMixedGraph::OutgoingArcIterator</class>
    <class kind="class">util::ReverseArcMixedGraph::OutgoingOrOppositeIncomingArcIterator</class>
    <member kind="typedef">
      <type>NodeIndexType</type>
      <name>NodeIndex</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>adb271cf4bcf2de5b5bbe300d7054af29</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ArcIndexType</type>
      <name>ArcIndex</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a0eba6e5899924388644dfa2258ae8929</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>ReverseArcMixedGraph</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>af10d16de8964a7abf555759bb9f257aa</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>ReverseArcMixedGraph</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>a6735921f3354efcf696ad98e4cf7c2c1</anchor>
      <arglist>(NodeIndexType num_nodes, ArcIndexType arc_capacity)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>OutDegree</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>addeb94680d05c79b1be501edc5823209</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>InDegree</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>ae5ea8fcc78ca9b142e07665d447ad2bd</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OutgoingArcIterator &gt;</type>
      <name>OutgoingArcs</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>a6e39d187175616e99c958a81bcd1831f</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; IncomingArcIterator &gt;</type>
      <name>IncomingArcs</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>a4923ba6abf7a599e833becfa82750a86</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OutgoingOrOppositeIncomingArcIterator &gt;</type>
      <name>OutgoingOrOppositeIncomingArcs</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>a9cdfbde174efbc5747d58330dbd85e86</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OppositeIncomingArcIterator &gt;</type>
      <name>OppositeIncomingArcs</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>a2e920761999c90a1b266b96d609d8464</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OutgoingArcIterator &gt;</type>
      <name>OutgoingArcsStartingFrom</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>a41eeddd2d42998985b295d05fc04439e</anchor>
      <arglist>(NodeIndexType node, ArcIndexType from) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; IncomingArcIterator &gt;</type>
      <name>IncomingArcsStartingFrom</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>ac3108bd8dd239c6adb32b30c16c5fc70</anchor>
      <arglist>(NodeIndexType node, ArcIndexType from) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OutgoingOrOppositeIncomingArcIterator &gt;</type>
      <name>OutgoingOrOppositeIncomingArcsStartingFrom</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>a09e6c29120c6f7f0a84fb4c02af0f0ee</anchor>
      <arglist>(NodeIndexType node, ArcIndexType from) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OppositeIncomingArcIterator &gt;</type>
      <name>OppositeIncomingArcsStartingFrom</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>ae53769b81e0791bccfdf1187d4fcf7f6</anchor>
      <arglist>(NodeIndexType node, ArcIndexType from) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; NodeIndexType const  * &gt;</type>
      <name>operator[]</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>aff7c6ef395dfc941edac13b29e154b54</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>OppositeArc</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>a86effffe3928593a808edf548dd6b832</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Head</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>a5ebe11f2abcd78ad1f5606e27d1688bb</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Tail</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>a351cbc3c0c79e22f010e697bf65d7100</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ReserveArcs</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>ad39d8df784046ecfb6192ac0a0b5da44</anchor>
      <arglist>(ArcIndexType bound) override</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddNode</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>aab290c8728947d80dbf9fec6e320c3d1</anchor>
      <arglist>(NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>AddArc</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>a362fa49b586ff223477aab7a53d17f14</anchor>
      <arglist>(NodeIndexType tail, NodeIndexType head)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Build</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>a06e56a529396bbd2595f5c32c2f73142</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Build</name>
      <anchorfile>classutil_1_1ReverseArcMixedGraph.html</anchorfile>
      <anchor>aba7702cbe355fc0d1c7d1a67d07ba2bc</anchor>
      <arglist>(std::vector&lt; ArcIndexType &gt; *permutation)</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>num_nodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a0f551c921fa0b5aaa334a6e36f61db4a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>num_arcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aefb468e4d4a3128c91b3bad9f5b314c8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; NodeIndex &gt;</type>
      <name>AllNodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a32ba4a5ca9a4b89f750eb2dc56518b02</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; ArcIndex &gt;</type>
      <name>AllForwardArcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>abcfd7c21143e5ed38573c0dd60826dd8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>abf853acea86e01356f53055f77661770</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsArcValid</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a2097ccda3b1ad27e2c82166979018bda</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>node_capacity</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a463d57480c9563a7a707c5d0928c9946</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>arc_capacity</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a92ffd852b2ab2e5241f9832e71a2de71</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>ReserveNodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a69a71cbb575b13bde9899f5a6a217139</anchor>
      <arglist>(NodeIndexType bound)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Reserve</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab731ca5c638e7b1b0a2c459c94a90f55</anchor>
      <arglist>(NodeIndexType node_capacity, ArcIndexType arc_capacity)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>FreezeCapacities</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aa3bd051d1e141b09dda17aa9b5f24f69</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GroupForwardArcsByFunctor</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a07214b96597069d781e27b1dd17ef83e</anchor>
      <arglist>(const A &amp;a, B *b)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_end_arc_index</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a346b8a5811f0e287e1ebce2de2c1ad28</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kNilNode</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab1292c82a3f43be3bd57b63a05fe0214</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kNilArc</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ad5b77846f77c2771e840820812ad5521</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>ComputeCumulativeSum</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ac47030fcb75a3220f8cf9ed79697056e</anchor>
      <arglist>(std::vector&lt; ArcIndexType &gt; *v)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>BuildStartAndForwardHead</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a63fd586eed6c345866317e2f0faf377e</anchor>
      <arglist>(SVector&lt; NodeIndexType &gt; *head, std::vector&lt; ArcIndexType &gt; *start, std::vector&lt; ArcIndexType &gt; *permutation)</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>num_nodes_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a473db46afec1eabf0762411830dee30f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>node_capacity_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a424bd34a9767e7edeaf3a60ecd3cb000</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>num_arcs_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>af16f99d41856a7b22ae8a226ef09abff</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>arc_capacity_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a895e891d1ad52ce3efcfeb7ba11194c7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>const_capacities_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aa980e5526b9ded17a83928fc339c71e4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::ReverseArcStaticGraph</name>
    <filename>classutil_1_1ReverseArcStaticGraph.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <base>BaseGraph&lt; NodeIndexType, ArcIndexType, true &gt;</base>
    <class kind="class">util::ReverseArcStaticGraph::IncomingArcIterator</class>
    <class kind="class">util::ReverseArcStaticGraph::OppositeIncomingArcIterator</class>
    <class kind="class">util::ReverseArcStaticGraph::OutgoingArcIterator</class>
    <class kind="class">util::ReverseArcStaticGraph::OutgoingOrOppositeIncomingArcIterator</class>
    <member kind="typedef">
      <type>NodeIndexType</type>
      <name>NodeIndex</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>adb271cf4bcf2de5b5bbe300d7054af29</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ArcIndexType</type>
      <name>ArcIndex</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a0eba6e5899924388644dfa2258ae8929</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>ReverseArcStaticGraph</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>a98bf967d563272b62a207d179aae5973</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>ReverseArcStaticGraph</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>ac9b88c60dccd3b6aafa96df2de557a23</anchor>
      <arglist>(NodeIndexType num_nodes, ArcIndexType arc_capacity)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>OutDegree</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>aa9ec825f0a93d13a5c93e82b42cc1088</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>InDegree</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>aacb8fc16527ae0e30a874393a94cd8ee</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OutgoingArcIterator &gt;</type>
      <name>OutgoingArcs</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>af7ab2d9151c0b94ecb6569a369ce00b5</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; IncomingArcIterator &gt;</type>
      <name>IncomingArcs</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>a53ad65ff7f20bd4a6fb2494ed6dd2490</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OutgoingOrOppositeIncomingArcIterator &gt;</type>
      <name>OutgoingOrOppositeIncomingArcs</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>a383413966b4ee3790931137c44916bd3</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OppositeIncomingArcIterator &gt;</type>
      <name>OppositeIncomingArcs</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>a1708f7e290d33a29936556e7453a5092</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OutgoingArcIterator &gt;</type>
      <name>OutgoingArcsStartingFrom</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>a50115c615a3af5b9d50afc51a75541c3</anchor>
      <arglist>(NodeIndexType node, ArcIndexType from) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; IncomingArcIterator &gt;</type>
      <name>IncomingArcsStartingFrom</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>aa040d7a9d3e8af80b21f9ee07f6bae34</anchor>
      <arglist>(NodeIndexType node, ArcIndexType from) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OutgoingOrOppositeIncomingArcIterator &gt;</type>
      <name>OutgoingOrOppositeIncomingArcsStartingFrom</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>a61603e21e903115daf755e297bf652bb</anchor>
      <arglist>(NodeIndexType node, ArcIndexType from) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OppositeIncomingArcIterator &gt;</type>
      <name>OppositeIncomingArcsStartingFrom</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>a486c54240d6d491740aac1a4bd574f48</anchor>
      <arglist>(NodeIndexType node, ArcIndexType from) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; NodeIndexType const  * &gt;</type>
      <name>operator[]</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>a0ff94f595442b19c2de253a2685fe27f</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>OppositeArc</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>a2f8dc1bf8f2242b19aacc876301365a6</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Head</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>a72e8ed03fe3f8ff27d156622a86900aa</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Tail</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>a12cf8d1312236cac409d9a1a934c1ad3</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ReserveArcs</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>a950e85c43ad31af8dd24a15e895c77c7</anchor>
      <arglist>(ArcIndexType bound) override</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddNode</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>af83383a45904db07bed45418199ff2dc</anchor>
      <arglist>(NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>AddArc</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>a0b801f8cece53ce666b90be4a8b76ae6</anchor>
      <arglist>(NodeIndexType tail, NodeIndexType head)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Build</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>a485803d141f9c80beaaae7094548d672</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Build</name>
      <anchorfile>classutil_1_1ReverseArcStaticGraph.html</anchorfile>
      <anchor>a590ee70b96ce87433ba89b067495aa5f</anchor>
      <arglist>(std::vector&lt; ArcIndexType &gt; *permutation)</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>num_nodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a0f551c921fa0b5aaa334a6e36f61db4a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>num_arcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aefb468e4d4a3128c91b3bad9f5b314c8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; NodeIndex &gt;</type>
      <name>AllNodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a32ba4a5ca9a4b89f750eb2dc56518b02</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; ArcIndex &gt;</type>
      <name>AllForwardArcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>abcfd7c21143e5ed38573c0dd60826dd8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>abf853acea86e01356f53055f77661770</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsArcValid</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a2097ccda3b1ad27e2c82166979018bda</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>node_capacity</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a463d57480c9563a7a707c5d0928c9946</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>arc_capacity</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a92ffd852b2ab2e5241f9832e71a2de71</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>ReserveNodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a69a71cbb575b13bde9899f5a6a217139</anchor>
      <arglist>(NodeIndexType bound)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Reserve</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab731ca5c638e7b1b0a2c459c94a90f55</anchor>
      <arglist>(NodeIndexType node_capacity, ArcIndexType arc_capacity)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>FreezeCapacities</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aa3bd051d1e141b09dda17aa9b5f24f69</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GroupForwardArcsByFunctor</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a07214b96597069d781e27b1dd17ef83e</anchor>
      <arglist>(const A &amp;a, B *b)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_end_arc_index</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a346b8a5811f0e287e1ebce2de2c1ad28</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kNilNode</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab1292c82a3f43be3bd57b63a05fe0214</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kNilArc</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ad5b77846f77c2771e840820812ad5521</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>ComputeCumulativeSum</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ac47030fcb75a3220f8cf9ed79697056e</anchor>
      <arglist>(std::vector&lt; ArcIndexType &gt; *v)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>BuildStartAndForwardHead</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a63fd586eed6c345866317e2f0faf377e</anchor>
      <arglist>(SVector&lt; NodeIndexType &gt; *head, std::vector&lt; ArcIndexType &gt; *start, std::vector&lt; ArcIndexType &gt; *permutation)</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>num_nodes_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a473db46afec1eabf0762411830dee30f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>node_capacity_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a424bd34a9767e7edeaf3a60ecd3cb000</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>num_arcs_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>af16f99d41856a7b22ae8a226ef09abff</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>arc_capacity_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a895e891d1ad52ce3efcfeb7ba11194c7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>const_capacities_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aa980e5526b9ded17a83928fc339c71e4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>SccCounterOutput</name>
    <filename>structSccCounterOutput.html</filename>
    <templarg></templarg>
    <member kind="function">
      <type>void</type>
      <name>emplace_back</name>
      <anchorfile>structSccCounterOutput.html</anchorfile>
      <anchor>a98b2cfdab0fa5812eec11b4a5e85bf05</anchor>
      <arglist>(NodeIndex const *b, NodeIndex const *e)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>size</name>
      <anchorfile>structSccCounterOutput.html</anchorfile>
      <anchor>a31eef251d309df342e2dd150389e2458</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>number_of_components</name>
      <anchorfile>structSccCounterOutput.html</anchorfile>
      <anchor>aa506bc8bf4c4be4615a22a7d1d1d1662</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::Set</name>
    <filename>classoperations__research_1_1Set.html</filename>
    <templarg></templarg>
    <member kind="typedef">
      <type>Integer</type>
      <name>IntegerType</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>a2eb0206e8f324178feddb08668851373</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>Set</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>a1af4b8873ead26e8375650d7cbd80ff9</anchor>
      <arglist>(Integer n)</arglist>
    </member>
    <member kind="function">
      <type>Integer</type>
      <name>value</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>ad84aaa6dd878e39e09b522f9dd7653eb</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>Set</type>
      <name>AddElement</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>aa0c538f9b81cb0081a115b5ea51b9d6e</anchor>
      <arglist>(int n) const</arglist>
    </member>
    <member kind="function">
      <type>Set</type>
      <name>RemoveElement</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>aa6d7b94f2dfe466cfe4fa7846b040721</anchor>
      <arglist>(int n) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Contains</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>a25fd2252df71c9531341ea392d639b06</anchor>
      <arglist>(int n) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Includes</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>a16460bdbc6388e9895591c5c3d396869</anchor>
      <arglist>(Set other) const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>Cardinality</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>a24144feea74d47a81c7c1476b24e8e62</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>SmallestElement</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>a7a026ad4d35f459e5ca13e30cea9ad21</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>Set</type>
      <name>RemoveSmallestElement</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>a1f635310ba40e9118f83662f4839e104</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>ElementRank</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>a5af831720988b31639b578e73d727f4e</anchor>
      <arglist>(int n) const</arglist>
    </member>
    <member kind="function">
      <type>Set</type>
      <name>SmallestSingleton</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>aafdb1b510571bc337d0dee99f4ef7dea</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>SingletonRank</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>aa3639cb771e8dd36bb9092c869cae7c3</anchor>
      <arglist>(Set singleton) const</arglist>
    </member>
    <member kind="function">
      <type>ElementIterator&lt; Set &gt;</type>
      <name>begin</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>ab6cacc30b3640c31b723e42570c73bd4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ElementIterator&lt; Set &gt;</type>
      <name>end</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>a1cfc40ac6c93cea1957a96ea082010ce</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>operator!=</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>a0bdd9a18dc27c98ab0e7599fa35a039a</anchor>
      <arglist>(const Set &amp;other) const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>SmallestElement</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>a06cef107c255364827efeb88eb110e09</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>Cardinality</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>aec3dd7ca92402871bc9f25f2a99c117e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static Set</type>
      <name>FullSet</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>a141358673ffd6a239cff71146df9b133</anchor>
      <arglist>(Integer card)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static Set</type>
      <name>Singleton</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>a4a7e7acabfbd596162ed619fcca5746f</anchor>
      <arglist>(Integer n)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const Integer</type>
      <name>One</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>a1743557f4e470ddb1167fe1393fb67d0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const Integer</type>
      <name>Zero</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>ae9c419f7af4bb71f2ef3d9734e14b02f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>MaxCardinality</name>
      <anchorfile>classoperations__research_1_1Set.html</anchorfile>
      <anchor>acf527d4f2a9de5310586b357f76ef408</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::SetRangeIterator</name>
    <filename>classoperations__research_1_1SetRangeIterator.html</filename>
    <templarg></templarg>
    <member kind="typedef">
      <type>SetRange::SetType</type>
      <name>SetType</name>
      <anchorfile>classoperations__research_1_1SetRangeIterator.html</anchorfile>
      <anchor>ac33a8fa2bd3858cb38ac4aa7027b14f3</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>SetType::IntegerType</type>
      <name>IntegerType</name>
      <anchorfile>classoperations__research_1_1SetRangeIterator.html</anchorfile>
      <anchor>a8b736bbe536c9abc15d84d5fa462a506</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>SetRangeIterator</name>
      <anchorfile>classoperations__research_1_1SetRangeIterator.html</anchorfile>
      <anchor>ab10604e513e69c2e69885026aa4419f6</anchor>
      <arglist>(const SetType set)</arglist>
    </member>
    <member kind="function">
      <type>SetType</type>
      <name>operator *</name>
      <anchorfile>classoperations__research_1_1SetRangeIterator.html</anchorfile>
      <anchor>a91417bb16ed420eba66eeb88cad385a9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>operator!=</name>
      <anchorfile>classoperations__research_1_1SetRangeIterator.html</anchorfile>
      <anchor>ae49d2a420e45ad8370e29906e66572ee</anchor>
      <arglist>(const SetRangeIterator &amp;other) const</arglist>
    </member>
    <member kind="function">
      <type>const SetRangeIterator &amp;</type>
      <name>operator++</name>
      <anchorfile>classoperations__research_1_1SetRangeIterator.html</anchorfile>
      <anchor>a2b617b0eee1cb874ce18d0fbfbcb0857</anchor>
      <arglist>()</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::SetRangeWithCardinality</name>
    <filename>classoperations__research_1_1SetRangeWithCardinality.html</filename>
    <templarg></templarg>
    <member kind="typedef">
      <type>Set</type>
      <name>SetType</name>
      <anchorfile>classoperations__research_1_1SetRangeWithCardinality.html</anchorfile>
      <anchor>aee6525360117d00bf72c1c399cdaab77</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>SetRangeWithCardinality</name>
      <anchorfile>classoperations__research_1_1SetRangeWithCardinality.html</anchorfile>
      <anchor>a361a8278313aa9ba0c4628b0da6590b1</anchor>
      <arglist>(int card, int max_card)</arglist>
    </member>
    <member kind="function">
      <type>SetRangeIterator&lt; SetRangeWithCardinality &gt;</type>
      <name>begin</name>
      <anchorfile>classoperations__research_1_1SetRangeWithCardinality.html</anchorfile>
      <anchor>a03ad5ce85e22b3acb0a25b8c09857b72</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>SetRangeIterator&lt; SetRangeWithCardinality &gt;</type>
      <name>end</name>
      <anchorfile>classoperations__research_1_1SetRangeWithCardinality.html</anchorfile>
      <anchor>a69748fc40a699f2458bf7099c39b5c7d</anchor>
      <arglist>() const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::SimpleMaxFlow</name>
    <filename>classoperations__research_1_1SimpleMaxFlow.html</filename>
    <member kind="enumeration">
      <type></type>
      <name>Status</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>a8a7acde49c4d55f2eb42e2b6869cdb3d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>a8a7acde49c4d55f2eb42e2b6869cdb3da3b60465215ab4363dec64fd313771658</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>POSSIBLE_OVERFLOW</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>a8a7acde49c4d55f2eb42e2b6869cdb3da18a3ff25435f10be68329d4b39de4700</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_INPUT</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>a8a7acde49c4d55f2eb42e2b6869cdb3da2260e76ea3adc21a9cd21f46f232ebbc</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_RESULT</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>a8a7acde49c4d55f2eb42e2b6869cdb3da11ce6c709e3b39a368358f2ee79942d6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>a8a7acde49c4d55f2eb42e2b6869cdb3da3b60465215ab4363dec64fd313771658</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>POSSIBLE_OVERFLOW</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>a8a7acde49c4d55f2eb42e2b6869cdb3da18a3ff25435f10be68329d4b39de4700</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_INPUT</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>a8a7acde49c4d55f2eb42e2b6869cdb3da2260e76ea3adc21a9cd21f46f232ebbc</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_RESULT</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>a8a7acde49c4d55f2eb42e2b6869cdb3da11ce6c709e3b39a368358f2ee79942d6</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>SimpleMaxFlow</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>ada0b88e232c739108184deb35dd54b3f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>ArcIndex</type>
      <name>AddArcWithCapacity</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>a64c1cc700d51eca9eac6bec6542d0bfb</anchor>
      <arglist>(NodeIndex tail, NodeIndex head, FlowQuantity capacity)</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>NumNodes</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>a200b4dbff9102ee1232e05f569fc2427</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndex</type>
      <name>NumArcs</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>a10899eb2ae1b242c4737e4c9e7ecdae5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>Tail</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>aead7fdba72a6048caad9c20150368348</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>Head</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>ab239080405c843687c20d1533b5512be</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>Capacity</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>a86dad6a200cad46b7d53305e2837cfb3</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>Status</type>
      <name>Solve</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>a5ae2ad55f2fd8ce704fe695f835c2ed2</anchor>
      <arglist>(NodeIndex source, NodeIndex sink)</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>OptimalFlow</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>a553204b2dccf205ff9c00c3c589d309b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>Flow</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>aef35310ddd005652bb2b3db14c323ae9</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GetSourceSideMinCut</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>ae83c98d7dcf6a2d76a5d500273255fe6</anchor>
      <arglist>(std::vector&lt; NodeIndex &gt; *result)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GetSinkSideMinCut</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>a693abc3c55e13dbe2f836e562b5a9c0f</anchor>
      <arglist>(std::vector&lt; NodeIndex &gt; *result)</arglist>
    </member>
    <member kind="function">
      <type>FlowModel</type>
      <name>CreateFlowModelOfLastSolve</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>a177f9e6c8a8c603e3cead358da5d1026</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetArcCapacity</name>
      <anchorfile>classoperations__research_1_1SimpleMaxFlow.html</anchorfile>
      <anchor>a57296b653813abdd8d711048e87d1212</anchor>
      <arglist>(ArcIndex arc, FlowQuantity capacity)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::SimpleMinCostFlow</name>
    <filename>classoperations__research_1_1SimpleMinCostFlow.html</filename>
    <base>operations_research::MinCostFlowBase</base>
    <member kind="enumeration">
      <type></type>
      <name>Status</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>NOT_SOLVED</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a44ab1b2f3c5251267bce749661e4e6de</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a7ba4907297a99141ebd2bc890d5a591c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>FEASIBLE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457ab78b8e2734f0e777e79b8e5f93948101</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INFEASIBLE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457af323941c683086f71ce1e7cae7a0a1b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UNBALANCED</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457ad79843de8fcdec97ba2ce9b83d6cf3c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_RESULT</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a030d97dcea9960e3d1f127024b0e2168</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_COST_RANGE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a41523b94852c3cb55c86ce0e291e3719</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>NOT_SOLVED</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a44ab1b2f3c5251267bce749661e4e6de</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a7ba4907297a99141ebd2bc890d5a591c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>FEASIBLE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457ab78b8e2734f0e777e79b8e5f93948101</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INFEASIBLE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457af323941c683086f71ce1e7cae7a0a1b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UNBALANCED</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457ad79843de8fcdec97ba2ce9b83d6cf3c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_RESULT</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a030d97dcea9960e3d1f127024b0e2168</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BAD_COST_RANGE</name>
      <anchorfile>classoperations__research_1_1MinCostFlowBase.html</anchorfile>
      <anchor>ae5cad0c7d9c3eddbc7c3aefbee060457a41523b94852c3cb55c86ce0e291e3719</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>SimpleMinCostFlow</name>
      <anchorfile>classoperations__research_1_1SimpleMinCostFlow.html</anchorfile>
      <anchor>a4f442c73b242e40a793d529df4e44d07</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>ArcIndex</type>
      <name>AddArcWithCapacityAndUnitCost</name>
      <anchorfile>classoperations__research_1_1SimpleMinCostFlow.html</anchorfile>
      <anchor>a632cc069a03f2d9873715467c09a3810</anchor>
      <arglist>(NodeIndex tail, NodeIndex head, FlowQuantity capacity, CostValue unit_cost)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetNodeSupply</name>
      <anchorfile>classoperations__research_1_1SimpleMinCostFlow.html</anchorfile>
      <anchor>a7410b695f7e06dd3de7e6920bc28a3ac</anchor>
      <arglist>(NodeIndex node, FlowQuantity supply)</arglist>
    </member>
    <member kind="function">
      <type>Status</type>
      <name>Solve</name>
      <anchorfile>classoperations__research_1_1SimpleMinCostFlow.html</anchorfile>
      <anchor>af4eeb2bca251bf02604bcbea5c304421</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>Status</type>
      <name>SolveMaxFlowWithMinCost</name>
      <anchorfile>classoperations__research_1_1SimpleMinCostFlow.html</anchorfile>
      <anchor>a443e6d29a83127237379abb898a26195</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>CostValue</type>
      <name>OptimalCost</name>
      <anchorfile>classoperations__research_1_1SimpleMinCostFlow.html</anchorfile>
      <anchor>aaec6a152673412884a7c0d1b59991a70</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>MaximumFlow</name>
      <anchorfile>classoperations__research_1_1SimpleMinCostFlow.html</anchorfile>
      <anchor>a49270938ce4794ae76f8a25c23ddef65</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>Flow</name>
      <anchorfile>classoperations__research_1_1SimpleMinCostFlow.html</anchorfile>
      <anchor>ac2a7f803ea909ac1f3225f2607f1f33b</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>NumNodes</name>
      <anchorfile>classoperations__research_1_1SimpleMinCostFlow.html</anchorfile>
      <anchor>ac80325e043df267da7b4f9e0799bba54</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndex</type>
      <name>NumArcs</name>
      <anchorfile>classoperations__research_1_1SimpleMinCostFlow.html</anchorfile>
      <anchor>aad61644a352f9eccbce03deffa6f6313</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>Tail</name>
      <anchorfile>classoperations__research_1_1SimpleMinCostFlow.html</anchorfile>
      <anchor>a492bdbfb9459995c0f0c41c1cbd0b1bd</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndex</type>
      <name>Head</name>
      <anchorfile>classoperations__research_1_1SimpleMinCostFlow.html</anchorfile>
      <anchor>a4792c687fa3c6fc576f0495e542ee6e5</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>Capacity</name>
      <anchorfile>classoperations__research_1_1SimpleMinCostFlow.html</anchorfile>
      <anchor>a6f1a6fedf4eff7e8b015564e0a4fc786</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
    <member kind="function">
      <type>FlowQuantity</type>
      <name>Supply</name>
      <anchorfile>classoperations__research_1_1SimpleMinCostFlow.html</anchorfile>
      <anchor>a034e472c35933cd3b6c6fc02985f54f7</anchor>
      <arglist>(NodeIndex node) const</arglist>
    </member>
    <member kind="function">
      <type>CostValue</type>
      <name>UnitCost</name>
      <anchorfile>classoperations__research_1_1SimpleMinCostFlow.html</anchorfile>
      <anchor>a98af872df7cb24c5d3074cc44a16f9a7</anchor>
      <arglist>(ArcIndex arc) const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::StarGraphBase</name>
    <filename>classoperations__research_1_1StarGraphBase.html</filename>
    <templarg>NodeIndexType</templarg>
    <templarg>ArcIndexType</templarg>
    <templarg>DerivedGraph</templarg>
    <class kind="class">operations_research::StarGraphBase::ArcIterator</class>
    <class kind="class">operations_research::StarGraphBase::NodeIterator</class>
    <class kind="class">operations_research::StarGraphBase::OutgoingArcIterator</class>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>num_nodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa8a10d242f7088bfdee282e1246e00b8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>num_arcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ac0ae30f08a54ea67bf295b446157aed3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>end_node_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a03cebcf1fdf356a8217ae2879100d324</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>end_arc_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a68a1d993a6371e6fbf6a8ffe5c944a45</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>max_num_nodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a51a5033049b63ee5ce35b531b8474f0a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_num_arcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a8aad937029f57b70efe059b908241685</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>max_end_node_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a6aecaff5cd6eea0a7c93de644b8defc6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_end_arc_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7e2435063a7df2859f5b589852a8d4d3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad69ccdbdbceba88bf784e2add4964b5b</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>LookUpArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad24fe35c2e3798137b42e1bc1c35e587</anchor>
      <arglist>(const NodeIndexType tail, const NodeIndexType head) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Head</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aac64d5e43f89177ab46f7ef763bb3c64</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>NodeDebugString</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a5d2e4d934a8eb290a6298480d48faa52</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>ArcDebugString</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a30a3055f6669fd7739fd7acea16c4571</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kNilNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a2c3f7aa31326efb573187d321679cba6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kNilArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7174acc6507b365096d7122ef430b1dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kFirstNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a2c2ac42ee1d86e253e0a85f3f1321824</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kFirstArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a3693e89df768c2f25a31d4b4ee64ee3a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kMaxNumNodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aebc1c53cc0d7242f47f0cf971a105e20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kMaxNumArcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a043c924d609639e1cc5bea7d4a5a7fd3</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type></type>
      <name>StarGraphBase</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad1ce86785b174a8e247332d8043dbde0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type></type>
      <name>~StarGraphBase</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ac513d8f4923fdd3e75eb7db1cadaade7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndexType</type>
      <name>StartNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7cf8fa1e0a3abc8179da6309a86097f5</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>StartArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa58e05a2a30eb0b2b38cc366bba1d1ae</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndexType</type>
      <name>NextNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a8038f289c7e5558d1b4d264677c0f4be</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>NextArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a30864ddfe33af3fe52a64d35bf298503</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>FirstOutgoingArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ae83c0a0706d7c6bf85e08719e818c813</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>max_num_nodes_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a539adfb6a1003d4270e0f2c8fe6705b9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>max_num_arcs_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa219b1703090300ed69dc9f5f6f54ded</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>num_nodes_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad9877f4f6b0822e2cb6aa9c4fe60ceb9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>num_arcs_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a76da9f41f6215acb4ec4dd8da00c0e28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; NodeIndexType &gt;</type>
      <name>head_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a1d032dcb17aa589d9a386fe04499654b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; ArcIndexType &gt;</type>
      <name>first_incident_arc_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>af1a0166faa8ccde14511e9fc547febac</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>StarGraphBase&lt; NodeIndexType, ArcIndexType, EbertGraph&lt; NodeIndexType, ArcIndexType &gt; &gt;</name>
    <filename>classoperations__research_1_1StarGraphBase.html</filename>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>num_nodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa8a10d242f7088bfdee282e1246e00b8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>num_arcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ac0ae30f08a54ea67bf295b446157aed3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>end_node_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a03cebcf1fdf356a8217ae2879100d324</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>end_arc_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a68a1d993a6371e6fbf6a8ffe5c944a45</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>max_num_nodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a51a5033049b63ee5ce35b531b8474f0a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_num_arcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a8aad937029f57b70efe059b908241685</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>max_end_node_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a6aecaff5cd6eea0a7c93de644b8defc6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_end_arc_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7e2435063a7df2859f5b589852a8d4d3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad69ccdbdbceba88bf784e2add4964b5b</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>LookUpArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad24fe35c2e3798137b42e1bc1c35e587</anchor>
      <arglist>(const NodeIndexType tail, const NodeIndexType head) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Head</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aac64d5e43f89177ab46f7ef763bb3c64</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>NodeDebugString</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a5d2e4d934a8eb290a6298480d48faa52</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>ArcDebugString</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a30a3055f6669fd7739fd7acea16c4571</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kNilNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a2c3f7aa31326efb573187d321679cba6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kNilArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7174acc6507b365096d7122ef430b1dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kFirstNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a2c2ac42ee1d86e253e0a85f3f1321824</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kFirstArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a3693e89df768c2f25a31d4b4ee64ee3a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kMaxNumNodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aebc1c53cc0d7242f47f0cf971a105e20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kMaxNumArcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a043c924d609639e1cc5bea7d4a5a7fd3</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type></type>
      <name>StarGraphBase</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad1ce86785b174a8e247332d8043dbde0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type></type>
      <name>~StarGraphBase</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ac513d8f4923fdd3e75eb7db1cadaade7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndexType</type>
      <name>StartNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7cf8fa1e0a3abc8179da6309a86097f5</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>StartArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa58e05a2a30eb0b2b38cc366bba1d1ae</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndexType</type>
      <name>NextNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a8038f289c7e5558d1b4d264677c0f4be</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>NextArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a30864ddfe33af3fe52a64d35bf298503</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>FirstOutgoingArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ae83c0a0706d7c6bf85e08719e818c813</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>max_num_nodes_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a539adfb6a1003d4270e0f2c8fe6705b9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>max_num_arcs_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa219b1703090300ed69dc9f5f6f54ded</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>num_nodes_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad9877f4f6b0822e2cb6aa9c4fe60ceb9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>num_arcs_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a76da9f41f6215acb4ec4dd8da00c0e28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; NodeIndexType &gt;</type>
      <name>head_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a1d032dcb17aa589d9a386fe04499654b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; ArcIndexType &gt;</type>
      <name>first_incident_arc_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>af1a0166faa8ccde14511e9fc547febac</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>StarGraphBase&lt; NodeIndexType, ArcIndexType, ForwardEbertGraph&lt; NodeIndexType, ArcIndexType &gt; &gt;</name>
    <filename>classoperations__research_1_1StarGraphBase.html</filename>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>num_nodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa8a10d242f7088bfdee282e1246e00b8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>num_arcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ac0ae30f08a54ea67bf295b446157aed3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>end_node_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a03cebcf1fdf356a8217ae2879100d324</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>end_arc_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a68a1d993a6371e6fbf6a8ffe5c944a45</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>max_num_nodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a51a5033049b63ee5ce35b531b8474f0a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_num_arcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a8aad937029f57b70efe059b908241685</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>max_end_node_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a6aecaff5cd6eea0a7c93de644b8defc6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_end_arc_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7e2435063a7df2859f5b589852a8d4d3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad69ccdbdbceba88bf784e2add4964b5b</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>LookUpArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad24fe35c2e3798137b42e1bc1c35e587</anchor>
      <arglist>(const NodeIndexType tail, const NodeIndexType head) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Head</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aac64d5e43f89177ab46f7ef763bb3c64</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>NodeDebugString</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a5d2e4d934a8eb290a6298480d48faa52</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>ArcDebugString</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a30a3055f6669fd7739fd7acea16c4571</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kNilNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a2c3f7aa31326efb573187d321679cba6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kNilArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7174acc6507b365096d7122ef430b1dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kFirstNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a2c2ac42ee1d86e253e0a85f3f1321824</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kFirstArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a3693e89df768c2f25a31d4b4ee64ee3a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kMaxNumNodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aebc1c53cc0d7242f47f0cf971a105e20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kMaxNumArcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a043c924d609639e1cc5bea7d4a5a7fd3</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type></type>
      <name>StarGraphBase</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad1ce86785b174a8e247332d8043dbde0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type></type>
      <name>~StarGraphBase</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ac513d8f4923fdd3e75eb7db1cadaade7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndexType</type>
      <name>StartNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7cf8fa1e0a3abc8179da6309a86097f5</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>StartArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa58e05a2a30eb0b2b38cc366bba1d1ae</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndexType</type>
      <name>NextNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a8038f289c7e5558d1b4d264677c0f4be</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>NextArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a30864ddfe33af3fe52a64d35bf298503</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>FirstOutgoingArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ae83c0a0706d7c6bf85e08719e818c813</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>max_num_nodes_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a539adfb6a1003d4270e0f2c8fe6705b9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>max_num_arcs_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa219b1703090300ed69dc9f5f6f54ded</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>num_nodes_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad9877f4f6b0822e2cb6aa9c4fe60ceb9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>num_arcs_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a76da9f41f6215acb4ec4dd8da00c0e28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; NodeIndexType &gt;</type>
      <name>head_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a1d032dcb17aa589d9a386fe04499654b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; ArcIndexType &gt;</type>
      <name>first_incident_arc_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>af1a0166faa8ccde14511e9fc547febac</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>StarGraphBase&lt; NodeIndexType, ArcIndexType, ForwardStaticGraph&lt; NodeIndexType, ArcIndexType &gt; &gt;</name>
    <filename>classoperations__research_1_1StarGraphBase.html</filename>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>num_nodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa8a10d242f7088bfdee282e1246e00b8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>num_arcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ac0ae30f08a54ea67bf295b446157aed3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>end_node_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a03cebcf1fdf356a8217ae2879100d324</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>end_arc_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a68a1d993a6371e6fbf6a8ffe5c944a45</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>max_num_nodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a51a5033049b63ee5ce35b531b8474f0a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_num_arcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a8aad937029f57b70efe059b908241685</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>max_end_node_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a6aecaff5cd6eea0a7c93de644b8defc6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_end_arc_index</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7e2435063a7df2859f5b589852a8d4d3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad69ccdbdbceba88bf784e2add4964b5b</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>LookUpArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad24fe35c2e3798137b42e1bc1c35e587</anchor>
      <arglist>(const NodeIndexType tail, const NodeIndexType head) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Head</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aac64d5e43f89177ab46f7ef763bb3c64</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>NodeDebugString</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a5d2e4d934a8eb290a6298480d48faa52</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>ArcDebugString</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a30a3055f6669fd7739fd7acea16c4571</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kNilNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a2c3f7aa31326efb573187d321679cba6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kNilArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7174acc6507b365096d7122ef430b1dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kFirstNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a2c2ac42ee1d86e253e0a85f3f1321824</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kFirstArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a3693e89df768c2f25a31d4b4ee64ee3a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kMaxNumNodes</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aebc1c53cc0d7242f47f0cf971a105e20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kMaxNumArcs</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a043c924d609639e1cc5bea7d4a5a7fd3</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type></type>
      <name>StarGraphBase</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad1ce86785b174a8e247332d8043dbde0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type></type>
      <name>~StarGraphBase</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ac513d8f4923fdd3e75eb7db1cadaade7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndexType</type>
      <name>StartNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a7cf8fa1e0a3abc8179da6309a86097f5</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>StartArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa58e05a2a30eb0b2b38cc366bba1d1ae</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>NodeIndexType</type>
      <name>NextNode</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a8038f289c7e5558d1b4d264677c0f4be</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>NextArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a30864ddfe33af3fe52a64d35bf298503</anchor>
      <arglist>(const ArcIndexType arc) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>ArcIndexType</type>
      <name>FirstOutgoingArc</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ae83c0a0706d7c6bf85e08719e818c813</anchor>
      <arglist>(const NodeIndexType node) const</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>max_num_nodes_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a539adfb6a1003d4270e0f2c8fe6705b9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>max_num_arcs_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>aa219b1703090300ed69dc9f5f6f54ded</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>num_nodes_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>ad9877f4f6b0822e2cb6aa9c4fe60ceb9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>num_arcs_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a76da9f41f6215acb4ec4dd8da00c0e28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; NodeIndexType &gt;</type>
      <name>head_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>a1d032dcb17aa589d9a386fe04499654b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ZVector&lt; ArcIndexType &gt;</type>
      <name>first_incident_arc_</name>
      <anchorfile>classoperations__research_1_1StarGraphBase.html</anchorfile>
      <anchor>af1a0166faa8ccde14511e9fc547febac</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::StaticGraph</name>
    <filename>classutil_1_1StaticGraph.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <base>BaseGraph&lt; NodeIndexType, ArcIndexType, false &gt;</base>
    <class kind="class">util::StaticGraph::OutgoingArcIterator</class>
    <member kind="typedef">
      <type>NodeIndexType</type>
      <name>NodeIndex</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>adb271cf4bcf2de5b5bbe300d7054af29</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ArcIndexType</type>
      <name>ArcIndex</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a0eba6e5899924388644dfa2258ae8929</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>StaticGraph</name>
      <anchorfile>classutil_1_1StaticGraph.html</anchorfile>
      <anchor>afe7f293d20a6f38c859af7c51ab1b105</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>StaticGraph</name>
      <anchorfile>classutil_1_1StaticGraph.html</anchorfile>
      <anchor>abb5850c4145abf3dad76d25668368505</anchor>
      <arglist>(NodeIndexType num_nodes, ArcIndexType arc_capacity)</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Head</name>
      <anchorfile>classutil_1_1StaticGraph.html</anchorfile>
      <anchor>aefeec121033f3271585a059f0b5fbd3c</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>Tail</name>
      <anchorfile>classutil_1_1StaticGraph.html</anchorfile>
      <anchor>a9615f3c590e39f40ca5158bd4f37b5b4</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>OutDegree</name>
      <anchorfile>classutil_1_1StaticGraph.html</anchorfile>
      <anchor>a7241e6e63df39df439a5ebb4fe28773e</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OutgoingArcIterator &gt;</type>
      <name>OutgoingArcs</name>
      <anchorfile>classutil_1_1StaticGraph.html</anchorfile>
      <anchor>aa7eeb525aab90ef2bab82eb69687f600</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; OutgoingArcIterator &gt;</type>
      <name>OutgoingArcsStartingFrom</name>
      <anchorfile>classutil_1_1StaticGraph.html</anchorfile>
      <anchor>aa6626536ee02fe83aa26ff7bbcfc8ae7</anchor>
      <arglist>(NodeIndexType node, ArcIndexType from) const</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; NodeIndexType const  * &gt;</type>
      <name>operator[]</name>
      <anchorfile>classutil_1_1StaticGraph.html</anchorfile>
      <anchor>a160bf25732e2b1a19e8bf6d853014070</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ReserveNodes</name>
      <anchorfile>classutil_1_1StaticGraph.html</anchorfile>
      <anchor>a9b1c856f35140cb8902b94374a43d368</anchor>
      <arglist>(NodeIndexType bound) override</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ReserveArcs</name>
      <anchorfile>classutil_1_1StaticGraph.html</anchorfile>
      <anchor>a98f11aaa1013df49976fcb5433538ff5</anchor>
      <arglist>(ArcIndexType bound) override</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddNode</name>
      <anchorfile>classutil_1_1StaticGraph.html</anchorfile>
      <anchor>aa2a879f538b488698183a860bdb88596</anchor>
      <arglist>(NodeIndexType node)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>AddArc</name>
      <anchorfile>classutil_1_1StaticGraph.html</anchorfile>
      <anchor>a10d877b38553e9d2d0ce6fcfc4427df4</anchor>
      <arglist>(NodeIndexType tail, NodeIndexType head)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Build</name>
      <anchorfile>classutil_1_1StaticGraph.html</anchorfile>
      <anchor>a0b8444bcee7138b5702880a882d29283</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Build</name>
      <anchorfile>classutil_1_1StaticGraph.html</anchorfile>
      <anchor>abfa63fa219b3d8c73a324977312182eb</anchor>
      <arglist>(std::vector&lt; ArcIndexType &gt; *permutation)</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>num_nodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a0f551c921fa0b5aaa334a6e36f61db4a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>num_arcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aefb468e4d4a3128c91b3bad9f5b314c8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; NodeIndex &gt;</type>
      <name>AllNodes</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a32ba4a5ca9a4b89f750eb2dc56518b02</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerRange&lt; ArcIndex &gt;</type>
      <name>AllForwardArcs</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>abcfd7c21143e5ed38573c0dd60826dd8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsNodeValid</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>abf853acea86e01356f53055f77661770</anchor>
      <arglist>(NodeIndexType node) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsArcValid</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a2097ccda3b1ad27e2c82166979018bda</anchor>
      <arglist>(ArcIndexType arc) const</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType</type>
      <name>node_capacity</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a463d57480c9563a7a707c5d0928c9946</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>arc_capacity</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a92ffd852b2ab2e5241f9832e71a2de71</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Reserve</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab731ca5c638e7b1b0a2c459c94a90f55</anchor>
      <arglist>(NodeIndexType node_capacity, ArcIndexType arc_capacity)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>FreezeCapacities</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aa3bd051d1e141b09dda17aa9b5f24f69</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GroupForwardArcsByFunctor</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a07214b96597069d781e27b1dd17ef83e</anchor>
      <arglist>(const A &amp;a, B *b)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType</type>
      <name>max_end_arc_index</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a346b8a5811f0e287e1ebce2de2c1ad28</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const NodeIndexType</type>
      <name>kNilNode</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ab1292c82a3f43be3bd57b63a05fe0214</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ArcIndexType</type>
      <name>kNilArc</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ad5b77846f77c2771e840820812ad5521</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>ComputeCumulativeSum</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>ac47030fcb75a3220f8cf9ed79697056e</anchor>
      <arglist>(std::vector&lt; ArcIndexType &gt; *v)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>BuildStartAndForwardHead</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a63fd586eed6c345866317e2f0faf377e</anchor>
      <arglist>(SVector&lt; NodeIndexType &gt; *head, std::vector&lt; ArcIndexType &gt; *start, std::vector&lt; ArcIndexType &gt; *permutation)</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>num_nodes_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a473db46afec1eabf0762411830dee30f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>NodeIndexType</type>
      <name>node_capacity_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a424bd34a9767e7edeaf3a60ecd3cb000</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>num_arcs_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>af16f99d41856a7b22ae8a226ef09abff</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ArcIndexType</type>
      <name>arc_capacity_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>a895e891d1ad52ce3efcfeb7ba11194c7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>const_capacities_</name>
      <anchorfile>classutil_1_1BaseGraph.html</anchorfile>
      <anchor>aa980e5526b9ded17a83928fc339c71e4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::SVector</name>
    <filename>classutil_1_1SVector.html</filename>
    <templarg>T</templarg>
    <member kind="function">
      <type></type>
      <name>SVector</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>ac342b96ebd12c4cc505567c0d5342fd6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>~SVector</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>aa05f6afa9ff06e13079eff039632f0cb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>SVector</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a4ca50715f87be0dd9925dd46e2810436</anchor>
      <arglist>(const SVector &amp;other)</arglist>
    </member>
    <member kind="function">
      <type>SVector &amp;</type>
      <name>operator=</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>afc485e5858e15ce8ec376ca7640ede1e</anchor>
      <arglist>(const SVector &amp;other)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>SVector</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>abba34108f60eb020d0cc94ea8bd24c8f</anchor>
      <arglist>(SVector &amp;&amp;other)</arglist>
    </member>
    <member kind="function">
      <type>SVector &amp;</type>
      <name>operator=</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a9e1927755201f418ac04ac1ad4b4660d</anchor>
      <arglist>(SVector &amp;&amp;other)</arglist>
    </member>
    <member kind="function">
      <type>T &amp;</type>
      <name>operator[]</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a6270e7d8614fca867c7f935b7a3e9cd3</anchor>
      <arglist>(int n)</arglist>
    </member>
    <member kind="function">
      <type>const T &amp;</type>
      <name>operator[]</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a01c346b7779b313b6ff1099a10946ed0</anchor>
      <arglist>(int n) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>resize</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>af0059304ee5c601207e97b1c742d5e84</anchor>
      <arglist>(int n)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a06ed1003be01209763bb15e923399fc7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>T *</type>
      <name>data</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a25fc0f822d7e2a5eedab230ed56be07f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>swap</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>acee1cc8d243acbdad8d30f97c5d54a5f</anchor>
      <arglist>(SVector&lt; T &gt; &amp;x)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>reserve</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a287f9791ba2e68e137d53fc038bbe432</anchor>
      <arglist>(int n)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>grow</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>ac34dbebcbd795b04b51dbfd96292d671</anchor>
      <arglist>(const T &amp;left=T(), const T &amp;right=T())</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>size</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>afaad0723c70ab98c88d4a0441e1f98e4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>capacity</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a5d3d8b4f10e9d40e85fbd54d04c33611</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>max_size</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>add718a46fe9ba1db673f69b2c02ec2a2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_and_dealloc</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a36d88784d1f797dcf76845c47d74a4da</anchor>
      <arglist>()</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>SVector&lt; ArcIndexType &gt;</name>
    <filename>classutil_1_1SVector.html</filename>
    <member kind="function">
      <type></type>
      <name>SVector</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>ac342b96ebd12c4cc505567c0d5342fd6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>SVector</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a4ca50715f87be0dd9925dd46e2810436</anchor>
      <arglist>(const SVector &amp;other)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>SVector</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>abba34108f60eb020d0cc94ea8bd24c8f</anchor>
      <arglist>(SVector &amp;&amp;other)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>~SVector</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>aa05f6afa9ff06e13079eff039632f0cb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>SVector &amp;</type>
      <name>operator=</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>afc485e5858e15ce8ec376ca7640ede1e</anchor>
      <arglist>(const SVector &amp;other)</arglist>
    </member>
    <member kind="function">
      <type>SVector &amp;</type>
      <name>operator=</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a9e1927755201f418ac04ac1ad4b4660d</anchor>
      <arglist>(SVector &amp;&amp;other)</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType &amp;</type>
      <name>operator[]</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a6270e7d8614fca867c7f935b7a3e9cd3</anchor>
      <arglist>(int n)</arglist>
    </member>
    <member kind="function">
      <type>const ArcIndexType &amp;</type>
      <name>operator[]</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a01c346b7779b313b6ff1099a10946ed0</anchor>
      <arglist>(int n) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>resize</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>af0059304ee5c601207e97b1c742d5e84</anchor>
      <arglist>(int n)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a06ed1003be01209763bb15e923399fc7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>ArcIndexType *</type>
      <name>data</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a25fc0f822d7e2a5eedab230ed56be07f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>swap</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>acee1cc8d243acbdad8d30f97c5d54a5f</anchor>
      <arglist>(SVector&lt; ArcIndexType &gt; &amp;x)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>reserve</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a287f9791ba2e68e137d53fc038bbe432</anchor>
      <arglist>(int n)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>grow</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>ac34dbebcbd795b04b51dbfd96292d671</anchor>
      <arglist>(const ArcIndexType &amp;left=ArcIndexType(), const ArcIndexType &amp;right=ArcIndexType())</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>size</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>afaad0723c70ab98c88d4a0441e1f98e4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>capacity</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a5d3d8b4f10e9d40e85fbd54d04c33611</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>max_size</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>add718a46fe9ba1db673f69b2c02ec2a2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_and_dealloc</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a36d88784d1f797dcf76845c47d74a4da</anchor>
      <arglist>()</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>SVector&lt; NodeIndexType &gt;</name>
    <filename>classutil_1_1SVector.html</filename>
    <member kind="function">
      <type></type>
      <name>SVector</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>ac342b96ebd12c4cc505567c0d5342fd6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>SVector</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a4ca50715f87be0dd9925dd46e2810436</anchor>
      <arglist>(const SVector &amp;other)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>SVector</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>abba34108f60eb020d0cc94ea8bd24c8f</anchor>
      <arglist>(SVector &amp;&amp;other)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>~SVector</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>aa05f6afa9ff06e13079eff039632f0cb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>SVector &amp;</type>
      <name>operator=</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>afc485e5858e15ce8ec376ca7640ede1e</anchor>
      <arglist>(const SVector &amp;other)</arglist>
    </member>
    <member kind="function">
      <type>SVector &amp;</type>
      <name>operator=</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a9e1927755201f418ac04ac1ad4b4660d</anchor>
      <arglist>(SVector &amp;&amp;other)</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType &amp;</type>
      <name>operator[]</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a6270e7d8614fca867c7f935b7a3e9cd3</anchor>
      <arglist>(int n)</arglist>
    </member>
    <member kind="function">
      <type>const NodeIndexType &amp;</type>
      <name>operator[]</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a01c346b7779b313b6ff1099a10946ed0</anchor>
      <arglist>(int n) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>resize</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>af0059304ee5c601207e97b1c742d5e84</anchor>
      <arglist>(int n)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a06ed1003be01209763bb15e923399fc7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>NodeIndexType *</type>
      <name>data</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a25fc0f822d7e2a5eedab230ed56be07f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>swap</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>acee1cc8d243acbdad8d30f97c5d54a5f</anchor>
      <arglist>(SVector&lt; NodeIndexType &gt; &amp;x)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>reserve</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a287f9791ba2e68e137d53fc038bbe432</anchor>
      <arglist>(int n)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>grow</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>ac34dbebcbd795b04b51dbfd96292d671</anchor>
      <arglist>(const NodeIndexType &amp;left=NodeIndexType(), const NodeIndexType &amp;right=NodeIndexType())</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>size</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>afaad0723c70ab98c88d4a0441e1f98e4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>capacity</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a5d3d8b4f10e9d40e85fbd54d04c33611</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>max_size</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>add718a46fe9ba1db673f69b2c02ec2a2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_and_dealloc</name>
      <anchorfile>classutil_1_1SVector.html</anchorfile>
      <anchor>a36d88784d1f797dcf76845c47d74a4da</anchor>
      <arglist>()</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::TailArrayManager</name>
    <filename>classoperations__research_1_1TailArrayManager.html</filename>
    <templarg>GraphType</templarg>
    <member kind="function">
      <type></type>
      <name>TailArrayManager</name>
      <anchorfile>classoperations__research_1_1TailArrayManager.html</anchorfile>
      <anchor>af2a2857c30d4d6ce5d0fb9cb68ba440c</anchor>
      <arglist>(GraphType *g)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>BuildTailArrayFromAdjacencyListsIfForwardGraph</name>
      <anchorfile>classoperations__research_1_1TailArrayManager.html</anchorfile>
      <anchor>a8d3f759c1000ddd460ca266933374f0d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ReleaseTailArrayIfForwardGraph</name>
      <anchorfile>classoperations__research_1_1TailArrayManager.html</anchorfile>
      <anchor>ac1ef6f2392846180bc3c98f97a583906</anchor>
      <arglist>() const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>util::UndirectedAdjacencyListsOfDirectedGraph</name>
    <filename>classutil_1_1UndirectedAdjacencyListsOfDirectedGraph.html</filename>
    <templarg></templarg>
    <class kind="class">util::UndirectedAdjacencyListsOfDirectedGraph::AdjacencyListIterator</class>
    <member kind="typedef">
      <type>Graph::OutgoingOrOppositeIncomingArcIterator</type>
      <name>ArcIterator</name>
      <anchorfile>classutil_1_1UndirectedAdjacencyListsOfDirectedGraph.html</anchorfile>
      <anchor>a6402dd9e7851af22b907c0af3e240dce</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>UndirectedAdjacencyListsOfDirectedGraph</name>
      <anchorfile>classutil_1_1UndirectedAdjacencyListsOfDirectedGraph.html</anchorfile>
      <anchor>aa3254dffa224cf75efab4e3b034b39d8</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>BeginEndWrapper&lt; AdjacencyListIterator &gt;</type>
      <name>operator[]</name>
      <anchorfile>classutil_1_1UndirectedAdjacencyListsOfDirectedGraph.html</anchorfile>
      <anchor>a8d68da65b93ce8e85c66d431858897bb</anchor>
      <arglist>(int node) const</arglist>
    </member>
  </compound>
  <compound kind="namespace">
    <name>operations_research</name>
    <filename>namespaceoperations__research.html</filename>
    <class kind="class">operations_research::AnnotatedGraphBuildManager</class>
    <class kind="class">operations_research::ArcFunctorOrderingByTailAndHead</class>
    <class kind="class">operations_research::ArcIndexOrderingByTailNode</class>
    <class kind="class">operations_research::BronKerboschAlgorithm</class>
    <class kind="class">operations_research::ChristofidesPathSolver</class>
    <class kind="class">operations_research::ConnectedComponents</class>
    <class kind="class">operations_research::CostValueCycleHandler</class>
    <class kind="class">operations_research::EbertGraph</class>
    <class kind="class">operations_research::EbertGraphBase</class>
    <class kind="class">operations_research::ElementIterator</class>
    <class kind="class">operations_research::ForwardEbertGraph</class>
    <class kind="class">operations_research::ForwardStaticGraph</class>
    <class kind="class">operations_research::GenericMaxFlow</class>
    <class kind="class">operations_research::GenericMinCostFlow</class>
    <class kind="struct">operations_research::graph_traits</class>
    <class kind="struct">operations_research::graph_traits&lt; ForwardEbertGraph&lt; NodeIndexType, ArcIndexType &gt; &gt;</class>
    <class kind="struct">operations_research::graph_traits&lt; ForwardStaticGraph&lt; NodeIndexType, ArcIndexType &gt; &gt;</class>
    <class kind="struct">operations_research::Graphs</class>
    <class kind="struct">operations_research::Graphs&lt; operations_research::StarGraph &gt;</class>
    <class kind="class">operations_research::HamiltonianPathSolver</class>
    <class kind="class">operations_research::LatticeMemoryManager</class>
    <class kind="class">operations_research::LinearSumAssignment</class>
    <class kind="class">operations_research::MaxFlow</class>
    <class kind="class">operations_research::MaxFlowStatusClass</class>
    <class kind="class">operations_research::MinCostFlow</class>
    <class kind="class">operations_research::MinCostFlowBase</class>
    <class kind="class">operations_research::PermutationIndexComparisonByArcHead</class>
    <class kind="class">operations_research::PriorityQueueWithRestrictedPush</class>
    <class kind="class">operations_research::PruningHamiltonianSolver</class>
    <class kind="class">operations_research::Set</class>
    <class kind="class">operations_research::SetRangeIterator</class>
    <class kind="class">operations_research::SetRangeWithCardinality</class>
    <class kind="class">operations_research::SimpleMaxFlow</class>
    <class kind="class">operations_research::SimpleMinCostFlow</class>
    <class kind="class">operations_research::StarGraphBase</class>
    <class kind="class">operations_research::TailArrayManager</class>
    <member kind="typedef">
      <type>int32</type>
      <name>NodeIndex</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a0e629e35bfa311b31dd7f5065eb834bb</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>int32</type>
      <name>ArcIndex</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a31d858394c5eed1fa21edc3da47047c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>int64</type>
      <name>FlowQuantity</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a5841ff601ab08548afb15c45b2245de7</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>int64</type>
      <name>CostValue</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>aa7950685633ee869aa9471b2ec5fbcfa</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>EbertGraph&lt; NodeIndex, ArcIndex &gt;</type>
      <name>StarGraph</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ae39f15b318a3cba17b1e60e6da51c0d4</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ForwardEbertGraph&lt; NodeIndex, ArcIndex &gt;</type>
      <name>ForwardStarGraph</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a652af62fa5f211aa0c54d7994ca1c504</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ForwardStaticGraph&lt; NodeIndex, ArcIndex &gt;</type>
      <name>ForwardStarStaticGraph</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ac7440a08c859325694df19d4d4aee95c</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ZVector&lt; NodeIndex &gt;</type>
      <name>NodeIndexArray</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a389e5320fb5bcd0fb99d894488f9820b</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ZVector&lt; ArcIndex &gt;</type>
      <name>ArcIndexArray</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>aa79bf252fa6483cd33cbf95170353fb0</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ZVector&lt; FlowQuantity &gt;</type>
      <name>QuantityArray</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a7d4fc0319cb4e28ec175fc9163775a6e</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>ZVector&lt; CostValue &gt;</type>
      <name>CostArray</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>afdee62ecefa0520e530c18a55b083e6d</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>int</type>
      <name>PathNodeIndex</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a09767b3634289e432c3ce1d7c649666a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>CliqueResponse</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>af2d89e69d073dc3036a6de24710b416f</anchor>
      <arglist></arglist>
      <enumvalue file="namespaceoperations__research.html" anchor="af2d89e69d073dc3036a6de24710b416fa2f453cfe638e57e27bb0c9512436111e">CONTINUE</enumvalue>
      <enumvalue file="namespaceoperations__research.html" anchor="af2d89e69d073dc3036a6de24710b416fa615a46af313786fc4e349f34118be111">STOP</enumvalue>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>BronKerboschAlgorithmStatus</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a708cf34b342e7d2ed89a3b73dbec4eae</anchor>
      <arglist></arglist>
      <enumvalue file="namespaceoperations__research.html" anchor="a708cf34b342e7d2ed89a3b73dbec4eaea8f7afecbc8fbc4cd0f50a57d1172482e">COMPLETED</enumvalue>
      <enumvalue file="namespaceoperations__research.html" anchor="a708cf34b342e7d2ed89a3b73dbec4eaea658f2cadfdf09b6046246e9314f7cd43">INTERRUPTED</enumvalue>
    </member>
    <member kind="function">
      <type>void</type>
      <name>FindCliques</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a509097448ff5705cf3e64d889362bdec</anchor>
      <arglist>(std::function&lt; bool(int, int)&gt; graph, int node_count, std::function&lt; bool(const std::vector&lt; int &gt; &amp;)&gt; callback)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CoverArcsByCliques</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>afe4b5a6c0e4019314f288e3f4307c114</anchor>
      <arglist>(std::function&lt; bool(int, int)&gt; graph, int node_count, std::function&lt; bool(const std::vector&lt; int &gt; &amp;)&gt; callback)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>BuildLineGraph</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>acb53c505b8fd29ceb3abdcc7dfd809ce</anchor>
      <arglist>(const GraphType &amp;graph, GraphType *const line_graph)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsEulerianGraph</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab1cf773de0cae72d0c44efe5b8f4bb89</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsSemiEulerianGraph</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a6b312dd19c90b2af099e6f159869f7ee</anchor>
      <arglist>(const Graph &amp;graph, std::vector&lt; NodeIndex &gt; *odd_nodes)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; NodeIndex &gt;</type>
      <name>BuildEulerianPathFromNode</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a743d8c9d6f64531bdeb7bbf18023e9d4</anchor>
      <arglist>(const Graph &amp;graph, NodeIndex root)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; NodeIndex &gt;</type>
      <name>BuildEulerianTourFromNode</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>aa63055860fc53f8eed56d23d2571c180</anchor>
      <arglist>(const Graph &amp;graph, NodeIndex root)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; typename Graph::NodeIndex &gt;</type>
      <name>BuildEulerianTour</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a034666fe63ca105b735272974006362a</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; typename Graph::NodeIndex &gt;</type>
      <name>BuildEulerianPath</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a49b170b2d03863c465331e67b21f0c33</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>HamiltonianPathSolver&lt; CostType, CostFunction &gt;</type>
      <name>MakeHamiltonianPathSolver</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a715b0dbb9f0903ab629b8c6da1b35b45</anchor>
      <arglist>(int num_nodes, CostFunction cost)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; typename Graph::ArcIndex &gt;</type>
      <name>BuildKruskalMinimumSpanningTreeFromSortedArcs</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a00ab79ee21ffd8dece0996e37f9faa7a</anchor>
      <arglist>(const Graph &amp;graph, const std::vector&lt; typename Graph::ArcIndex &gt; &amp;sorted_arcs)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; typename Graph::ArcIndex &gt;</type>
      <name>BuildKruskalMinimumSpanningTree</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>aa565a47a059ef32ef1aec39768e4ec98</anchor>
      <arglist>(const Graph &amp;graph, const ArcComparator &amp;arc_comparator)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; typename Graph::ArcIndex &gt;</type>
      <name>BuildPrimMinimumSpanningTree</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a33a2f4c26fd60cd0fa98257b571c974f</anchor>
      <arglist>(const Graph &amp;graph, const ArcValue &amp;arc_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>DijkstraShortestPath</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a53e6a83fcbd689abf5b3078b0236f9f1</anchor>
      <arglist>(int node_count, int start_node, int end_node, std::function&lt; int64(int, int)&gt; graph, int64 disconnected_distance, std::vector&lt; int &gt; *nodes)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>StableDijkstraShortestPath</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ad56bae19a2298c3163af96ca7f8b89b1</anchor>
      <arglist>(int node_count, int start_node, int end_node, std::function&lt; int64(int, int)&gt; graph, int64 disconnected_distance, std::vector&lt; int &gt; *nodes)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>BellmanFordShortestPath</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ad5bfec6ea714171fbff2d8b791d0d286</anchor>
      <arglist>(int node_count, int start_node, int end_node, std::function&lt; int64(int, int)&gt; graph, int64 disconnected_distance, std::vector&lt; int &gt; *nodes)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>AStarShortestPath</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>acff272be25bcf9641218c05c59ec1a4e</anchor>
      <arglist>(int node_count, int start_node, int end_node, std::function&lt; int64(int, int)&gt; graph, std::function&lt; int64(int)&gt; heuristic, int64 disconnected_distance, std::vector&lt; int &gt; *nodes)</arglist>
    </member>
  </compound>
  <compound kind="namespace">
    <name>util</name>
    <filename>namespaceutil.html</filename>
    <class kind="class">util::BaseGraph</class>
    <class kind="class">util::CompleteBipartiteGraph</class>
    <class kind="class">util::CompleteGraph</class>
    <class kind="class">util::ListGraph</class>
    <class kind="class">util::ReverseArcListGraph</class>
    <class kind="class">util::ReverseArcMixedGraph</class>
    <class kind="class">util::ReverseArcStaticGraph</class>
    <class kind="class">util::StaticGraph</class>
    <class kind="class">util::SVector</class>
    <class kind="class">util::UndirectedAdjacencyListsOfDirectedGraph</class>
    <member kind="typedef">
      <type>ListGraph</type>
      <name>Graph</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ae76339cb2dcd3bc05ad762146f91fdda</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>GraphToStringFormat</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ae705e1342dacc10a13fb3f11f91d0696</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PRINT_GRAPH_ARCS</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ae705e1342dacc10a13fb3f11f91d0696acac9245da1bf36d1d9382dc579e1a4fd</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PRINT_GRAPH_ADJACENCY_LISTS</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ae705e1342dacc10a13fb3f11f91d0696aaed5759e3b6e3a8592c9a21e0048b565</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PRINT_GRAPH_ADJACENCY_LISTS_SORTED</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ae705e1342dacc10a13fb3f11f91d0696a454bb1ede69e280a1e4959acb82748ef</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; int &gt;</type>
      <name>GetConnectedComponents</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a13f0e8f7e15873600cf8e395958c71e1</anchor>
      <arglist>(int num_nodes, const UndirectedGraph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>PermuteWithExplicitElementType</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a9470623ca7db3c4a62ce3b326c6b07d8</anchor>
      <arglist>(const IntVector &amp;permutation, Array *array_to_permute, ElementType unused)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Permute</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a8c227a057c1ce9d46b1185abf77ad91e</anchor>
      <arglist>(const IntVector &amp;permutation, Array *array_to_permute)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Permute</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ac497881c4166bc694adc4bee62746118</anchor>
      <arglist>(const IntVector &amp;permutation, std::vector&lt; bool &gt; *array_to_permute)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a37be0131ae922e30a286797a0bef0c96</anchor>
      <arglist>(ListGraph, Outgoing, Base::kNilArc)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>af3c40fc068f645d9dcd15c332e44fc25</anchor>
      <arglist>(StaticGraph, Outgoing, DirectArcLimit(node))</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a3098e161a6aceeca482be78d2d221b3b</anchor>
      <arglist>(ReverseArcListGraph, Outgoing, Base::kNilArc)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a4d0ae05975a2063f2edbeb749f690fc7</anchor>
      <arglist>(ReverseArcListGraph, Incoming, Base::kNilArc)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a22b5dcc01043ab8da01ebab71ec3ad31</anchor>
      <arglist>(ReverseArcListGraph, OutgoingOrOppositeIncoming, Base::kNilArc)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a863ccdb51afb5ef92fe6c94188a5f7e0</anchor>
      <arglist>(ReverseArcListGraph, OppositeIncoming, Base::kNilArc)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a2cc2a1037195d237820edc97d35404be</anchor>
      <arglist>(ReverseArcStaticGraph, Outgoing, DirectArcLimit(node))</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a2a51d676cd5d9354bfe1f80d09c44f39</anchor>
      <arglist>(ReverseArcStaticGraph, Incoming, ReverseArcLimit(node))</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a1db1a919e67261878ff8abda53e664c7</anchor>
      <arglist>(ReverseArcStaticGraph, OutgoingOrOppositeIncoming, DirectArcLimit(node))</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a1728675285eb75f9f18d6ed7c134d0b6</anchor>
      <arglist>(ReverseArcStaticGraph, OppositeIncoming, ReverseArcLimit(node))</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ab3308688d13e59e2351bef038ce1fdb0</anchor>
      <arglist>(ReverseArcMixedGraph, Outgoing, DirectArcLimit(node))</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a3c022b68f68916770fe09996df2f35a3</anchor>
      <arglist>(ReverseArcMixedGraph, Incoming, Base::kNilArc)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a97910ddfce7560b406aa3f4939434eb8</anchor>
      <arglist>(ReverseArcMixedGraph, OutgoingOrOppositeIncoming, DirectArcLimit(node))</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DEFINE_RANGE_BASED_ARC_ITERATION</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a6ce1a67d16c75b202f56301321a457c6</anchor>
      <arglist>(ReverseArcMixedGraph, OppositeIncoming, Base::kNilArc)</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>GraphToString</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>affed79554a202aaa8bda5b5e98c3a6b2</anchor>
      <arglist>(const Graph &amp;graph, GraphToStringFormat format)</arglist>
    </member>
    <member kind="function">
      <type>false</type>
      <name>ValueOrDie</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a86199e4832dd5c1d61baa53bfecb7b6d</anchor>
      <arglist>())</arglist>
    </member>
    <member kind="function">
      <type>*</type>
      <name>if</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a0a640b5d8a0ba7deaba9afbd4f3ca438</anchor>
      <arglist>(!error_or_graph.ok())</arglist>
    </member>
    <member kind="function">
      <type>***util::StatusOr&lt; Graph * &gt;</type>
      <name>ReadGraphFile</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a9c5c6763e52cd1465a3e1a3ab2437e37</anchor>
      <arglist>(const std::string &amp;filename, bool directed, std::vector&lt; int &gt; *num_nodes_with_color_or_null)</arglist>
    </member>
    <member kind="function">
      <type>util::Status</type>
      <name>WriteGraphToFile</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a6600986f328a49c9485aa03fb6c82946</anchor>
      <arglist>(const Graph &amp;graph, const std::string &amp;filename, bool directed, const std::vector&lt; int &gt; &amp;num_nodes_with_color)</arglist>
    </member>
    <member kind="function">
      <type>util::StatusOr&lt; Graph * &gt;</type>
      <name>ReadGraphFile</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>aeec5b66be4fd6b3021e6eb08b3045a0e</anchor>
      <arglist>(const std::string &amp;filename, bool directed, std::vector&lt; int &gt; *num_nodes_with_color_or_null)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>GraphHasSelfArcs</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ac4af76993c891ee4ad507783edec2a1c</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>GraphHasDuplicateArcs</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a6b37593970a26f5c88b3d2ea9acea9d2</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>GraphIsSymmetric</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a784b483eeae1b49164a8a02fe9c0d3ba</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>GraphIsWeaklyConnected</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a97311561fd1f01e9f35b2f7ce18b0af3</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>std::unique_ptr&lt; Graph &gt;</type>
      <name>CopyGraph</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a0ed748741b17dad9e6cc485728bb0043</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>std::unique_ptr&lt; Graph &gt;</type>
      <name>RemapGraph</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>acfecdce43e9933bde2a94fd879f12f5f</anchor>
      <arglist>(const Graph &amp;graph, const std::vector&lt; int &gt; &amp;new_node_index)</arglist>
    </member>
    <member kind="function">
      <type>std::unique_ptr&lt; Graph &gt;</type>
      <name>GetSubgraphOfNodes</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ad1df170a504d335462a1104a942e6069</anchor>
      <arglist>(const Graph &amp;graph, const std::vector&lt; int &gt; &amp;nodes)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; int &gt;</type>
      <name>GetWeaklyConnectedComponents</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ab34783e729bb5fc99042893f6bfcbb2f</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsSubsetOf0N</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>aa9fb4c9a176acaf72053b11727436e9e</anchor>
      <arglist>(const std::vector&lt; int &gt; &amp;v, int n)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsValidPermutation</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ad7986b01cf61a31c09a27b4a97db6a83</anchor>
      <arglist>(const std::vector&lt; int &gt; &amp;v)</arglist>
    </member>
    <member kind="function">
      <type>std::unique_ptr&lt; Graph &gt;</type>
      <name>RemoveSelfArcsAndDuplicateArcs</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a8a06031908a024a50dbdddc394a22490</anchor>
      <arglist>(const Graph &amp;graph)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>RemoveCyclesFromPath</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a06fa201576c927d92657e090fa86bfdb</anchor>
      <arglist>(const Graph &amp;graph, std::vector&lt; int &gt; *arc_path)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>PathHasCycle</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>adbb18bcb2f9d64cbbaeb57c328f57e7b</anchor>
      <arglist>(const Graph &amp;graph, const std::vector&lt; int &gt; &amp;arc_path)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; int &gt;</type>
      <name>ComputeOnePossibleReverseArcMapping</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>ae469c559688b92f36bae2788c2e6063e</anchor>
      <arglist>(const Graph &amp;graph, bool die_if_not_symmetric)</arglist>
    </member>
    <member kind="variable">
      <type>***util::StatusOr&lt; MyGraph * &gt;</type>
      <name>error_or_graph</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a123e77d101e4aeb54a2b9e7d9612ad1b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>else</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>a99d2b83baf3f908e76fb2161b1c73322</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type></type>
      <name>false</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>aadd7603ae6e78cc2490ca9710fbaf180</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>*OutgoingArcIterator</type>
      <name>this</name>
      <anchorfile>namespaceutil.html</anchorfile>
      <anchor>acc90f8dbcd326a450a7c781ea7a9539d</anchor>
      <arglist></arglist>
    </member>
  </compound>
</tagfile>
