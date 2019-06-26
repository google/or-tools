<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile>
  <compound kind="file">
    <name>dense_doubly_linked_list.h</name>
    <path>/Users/lperron/Work/or-tools/ortools/algorithms/</path>
    <filename>dense__doubly__linked__list_8h</filename>
    <class kind="class">operations_research::DenseDoublyLinkedList</class>
    <namespace>operations_research</namespace>
  </compound>
  <compound kind="file">
    <name>dynamic_partition.h</name>
    <path>/Users/lperron/Work/or-tools/ortools/algorithms/</path>
    <filename>dynamic__partition_8h</filename>
    <class kind="class">operations_research::DynamicPartition</class>
    <class kind="struct">operations_research::DynamicPartition::IterablePart</class>
    <class kind="class">operations_research::MergingPartition</class>
    <namespace>operations_research</namespace>
  </compound>
  <compound kind="file">
    <name>dynamic_permutation.h</name>
    <path>/Users/lperron/Work/or-tools/ortools/algorithms/</path>
    <filename>dynamic__permutation_8h</filename>
    <class kind="class">operations_research::DynamicPermutation</class>
    <namespace>operations_research</namespace>
  </compound>
  <compound kind="file">
    <name>find_graph_symmetries.h</name>
    <path>/Users/lperron/Work/or-tools/ortools/algorithms/</path>
    <filename>find__graph__symmetries_8h</filename>
    <includes id="dynamic__partition_8h" name="dynamic_partition.h" local="yes" imported="no">ortools/algorithms/dynamic_partition.h</includes>
    <includes id="dynamic__permutation_8h" name="dynamic_permutation.h" local="yes" imported="no">ortools/algorithms/dynamic_permutation.h</includes>
    <class kind="class">operations_research::GraphSymmetryFinder</class>
    <namespace>operations_research</namespace>
  </compound>
  <compound kind="file">
    <name>hungarian.h</name>
    <path>/Users/lperron/Work/or-tools/ortools/algorithms/</path>
    <filename>hungarian_8h</filename>
    <namespace>operations_research</namespace>
    <member kind="function">
      <type>void</type>
      <name>MinimizeLinearAssignment</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a9c61bb2d6de0894f19675e2110458877</anchor>
      <arglist>(const std::vector&lt; std::vector&lt; double &gt; &gt; &amp;cost, absl::flat_hash_map&lt; int, int &gt; *direct_assignment, absl::flat_hash_map&lt; int, int &gt; *reverse_assignment)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MaximizeLinearAssignment</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ada8ccc36ef736b10ce389fbd347c4282</anchor>
      <arglist>(const std::vector&lt; std::vector&lt; double &gt; &gt; &amp;cost, absl::flat_hash_map&lt; int, int &gt; *direct_assignment, absl::flat_hash_map&lt; int, int &gt; *reverse_assignment)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>knapsack_solver.h</name>
    <path>/Users/lperron/Work/or-tools/ortools/algorithms/</path>
    <filename>knapsack__solver_8h</filename>
    <class kind="class">operations_research::KnapsackSolver</class>
    <class kind="struct">operations_research::KnapsackAssignment</class>
    <class kind="struct">operations_research::KnapsackItem</class>
    <class kind="class">operations_research::KnapsackSearchNode</class>
    <class kind="class">operations_research::KnapsackSearchPath</class>
    <class kind="class">operations_research::KnapsackState</class>
    <class kind="class">operations_research::KnapsackPropagator</class>
    <class kind="class">operations_research::KnapsackCapacityPropagator</class>
    <class kind="class">operations_research::BaseKnapsackSolver</class>
    <class kind="class">operations_research::KnapsackGenericSolver</class>
    <namespace>operations_research</namespace>
    <member kind="typedef">
      <type>KnapsackItem *</type>
      <name>KnapsackItemPtr</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab6f3adbb119982fdfb9a85d87310f255</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>Select next search node to expand Select next item_i to</type>
      <name>assign</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab0de44378545d5727eb4400210e568f8</anchor>
      <arglist>(using master propagator) - Generate a new search node where item_i is in the knapsack - Check validity of this new partial solution(using propagators) - If valid</arglist>
    </member>
    <member kind="function">
      <type>Select next search node to expand Select next item_i to add this new search node to the search Generate a new search node where item_i is not in the knapsack Check validity of this new partial</type>
      <name>solution</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a35a30dc825cd0afa0e095f47118cd3cd</anchor>
      <arglist>(using propagators) - If valid</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>sparse_permutation.h</name>
    <path>/Users/lperron/Work/or-tools/ortools/algorithms/</path>
    <filename>sparse__permutation_8h</filename>
    <class kind="class">operations_research::SparsePermutation</class>
    <class kind="struct">operations_research::SparsePermutation::Iterator</class>
    <namespace>operations_research</namespace>
  </compound>
  <compound kind="class">
    <name>operations_research::BaseKnapsackSolver</name>
    <filename>classoperations__research_1_1BaseKnapsackSolver.html</filename>
    <member kind="function">
      <type></type>
      <name>BaseKnapsackSolver</name>
      <anchorfile>classoperations__research_1_1BaseKnapsackSolver.html</anchorfile>
      <anchor>a507afd9932799f0be34f34605cd5ffee</anchor>
      <arglist>(const std::string &amp;solver_name)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~BaseKnapsackSolver</name>
      <anchorfile>classoperations__research_1_1BaseKnapsackSolver.html</anchorfile>
      <anchor>a5f685ec61d9de86d57bdc53124c08b12</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual void</type>
      <name>Init</name>
      <anchorfile>classoperations__research_1_1BaseKnapsackSolver.html</anchorfile>
      <anchor>a3e8f72facec15537065c1625e647d58b</anchor>
      <arglist>(const std::vector&lt; int64 &gt; &amp;profits, const std::vector&lt; std::vector&lt; int64 &gt; &gt; &amp;weights, const std::vector&lt; int64 &gt; &amp;capacities)=0</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>GetLowerAndUpperBoundWhenItem</name>
      <anchorfile>classoperations__research_1_1BaseKnapsackSolver.html</anchorfile>
      <anchor>a381cc9d7b544a8ec4727f9618f0b4f8f</anchor>
      <arglist>(int item_id, bool is_item_in, int64 *lower_bound, int64 *upper_bound)</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual int64</type>
      <name>Solve</name>
      <anchorfile>classoperations__research_1_1BaseKnapsackSolver.html</anchorfile>
      <anchor>a952deb24e032890d2781e52fedb70efa</anchor>
      <arglist>(TimeLimit *time_limit, bool *is_solution_optimal)=0</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual bool</type>
      <name>best_solution</name>
      <anchorfile>classoperations__research_1_1BaseKnapsackSolver.html</anchorfile>
      <anchor>a08bf62b91f6cf3fca10a504f7dd278a6</anchor>
      <arglist>(int item_id) const =0</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual std::string</type>
      <name>GetName</name>
      <anchorfile>classoperations__research_1_1BaseKnapsackSolver.html</anchorfile>
      <anchor>ae19aff92b4b38e712ead1cfcafd81f03</anchor>
      <arglist>() const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::DenseDoublyLinkedList</name>
    <filename>classoperations__research_1_1DenseDoublyLinkedList.html</filename>
    <member kind="function">
      <type></type>
      <name>DenseDoublyLinkedList</name>
      <anchorfile>classoperations__research_1_1DenseDoublyLinkedList.html</anchorfile>
      <anchor>a6ea3b3bb173b72107f927b55b7d32919</anchor>
      <arglist>(const T &amp;sorted_elements)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>Size</name>
      <anchorfile>classoperations__research_1_1DenseDoublyLinkedList.html</anchorfile>
      <anchor>adb8f55ef6621a5918df0acf9393b1771</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>Next</name>
      <anchorfile>classoperations__research_1_1DenseDoublyLinkedList.html</anchorfile>
      <anchor>a7ea39c46d7839fa2423f91a83eb9f2e1</anchor>
      <arglist>(int i) const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>Prev</name>
      <anchorfile>classoperations__research_1_1DenseDoublyLinkedList.html</anchorfile>
      <anchor>a99513d66c211333074c3ba1e596af544</anchor>
      <arglist>(int i) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Remove</name>
      <anchorfile>classoperations__research_1_1DenseDoublyLinkedList.html</anchorfile>
      <anchor>a15398a9cf876c2e4373cce7a7c43b24d</anchor>
      <arglist>(int i)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::DynamicPartition</name>
    <filename>classoperations__research_1_1DynamicPartition.html</filename>
    <class kind="struct">operations_research::DynamicPartition::IterablePart</class>
    <member kind="enumeration">
      <type></type>
      <name>DebugStringSorting</name>
      <anchorfile>classoperations__research_1_1DynamicPartition.html</anchorfile>
      <anchor>a67b2a5be57842485dcb3c3db93bc2e1a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SORT_LEXICOGRAPHICALLY</name>
      <anchorfile>classoperations__research_1_1DynamicPartition.html</anchorfile>
      <anchor>a67b2a5be57842485dcb3c3db93bc2e1aacb9b001cac613035de5d6f1f38f7fda1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SORT_BY_PART</name>
      <anchorfile>classoperations__research_1_1DynamicPartition.html</anchorfile>
      <anchor>a67b2a5be57842485dcb3c3db93bc2e1aa5edad0b79a2c83ec1b1560960504c161</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SORT_LEXICOGRAPHICALLY</name>
      <anchorfile>classoperations__research_1_1DynamicPartition.html</anchorfile>
      <anchor>a67b2a5be57842485dcb3c3db93bc2e1aacb9b001cac613035de5d6f1f38f7fda1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SORT_BY_PART</name>
      <anchorfile>classoperations__research_1_1DynamicPartition.html</anchorfile>
      <anchor>a67b2a5be57842485dcb3c3db93bc2e1aa5edad0b79a2c83ec1b1560960504c161</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DynamicPartition</name>
      <anchorfile>classoperations__research_1_1DynamicPartition.html</anchorfile>
      <anchor>afe6f90b99c0b1d646b00ad2d1bb0a09d</anchor>
      <arglist>(int num_elements)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DynamicPartition</name>
      <anchorfile>classoperations__research_1_1DynamicPartition.html</anchorfile>
      <anchor>ae0789ce355b93a5161e63666b4467e67</anchor>
      <arglist>(const std::vector&lt; int &gt; &amp;initial_part_of_element)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>NumElements</name>
      <anchorfile>classoperations__research_1_1DynamicPartition.html</anchorfile>
      <anchor>a4b07bd0eb40bb3fcb3419672313342ea</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const int</type>
      <name>NumParts</name>
      <anchorfile>classoperations__research_1_1DynamicPartition.html</anchorfile>
      <anchor>a0f4d81d407e148e26480f5ef01778270</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IterablePart</type>
      <name>ElementsInPart</name>
      <anchorfile>classoperations__research_1_1DynamicPartition.html</anchorfile>
      <anchor>ac1dcc361f73f495e0f362f834b224e89</anchor>
      <arglist>(int i) const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>PartOf</name>
      <anchorfile>classoperations__research_1_1DynamicPartition.html</anchorfile>
      <anchor>af3a2c6e1e18891125f15e42e391055bb</anchor>
      <arglist>(int element) const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>SizeOfPart</name>
      <anchorfile>classoperations__research_1_1DynamicPartition.html</anchorfile>
      <anchor>a87ccb17e1972d153100a9b061a918ba1</anchor>
      <arglist>(int part) const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>ParentOfPart</name>
      <anchorfile>classoperations__research_1_1DynamicPartition.html</anchorfile>
      <anchor>a079d605b371556e228347843c9b15d2d</anchor>
      <arglist>(int part) const</arglist>
    </member>
    <member kind="function">
      <type>IterablePart</type>
      <name>ElementsInSamePartAs</name>
      <anchorfile>classoperations__research_1_1DynamicPartition.html</anchorfile>
      <anchor>a79106998d8499c5c25f4c74c14456b60</anchor>
      <arglist>(int i) const</arglist>
    </member>
    <member kind="function">
      <type>uint64</type>
      <name>FprintOfPart</name>
      <anchorfile>classoperations__research_1_1DynamicPartition.html</anchorfile>
      <anchor>a4e68b8de69272cf549b346595d332198</anchor>
      <arglist>(int part) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Refine</name>
      <anchorfile>classoperations__research_1_1DynamicPartition.html</anchorfile>
      <anchor>a16fce09dd4c359a6acfd0a864e0a5ebd</anchor>
      <arglist>(const std::vector&lt; int &gt; &amp;distinguished_subset)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>UndoRefineUntilNumPartsEqual</name>
      <anchorfile>classoperations__research_1_1DynamicPartition.html</anchorfile>
      <anchor>a968140cdf9d2766625e1d476a7a71590</anchor>
      <arglist>(int original_num_parts)</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>DebugString</name>
      <anchorfile>classoperations__research_1_1DynamicPartition.html</anchorfile>
      <anchor>ac535d0a440192c96effe5b938a9482c7</anchor>
      <arglist>(DebugStringSorting sorting) const</arglist>
    </member>
    <member kind="function">
      <type>const std::vector&lt; int &gt; &amp;</type>
      <name>ElementsInHierarchicalOrder</name>
      <anchorfile>classoperations__research_1_1DynamicPartition.html</anchorfile>
      <anchor>ab516c7e5e0e64a4fca27ab1790d459e5</anchor>
      <arglist>() const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::DynamicPermutation</name>
    <filename>classoperations__research_1_1DynamicPermutation.html</filename>
    <member kind="function">
      <type></type>
      <name>DynamicPermutation</name>
      <anchorfile>classoperations__research_1_1DynamicPermutation.html</anchorfile>
      <anchor>a5743ffc1c6fb22b0c4d63ce242135cbe</anchor>
      <arglist>(int n)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>Size</name>
      <anchorfile>classoperations__research_1_1DynamicPermutation.html</anchorfile>
      <anchor>a3e31debbde606afd35a1a8e4a366a5da</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddMappings</name>
      <anchorfile>classoperations__research_1_1DynamicPermutation.html</anchorfile>
      <anchor>af1058ff0d3ae20f6f313a82e12febbbf</anchor>
      <arglist>(const std::vector&lt; int &gt; &amp;src, const std::vector&lt; int &gt; &amp;dst)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>UndoLastMappings</name>
      <anchorfile>classoperations__research_1_1DynamicPermutation.html</anchorfile>
      <anchor>a5493ce64ddd5b10144e57a0e630c1657</anchor>
      <arglist>(std::vector&lt; int &gt; *undone_mapping_src)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Reset</name>
      <anchorfile>classoperations__research_1_1DynamicPermutation.html</anchorfile>
      <anchor>a439e971c48da03d73621df8149c27167</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>ImageOf</name>
      <anchorfile>classoperations__research_1_1DynamicPermutation.html</anchorfile>
      <anchor>a455f2fd63b17930ebb1d9a72f642ebd9</anchor>
      <arglist>(int i) const</arglist>
    </member>
    <member kind="function">
      <type>const std::vector&lt; int &gt; &amp;</type>
      <name>AllMappingsSrc</name>
      <anchorfile>classoperations__research_1_1DynamicPermutation.html</anchorfile>
      <anchor>a5f2bde228e58a2d71d457b0329f6ab2a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>RootOf</name>
      <anchorfile>classoperations__research_1_1DynamicPermutation.html</anchorfile>
      <anchor>a26a6e9c551114feee802a46a49cf2b4a</anchor>
      <arglist>(int i) const</arglist>
    </member>
    <member kind="function">
      <type>const std::set&lt; int &gt; &amp;</type>
      <name>LooseEnds</name>
      <anchorfile>classoperations__research_1_1DynamicPermutation.html</anchorfile>
      <anchor>acf9308c2eb8d7c1d5b5e3193c672f36d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>std::unique_ptr&lt; SparsePermutation &gt;</type>
      <name>CreateSparsePermutation</name>
      <anchorfile>classoperations__research_1_1DynamicPermutation.html</anchorfile>
      <anchor>afc0264e56e82c46a27db9b57ed58cd45</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>DebugString</name>
      <anchorfile>classoperations__research_1_1DynamicPermutation.html</anchorfile>
      <anchor>ae095cf4e546b2a3048a4f424fcd1e770</anchor>
      <arglist>() const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::GraphSymmetryFinder</name>
    <filename>classoperations__research_1_1GraphSymmetryFinder.html</filename>
    <member kind="typedef">
      <type>::util::StaticGraph</type>
      <name>Graph</name>
      <anchorfile>classoperations__research_1_1GraphSymmetryFinder.html</anchorfile>
      <anchor>aeabd870fd44ae7ee75a3341b86e140a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>GraphSymmetryFinder</name>
      <anchorfile>classoperations__research_1_1GraphSymmetryFinder.html</anchorfile>
      <anchor>af670a8f7f325e3a7431f2723c52ec25d</anchor>
      <arglist>(const Graph &amp;graph, bool is_undirected)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsGraphAutomorphism</name>
      <anchorfile>classoperations__research_1_1GraphSymmetryFinder.html</anchorfile>
      <anchor>afb920046454e1c278a6189cf6634a48c</anchor>
      <arglist>(const DynamicPermutation &amp;permutation) const</arglist>
    </member>
    <member kind="function">
      <type>util::Status</type>
      <name>FindSymmetries</name>
      <anchorfile>classoperations__research_1_1GraphSymmetryFinder.html</anchorfile>
      <anchor>a0923c1bbc2e1514814a1d7fa55d7b0dc</anchor>
      <arglist>(double time_limit_seconds, std::vector&lt; int &gt; *node_equivalence_classes_io, std::vector&lt; std::unique_ptr&lt; SparsePermutation &gt; &gt; *generators, std::vector&lt; int &gt; *factorized_automorphism_group_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>RecursivelyRefinePartitionByAdjacency</name>
      <anchorfile>classoperations__research_1_1GraphSymmetryFinder.html</anchorfile>
      <anchor>a70544105d8f21edcbed63fdd7f6d34ab</anchor>
      <arglist>(int first_unrefined_part_index, DynamicPartition *partition)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>DistinguishNodeInPartition</name>
      <anchorfile>classoperations__research_1_1GraphSymmetryFinder.html</anchorfile>
      <anchor>af15616e8b6f42c682ff091b989190ff8</anchor>
      <arglist>(int node, DynamicPartition *partition, std::vector&lt; int &gt; *new_singletons_or_null)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>operations_research::DynamicPartition::IterablePart</name>
    <filename>structoperations__research_1_1DynamicPartition_1_1IterablePart.html</filename>
    <member kind="typedef">
      <type>int</type>
      <name>value_type</name>
      <anchorfile>structoperations__research_1_1DynamicPartition_1_1IterablePart.html</anchorfile>
      <anchor>ae0d65370bdf691912b56f25f52a4561e</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>std::vector&lt; int &gt;::const_iterator</type>
      <name>const_iterator</name>
      <anchorfile>structoperations__research_1_1DynamicPartition_1_1IterablePart.html</anchorfile>
      <anchor>a449c15b1c8a64ef6572463e2f21ae174</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; int &gt;::const_iterator</type>
      <name>begin</name>
      <anchorfile>structoperations__research_1_1DynamicPartition_1_1IterablePart.html</anchorfile>
      <anchor>ab1abe2eb3716aa80bd29b1e7260484a2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; int &gt;::const_iterator</type>
      <name>end</name>
      <anchorfile>structoperations__research_1_1DynamicPartition_1_1IterablePart.html</anchorfile>
      <anchor>abd3e25c24209be1e6cffda93230bbdd9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>size</name>
      <anchorfile>structoperations__research_1_1DynamicPartition_1_1IterablePart.html</anchorfile>
      <anchor>a757903108b39220312da2867ca8babe8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>IterablePart</name>
      <anchorfile>structoperations__research_1_1DynamicPartition_1_1IterablePart.html</anchorfile>
      <anchor>a4858bbbfb1381ce091704073d878495b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>IterablePart</name>
      <anchorfile>structoperations__research_1_1DynamicPartition_1_1IterablePart.html</anchorfile>
      <anchor>ae05724f38a9dbde27efff486cfde1e81</anchor>
      <arglist>(const std::vector&lt; int &gt;::const_iterator &amp;b, const std::vector&lt; int &gt;::const_iterator &amp;e)</arglist>
    </member>
    <member kind="variable">
      <type>std::vector&lt; int &gt;::const_iterator</type>
      <name>begin_</name>
      <anchorfile>structoperations__research_1_1DynamicPartition_1_1IterablePart.html</anchorfile>
      <anchor>ac1869cef1d61b3cf54d2336f9e1768b0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>std::vector&lt; int &gt;::const_iterator</type>
      <name>end_</name>
      <anchorfile>structoperations__research_1_1DynamicPartition_1_1IterablePart.html</anchorfile>
      <anchor>a811d152ac3bcfa8132b5dc1e5c0e5818</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>operations_research::SparsePermutation::Iterator</name>
    <filename>structoperations__research_1_1SparsePermutation_1_1Iterator.html</filename>
    <member kind="typedef">
      <type>int</type>
      <name>value_type</name>
      <anchorfile>structoperations__research_1_1SparsePermutation_1_1Iterator.html</anchorfile>
      <anchor>a502fa137e188bc3bcf394fd898af8eb5</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>std::vector&lt; int &gt;::const_iterator</type>
      <name>const_iterator</name>
      <anchorfile>structoperations__research_1_1SparsePermutation_1_1Iterator.html</anchorfile>
      <anchor>a2a400be7490fa963e8c0cff44a7f199b</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>Iterator</name>
      <anchorfile>structoperations__research_1_1SparsePermutation_1_1Iterator.html</anchorfile>
      <anchor>a5fca038158fb31f2ccd8a6bad141b96a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>Iterator</name>
      <anchorfile>structoperations__research_1_1SparsePermutation_1_1Iterator.html</anchorfile>
      <anchor>af753db0fd3ba19caf1d2efdda42bb15e</anchor>
      <arglist>(const std::vector&lt; int &gt;::const_iterator &amp;b, const std::vector&lt; int &gt;::const_iterator &amp;e)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; int &gt;::const_iterator</type>
      <name>begin</name>
      <anchorfile>structoperations__research_1_1SparsePermutation_1_1Iterator.html</anchorfile>
      <anchor>a3d0ee250e9f9421b0cfa2ace76b1063f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; int &gt;::const_iterator</type>
      <name>end</name>
      <anchorfile>structoperations__research_1_1SparsePermutation_1_1Iterator.html</anchorfile>
      <anchor>a2bcfdb8bce3544de6ab9b6c6fc572a99</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>size</name>
      <anchorfile>structoperations__research_1_1SparsePermutation_1_1Iterator.html</anchorfile>
      <anchor>a99e9ae420b0274aae3c1fbf997178f41</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable">
      <type>const std::vector&lt; int &gt;::const_iterator</type>
      <name>begin_</name>
      <anchorfile>structoperations__research_1_1SparsePermutation_1_1Iterator.html</anchorfile>
      <anchor>aa12b08b67ec2491e07cba6073d40f847</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const std::vector&lt; int &gt;::const_iterator</type>
      <name>end_</name>
      <anchorfile>structoperations__research_1_1SparsePermutation_1_1Iterator.html</anchorfile>
      <anchor>a337e656f1df64dc5910e329459fe6ec3</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>operations_research::KnapsackAssignment</name>
    <filename>structoperations__research_1_1KnapsackAssignment.html</filename>
    <member kind="function">
      <type></type>
      <name>KnapsackAssignment</name>
      <anchorfile>structoperations__research_1_1KnapsackAssignment.html</anchorfile>
      <anchor>aab038e186549b78c472c9fbe16f6f315</anchor>
      <arglist>(int _item_id, bool _is_in)</arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>item_id</name>
      <anchorfile>structoperations__research_1_1KnapsackAssignment.html</anchorfile>
      <anchor>a91340b6da590fae44e79199666ccf468</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>is_in</name>
      <anchorfile>structoperations__research_1_1KnapsackAssignment.html</anchorfile>
      <anchor>ab4ba3de70cb7aa3ad9aaee3dd2c60b7b</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::KnapsackCapacityPropagator</name>
    <filename>classoperations__research_1_1KnapsackCapacityPropagator.html</filename>
    <base>operations_research::KnapsackPropagator</base>
    <member kind="function">
      <type></type>
      <name>KnapsackCapacityPropagator</name>
      <anchorfile>classoperations__research_1_1KnapsackCapacityPropagator.html</anchorfile>
      <anchor>a9dd517fbac9988f24173eaf86238976e</anchor>
      <arglist>(const KnapsackState &amp;state, int64 capacity)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>~KnapsackCapacityPropagator</name>
      <anchorfile>classoperations__research_1_1KnapsackCapacityPropagator.html</anchorfile>
      <anchor>a33d39340cd8d61263b20361c8b774832</anchor>
      <arglist>() override</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ComputeProfitBounds</name>
      <anchorfile>classoperations__research_1_1KnapsackCapacityPropagator.html</anchorfile>
      <anchor>af1d7d1e175e652c64962af490ab42247</anchor>
      <arglist>() override</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetNextItemId</name>
      <anchorfile>classoperations__research_1_1KnapsackCapacityPropagator.html</anchorfile>
      <anchor>aeea8be32ceca77b5eaf9139df5e4ea4a</anchor>
      <arglist>() const override</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Init</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>aeb74436e19f4e74afdeb17f623bb0335</anchor>
      <arglist>(const std::vector&lt; int64 &gt; &amp;profits, const std::vector&lt; int64 &gt; &amp;weights)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Update</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>a5843f83c839a81d039724b6d49ba325a</anchor>
      <arglist>(bool revert, const KnapsackAssignment &amp;assignment)</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>current_profit</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>a49bb0497f50479663fc4b37bb4a08268</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>profit_lower_bound</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>a11ea8cfd2264c8f033d59e2848f9a871</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>profit_upper_bound</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>aaa95c66b22228756739735f4574cb4a1</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyCurrentStateToSolution</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>a4a8ef9517e0c8bf1eb1e450b72aa4e05</anchor>
      <arglist>(bool has_one_propagator, std::vector&lt; bool &gt; *solution) const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>InitPropagator</name>
      <anchorfile>classoperations__research_1_1KnapsackCapacityPropagator.html</anchorfile>
      <anchor>a59eff70af400b44d87d57b90047ada6e</anchor>
      <arglist>() override</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>bool</type>
      <name>UpdatePropagator</name>
      <anchorfile>classoperations__research_1_1KnapsackCapacityPropagator.html</anchorfile>
      <anchor>a944c99875dbcc49d557c129f4077799b</anchor>
      <arglist>(bool revert, const KnapsackAssignment &amp;assignment) override</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>CopyCurrentStateToSolutionPropagator</name>
      <anchorfile>classoperations__research_1_1KnapsackCapacityPropagator.html</anchorfile>
      <anchor>a03745304f7f106699428e4ed6efdef9d</anchor>
      <arglist>(std::vector&lt; bool &gt; *solution) const override</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>const KnapsackState &amp;</type>
      <name>state</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>af652548ebe8eeb5b0abfb900cf7f2927</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>const std::vector&lt; KnapsackItemPtr &gt; &amp;</type>
      <name>items</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>ac6a4643a2599c05708695addf803e52d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>set_profit_lower_bound</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>a697a86fbe4ab12732c8e6972ebdd5947</anchor>
      <arglist>(int64 profit)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>set_profit_upper_bound</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>a7bbc1ccc57318641a01cd253299f5861</anchor>
      <arglist>(int64 profit)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::KnapsackGenericSolver</name>
    <filename>classoperations__research_1_1KnapsackGenericSolver.html</filename>
    <base>operations_research::BaseKnapsackSolver</base>
    <member kind="function">
      <type></type>
      <name>KnapsackGenericSolver</name>
      <anchorfile>classoperations__research_1_1KnapsackGenericSolver.html</anchorfile>
      <anchor>ad92ca41f734bcbc35ef295f8a4bd556c</anchor>
      <arglist>(const std::string &amp;solver_name)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>~KnapsackGenericSolver</name>
      <anchorfile>classoperations__research_1_1KnapsackGenericSolver.html</anchorfile>
      <anchor>a7635ee76659f4e0117709aab3809460b</anchor>
      <arglist>() override</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Init</name>
      <anchorfile>classoperations__research_1_1KnapsackGenericSolver.html</anchorfile>
      <anchor>af9635774fd7761f62ee92e64205034b3</anchor>
      <arglist>(const std::vector&lt; int64 &gt; &amp;profits, const std::vector&lt; std::vector&lt; int64 &gt; &gt; &amp;weights, const std::vector&lt; int64 &gt; &amp;capacities) override</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetNumberOfItems</name>
      <anchorfile>classoperations__research_1_1KnapsackGenericSolver.html</anchorfile>
      <anchor>aa809ec4aa24281647003e8a473784051</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>GetLowerAndUpperBoundWhenItem</name>
      <anchorfile>classoperations__research_1_1KnapsackGenericSolver.html</anchorfile>
      <anchor>aa49dbbb2b6eae494f521992c964bad53</anchor>
      <arglist>(int item_id, bool is_item_in, int64 *lower_bound, int64 *upper_bound) override</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_master_propagator_id</name>
      <anchorfile>classoperations__research_1_1KnapsackGenericSolver.html</anchorfile>
      <anchor>ae320eaa08affeb2643270191b8f9700b</anchor>
      <arglist>(int master_propagator_id)</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>Solve</name>
      <anchorfile>classoperations__research_1_1KnapsackGenericSolver.html</anchorfile>
      <anchor>a8f57ec05930a3df316b5df101c814984</anchor>
      <arglist>(TimeLimit *time_limit, bool *is_solution_optimal) override</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>best_solution</name>
      <anchorfile>classoperations__research_1_1KnapsackGenericSolver.html</anchorfile>
      <anchor>a3f48cb5dfceb3c4129779568d2569606</anchor>
      <arglist>(int item_id) const override</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual std::string</type>
      <name>GetName</name>
      <anchorfile>classoperations__research_1_1BaseKnapsackSolver.html</anchorfile>
      <anchor>ae19aff92b4b38e712ead1cfcafd81f03</anchor>
      <arglist>() const</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>operations_research::KnapsackItem</name>
    <filename>structoperations__research_1_1KnapsackItem.html</filename>
    <member kind="function">
      <type></type>
      <name>KnapsackItem</name>
      <anchorfile>structoperations__research_1_1KnapsackItem.html</anchorfile>
      <anchor>a5ae427adb88e21ec0f883b8bbcc4b1d4</anchor>
      <arglist>(int _id, int64 _weight, int64 _profit)</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>GetEfficiency</name>
      <anchorfile>structoperations__research_1_1KnapsackItem.html</anchorfile>
      <anchor>a99b55b42e78cab5b85c8f411d13f32e9</anchor>
      <arglist>(int64 profit_max) const</arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>id</name>
      <anchorfile>structoperations__research_1_1KnapsackItem.html</anchorfile>
      <anchor>a69aae0cb6039a8a356744648c8dfb9c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int64</type>
      <name>weight</name>
      <anchorfile>structoperations__research_1_1KnapsackItem.html</anchorfile>
      <anchor>ab974e9e70c585c50c96851ecb454d882</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int64</type>
      <name>profit</name>
      <anchorfile>structoperations__research_1_1KnapsackItem.html</anchorfile>
      <anchor>a057015810b94bfb1b5efac74e9d8d487</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::KnapsackPropagator</name>
    <filename>classoperations__research_1_1KnapsackPropagator.html</filename>
    <member kind="function">
      <type></type>
      <name>KnapsackPropagator</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>a8d55d0c6ac3f05b72100f1635cfd2250</anchor>
      <arglist>(const KnapsackState &amp;state)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~KnapsackPropagator</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>a510e0ee59d597e16200ee04ab2e417ac</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Init</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>aeb74436e19f4e74afdeb17f623bb0335</anchor>
      <arglist>(const std::vector&lt; int64 &gt; &amp;profits, const std::vector&lt; int64 &gt; &amp;weights)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Update</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>a5843f83c839a81d039724b6d49ba325a</anchor>
      <arglist>(bool revert, const KnapsackAssignment &amp;assignment)</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual void</type>
      <name>ComputeProfitBounds</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>a8133fbb05961e760ed907e1f599966d8</anchor>
      <arglist>()=0</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual int</type>
      <name>GetNextItemId</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>a44b8bedd2218a210512ab3f8f3172312</anchor>
      <arglist>() const =0</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>current_profit</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>a49bb0497f50479663fc4b37bb4a08268</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>profit_lower_bound</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>a11ea8cfd2264c8f033d59e2848f9a871</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>profit_upper_bound</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>aaa95c66b22228756739735f4574cb4a1</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyCurrentStateToSolution</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>a4a8ef9517e0c8bf1eb1e450b72aa4e05</anchor>
      <arglist>(bool has_one_propagator, std::vector&lt; bool &gt; *solution) const</arglist>
    </member>
    <member kind="function" protection="protected" virtualness="pure">
      <type>virtual void</type>
      <name>InitPropagator</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>aeb99e95f19c68257f0b450cfff8fc3d0</anchor>
      <arglist>()=0</arglist>
    </member>
    <member kind="function" protection="protected" virtualness="pure">
      <type>virtual bool</type>
      <name>UpdatePropagator</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>abaf6d9225cde1b0d83d1a54299705ccb</anchor>
      <arglist>(bool revert, const KnapsackAssignment &amp;assignment)=0</arglist>
    </member>
    <member kind="function" protection="protected" virtualness="pure">
      <type>virtual void</type>
      <name>CopyCurrentStateToSolutionPropagator</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>a95551ecb408465b2fcf57aaaac393336</anchor>
      <arglist>(std::vector&lt; bool &gt; *solution) const =0</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>const KnapsackState &amp;</type>
      <name>state</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>af652548ebe8eeb5b0abfb900cf7f2927</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>const std::vector&lt; KnapsackItemPtr &gt; &amp;</type>
      <name>items</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>ac6a4643a2599c05708695addf803e52d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>set_profit_lower_bound</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>a697a86fbe4ab12732c8e6972ebdd5947</anchor>
      <arglist>(int64 profit)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>set_profit_upper_bound</name>
      <anchorfile>classoperations__research_1_1KnapsackPropagator.html</anchorfile>
      <anchor>a7bbc1ccc57318641a01cd253299f5861</anchor>
      <arglist>(int64 profit)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::KnapsackSearchNode</name>
    <filename>classoperations__research_1_1KnapsackSearchNode.html</filename>
    <member kind="function">
      <type></type>
      <name>KnapsackSearchNode</name>
      <anchorfile>classoperations__research_1_1KnapsackSearchNode.html</anchorfile>
      <anchor>ae4b93f14ee40150e79e0551006a93705</anchor>
      <arglist>(const KnapsackSearchNode *const parent, const KnapsackAssignment &amp;assignment)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>depth</name>
      <anchorfile>classoperations__research_1_1KnapsackSearchNode.html</anchorfile>
      <anchor>a16d77e11bc5eac6ddd95e129a8904753</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const KnapsackSearchNode *const</type>
      <name>parent</name>
      <anchorfile>classoperations__research_1_1KnapsackSearchNode.html</anchorfile>
      <anchor>aefa89c44b243d58591f919ec6e455989</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const KnapsackAssignment &amp;</type>
      <name>assignment</name>
      <anchorfile>classoperations__research_1_1KnapsackSearchNode.html</anchorfile>
      <anchor>a4a1f5f83b8b2ec165e4e6b8cd35dd60d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>current_profit</name>
      <anchorfile>classoperations__research_1_1KnapsackSearchNode.html</anchorfile>
      <anchor>ac7cf8a7ec95046908db026fc079d3a88</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_current_profit</name>
      <anchorfile>classoperations__research_1_1KnapsackSearchNode.html</anchorfile>
      <anchor>a2be03bd371b69f335778de84dc7ae5a8</anchor>
      <arglist>(int64 profit)</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>profit_upper_bound</name>
      <anchorfile>classoperations__research_1_1KnapsackSearchNode.html</anchorfile>
      <anchor>a3691617cae29584af4d54edf27b1f045</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_profit_upper_bound</name>
      <anchorfile>classoperations__research_1_1KnapsackSearchNode.html</anchorfile>
      <anchor>a5af222fdaf626cb0e33d2191d90b806c</anchor>
      <arglist>(int64 profit)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>next_item_id</name>
      <anchorfile>classoperations__research_1_1KnapsackSearchNode.html</anchorfile>
      <anchor>a28a03df4a481f501c503fc9500eb8f47</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_next_item_id</name>
      <anchorfile>classoperations__research_1_1KnapsackSearchNode.html</anchorfile>
      <anchor>aa09bf719c32bf8f662d3c609f3e72f19</anchor>
      <arglist>(int id)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::KnapsackSearchPath</name>
    <filename>classoperations__research_1_1KnapsackSearchPath.html</filename>
    <member kind="function">
      <type></type>
      <name>KnapsackSearchPath</name>
      <anchorfile>classoperations__research_1_1KnapsackSearchPath.html</anchorfile>
      <anchor>a459653bd55ab09a55714f660419367b1</anchor>
      <arglist>(const KnapsackSearchNode &amp;from, const KnapsackSearchNode &amp;to)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Init</name>
      <anchorfile>classoperations__research_1_1KnapsackSearchPath.html</anchorfile>
      <anchor>a91d3f4f27442d3ee1f748811d5d0d964</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const KnapsackSearchNode &amp;</type>
      <name>from</name>
      <anchorfile>classoperations__research_1_1KnapsackSearchPath.html</anchorfile>
      <anchor>abfbc25d180ab655ca72aa1dda80b5e1b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const KnapsackSearchNode &amp;</type>
      <name>via</name>
      <anchorfile>classoperations__research_1_1KnapsackSearchPath.html</anchorfile>
      <anchor>afede2909f269364002ffe722f4bed1b2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const KnapsackSearchNode &amp;</type>
      <name>to</name>
      <anchorfile>classoperations__research_1_1KnapsackSearchPath.html</anchorfile>
      <anchor>a070dcd6139b7a475886f061cefb7fd78</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const KnapsackSearchNode *</type>
      <name>MoveUpToDepth</name>
      <anchorfile>classoperations__research_1_1KnapsackSearchPath.html</anchorfile>
      <anchor>a3c129707bd16aea17d0eb3ad6c4e10e5</anchor>
      <arglist>(const KnapsackSearchNode &amp;node, int depth) const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::KnapsackSolver</name>
    <filename>classoperations__research_1_1KnapsackSolver.html</filename>
    <member kind="enumeration">
      <type></type>
      <name>SolverType</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a81ce17438663c39f7793e8db92ff1893</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KNAPSACK_BRUTE_FORCE_SOLVER</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a81ce17438663c39f7793e8db92ff1893a4d72c45990d1a81e3f5bcdaf6de72096</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KNAPSACK_64ITEMS_SOLVER</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a81ce17438663c39f7793e8db92ff1893aa6c3b9157b2506f5a53b0c73165c8f9b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KNAPSACK_DYNAMIC_PROGRAMMING_SOLVER</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a81ce17438663c39f7793e8db92ff1893a9c02fa29c925bc1d37cba92490998132</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a81ce17438663c39f7793e8db92ff1893ae8c15c25eaf606f5f853821aaabba164</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KNAPSACK_BRUTE_FORCE_SOLVER</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a81ce17438663c39f7793e8db92ff1893a4d72c45990d1a81e3f5bcdaf6de72096</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KNAPSACK_64ITEMS_SOLVER</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a81ce17438663c39f7793e8db92ff1893aa6c3b9157b2506f5a53b0c73165c8f9b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KNAPSACK_DYNAMIC_PROGRAMMING_SOLVER</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a81ce17438663c39f7793e8db92ff1893a9c02fa29c925bc1d37cba92490998132</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a81ce17438663c39f7793e8db92ff1893ae8c15c25eaf606f5f853821aaabba164</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>KnapsackSolver</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a7638cb76df7cb79d956dda62d179a554</anchor>
      <arglist>(const std::string &amp;solver_name)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>KnapsackSolver</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>ab36466b997fe3f25955461708061ce40</anchor>
      <arglist>(SolverType solver_type, const std::string &amp;solver_name)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~KnapsackSolver</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>ac6d1a38c9a1c607e80d05720372ace4d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Init</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>ad2a0397c061a1cf0bfc759b35f23e463</anchor>
      <arglist>(const std::vector&lt; int64 &gt; &amp;profits, const std::vector&lt; std::vector&lt; int64 &gt; &gt; &amp;weights, const std::vector&lt; int64 &gt; &amp;capacities)</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>Solve</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a7c3b5825c8effd86de03a610d1a38ed7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>BestSolutionContains</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a170ccb08012026c5a438fcf16feb6faa</anchor>
      <arglist>(int item_id) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsSolutionOptimal</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a96f960b29496f637af7a3eeb10e606ab</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>GetName</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>ad2950d2690930dd8562b40d29bb7002e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>use_reduction</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a0408f46c96d0dbf4eb463fa385778593</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_use_reduction</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>abb429376ba12d5541f22a905a728b0e1</anchor>
      <arglist>(bool use_reduction)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_time_limit</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a0f6ca3ef8483871052325ea2d04ca72d</anchor>
      <arglist>(double time_limit_seconds)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::KnapsackState</name>
    <filename>classoperations__research_1_1KnapsackState.html</filename>
    <member kind="function">
      <type></type>
      <name>KnapsackState</name>
      <anchorfile>classoperations__research_1_1KnapsackState.html</anchorfile>
      <anchor>adb083eb4c9bb7413cff7262e58c5abe0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Init</name>
      <anchorfile>classoperations__research_1_1KnapsackState.html</anchorfile>
      <anchor>a6b5c2748ef2668630c5bfdfa56093e95</anchor>
      <arglist>(int number_of_items)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>UpdateState</name>
      <anchorfile>classoperations__research_1_1KnapsackState.html</anchorfile>
      <anchor>a3fa5da9a819f219f4c51e410fe12c1a3</anchor>
      <arglist>(bool revert, const KnapsackAssignment &amp;assignment)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetNumberOfItems</name>
      <anchorfile>classoperations__research_1_1KnapsackState.html</anchorfile>
      <anchor>aa819237f8d1a2dddad038ac0c8efa495</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>is_bound</name>
      <anchorfile>classoperations__research_1_1KnapsackState.html</anchorfile>
      <anchor>ad5b6fe4a0bbfa609c73c8802379d50e5</anchor>
      <arglist>(int id) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>is_in</name>
      <anchorfile>classoperations__research_1_1KnapsackState.html</anchorfile>
      <anchor>a9365eaaee4e8e05325f0f12a50a94297</anchor>
      <arglist>(int id) const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::MergingPartition</name>
    <filename>classoperations__research_1_1MergingPartition.html</filename>
    <member kind="function">
      <type></type>
      <name>MergingPartition</name>
      <anchorfile>classoperations__research_1_1MergingPartition.html</anchorfile>
      <anchor>a561892525aa4b6891d5f7636f62cd749</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MergingPartition</name>
      <anchorfile>classoperations__research_1_1MergingPartition.html</anchorfile>
      <anchor>a2dddfdd1d086883ce9a1306acf647f30</anchor>
      <arglist>(int num_nodes)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Reset</name>
      <anchorfile>classoperations__research_1_1MergingPartition.html</anchorfile>
      <anchor>a2acc4c8e2883189a896862486d160438</anchor>
      <arglist>(int num_nodes)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>NumNodes</name>
      <anchorfile>classoperations__research_1_1MergingPartition.html</anchorfile>
      <anchor>a8e5d14e6155d2a01fe50fc088ee55f3f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>MergePartsOf</name>
      <anchorfile>classoperations__research_1_1MergingPartition.html</anchorfile>
      <anchor>a189669a00c0a94f224db9005c8925310</anchor>
      <arglist>(int node1, int node2)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetRootAndCompressPath</name>
      <anchorfile>classoperations__research_1_1MergingPartition.html</anchorfile>
      <anchor>ab5aff2ca8ee76377b90e75f369a06965</anchor>
      <arglist>(int node)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>KeepOnlyOneNodePerPart</name>
      <anchorfile>classoperations__research_1_1MergingPartition.html</anchorfile>
      <anchor>a09305f65b966f5d00cdb7c5e3fcec43a</anchor>
      <arglist>(std::vector&lt; int &gt; *nodes)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>FillEquivalenceClasses</name>
      <anchorfile>classoperations__research_1_1MergingPartition.html</anchorfile>
      <anchor>a450228c4fe64624a5c075a462ff91290</anchor>
      <arglist>(std::vector&lt; int &gt; *node_equivalence_classes)</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>DebugString</name>
      <anchorfile>classoperations__research_1_1MergingPartition.html</anchorfile>
      <anchor>a62fb42554b2c7db5121b5b4230188d9d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ResetNode</name>
      <anchorfile>classoperations__research_1_1MergingPartition.html</anchorfile>
      <anchor>a955ac764e9c8dcd4358202de4ecd47fd</anchor>
      <arglist>(int node)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>NumNodesInSamePartAs</name>
      <anchorfile>classoperations__research_1_1MergingPartition.html</anchorfile>
      <anchor>a154e7e5b242aa754c41d709d3bc351c1</anchor>
      <arglist>(int node)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetRoot</name>
      <anchorfile>classoperations__research_1_1MergingPartition.html</anchorfile>
      <anchor>a57d8288fbe286978c65abf82bd051000</anchor>
      <arglist>(int node) const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::SparsePermutation</name>
    <filename>classoperations__research_1_1SparsePermutation.html</filename>
    <class kind="struct">operations_research::SparsePermutation::Iterator</class>
    <member kind="function">
      <type></type>
      <name>SparsePermutation</name>
      <anchorfile>classoperations__research_1_1SparsePermutation.html</anchorfile>
      <anchor>ab4ee5c7df12fcaeb042a8c52654569bd</anchor>
      <arglist>(int size)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>Size</name>
      <anchorfile>classoperations__research_1_1SparsePermutation.html</anchorfile>
      <anchor>aff385d18c768ffa364c5dccb5fb35c0a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>NumCycles</name>
      <anchorfile>classoperations__research_1_1SparsePermutation.html</anchorfile>
      <anchor>a4941ad4a0095fa3bc4e8aa0e8fb521bf</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const std::vector&lt; int &gt; &amp;</type>
      <name>Support</name>
      <anchorfile>classoperations__research_1_1SparsePermutation.html</anchorfile>
      <anchor>a0b05b1e840388f9aba5bbe79a392d7d6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>Iterator</type>
      <name>Cycle</name>
      <anchorfile>classoperations__research_1_1SparsePermutation.html</anchorfile>
      <anchor>a59a2f011b1fdc9a63d6557749542c354</anchor>
      <arglist>(int i) const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>LastElementInCycle</name>
      <anchorfile>classoperations__research_1_1SparsePermutation.html</anchorfile>
      <anchor>a9fc0ca5752b5ad6cc2a78ff7c512b22a</anchor>
      <arglist>(int i) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddToCurrentCycle</name>
      <anchorfile>classoperations__research_1_1SparsePermutation.html</anchorfile>
      <anchor>ac1e64e1e738964d338be3cac2b2a58fd</anchor>
      <arglist>(int x)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CloseCurrentCycle</name>
      <anchorfile>classoperations__research_1_1SparsePermutation.html</anchorfile>
      <anchor>ac1e42a48198d2abc8642d81f6b846fee</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>RemoveCycles</name>
      <anchorfile>classoperations__research_1_1SparsePermutation.html</anchorfile>
      <anchor>abca48ca404a02f35f06f295814407100</anchor>
      <arglist>(const std::vector&lt; int &gt; &amp;cycle_indices)</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>DebugString</name>
      <anchorfile>classoperations__research_1_1SparsePermutation.html</anchorfile>
      <anchor>a6886dcada3acf697af20e88ce32a7ae1</anchor>
      <arglist>() const</arglist>
    </member>
  </compound>
  <compound kind="namespace">
    <name>operations_research</name>
    <filename>namespaceoperations__research.html</filename>
    <class kind="class">operations_research::BaseKnapsackSolver</class>
    <class kind="class">operations_research::DenseDoublyLinkedList</class>
    <class kind="class">operations_research::DynamicPartition</class>
    <class kind="class">operations_research::DynamicPermutation</class>
    <class kind="class">operations_research::GraphSymmetryFinder</class>
    <class kind="struct">operations_research::KnapsackAssignment</class>
    <class kind="class">operations_research::KnapsackCapacityPropagator</class>
    <class kind="class">operations_research::KnapsackGenericSolver</class>
    <class kind="struct">operations_research::KnapsackItem</class>
    <class kind="class">operations_research::KnapsackPropagator</class>
    <class kind="class">operations_research::KnapsackSearchNode</class>
    <class kind="class">operations_research::KnapsackSearchPath</class>
    <class kind="class">operations_research::KnapsackSolver</class>
    <class kind="class">operations_research::KnapsackState</class>
    <class kind="class">operations_research::MergingPartition</class>
    <class kind="class">operations_research::SparsePermutation</class>
    <member kind="typedef">
      <type>KnapsackItem *</type>
      <name>KnapsackItemPtr</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab6f3adbb119982fdfb9a85d87310f255</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MinimizeLinearAssignment</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a9c61bb2d6de0894f19675e2110458877</anchor>
      <arglist>(const std::vector&lt; std::vector&lt; double &gt; &gt; &amp;cost, absl::flat_hash_map&lt; int, int &gt; *direct_assignment, absl::flat_hash_map&lt; int, int &gt; *reverse_assignment)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MaximizeLinearAssignment</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ada8ccc36ef736b10ce389fbd347c4282</anchor>
      <arglist>(const std::vector&lt; std::vector&lt; double &gt; &gt; &amp;cost, absl::flat_hash_map&lt; int, int &gt; *direct_assignment, absl::flat_hash_map&lt; int, int &gt; *reverse_assignment)</arglist>
    </member>
    <member kind="function">
      <type>Select next search node to expand Select next item_i to</type>
      <name>assign</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab0de44378545d5727eb4400210e568f8</anchor>
      <arglist>(using master propagator) - Generate a new search node where item_i is in the knapsack - Check validity of this new partial solution(using propagators) - If valid</arglist>
    </member>
    <member kind="function">
      <type>Select next search node to expand Select next item_i to add this new search node to the search Generate a new search node where item_i is not in the knapsack Check validity of this new partial</type>
      <name>solution</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a35a30dc825cd0afa0e095f47118cd3cd</anchor>
      <arglist>(using propagators) - If valid</arglist>
    </member>
  </compound>
</tagfile>
