<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile>
  <compound kind="file">
    <name>dense_doubly_linked_list.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/algorithms/</path>
    <filename>dense__doubly__linked__list_8h</filename>
    <namespace>operations_research</namespace>
  </compound>
  <compound kind="file">
    <name>dynamic_partition.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/algorithms/</path>
    <filename>dynamic__partition_8h</filename>
    <namespace>operations_research</namespace>
  </compound>
  <compound kind="file">
    <name>dynamic_permutation.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/algorithms/</path>
    <filename>dynamic__permutation_8h</filename>
    <namespace>operations_research</namespace>
  </compound>
  <compound kind="file">
    <name>find_graph_symmetries.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/algorithms/</path>
    <filename>find__graph__symmetries_8h</filename>
    <includes id="dynamic__partition_8h" name="dynamic_partition.h" local="yes" imported="no">ortools/algorithms/dynamic_partition.h</includes>
    <includes id="dynamic__permutation_8h" name="dynamic_permutation.h" local="yes" imported="no">ortools/algorithms/dynamic_permutation.h</includes>
    <namespace>operations_research</namespace>
  </compound>
  <compound kind="file">
    <name>hungarian.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/algorithms/</path>
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
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/algorithms/</path>
    <filename>knapsack__solver_8h</filename>
    <class kind="class">operations_research::KnapsackSolver</class>
    <namespace>operations_research</namespace>
  </compound>
  <compound kind="file">
    <name>sparse_permutation.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/algorithms/</path>
    <filename>sparse__permutation_8h</filename>
    <namespace>operations_research</namespace>
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
      <name>KNAPSACK_MULTIDIMENSION_CBC_MIP_SOLVER</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a81ce17438663c39f7793e8db92ff1893ae1b7e0ea856a376a9c04130e0abdf812</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a81ce17438663c39f7793e8db92ff1893ae8c15c25eaf606f5f853821aaabba164</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KNAPSACK_MULTIDIMENSION_SCIP_MIP_SOLVER</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a81ce17438663c39f7793e8db92ff1893aa6d83fc5b8d17db1f82c1b414a4e8b20</anchor>
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
      <name>KNAPSACK_MULTIDIMENSION_CBC_MIP_SOLVER</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a81ce17438663c39f7793e8db92ff1893ae1b7e0ea856a376a9c04130e0abdf812</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a81ce17438663c39f7793e8db92ff1893ae8c15c25eaf606f5f853821aaabba164</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KNAPSACK_MULTIDIMENSION_SCIP_MIP_SOLVER</name>
      <anchorfile>classoperations__research_1_1KnapsackSolver.html</anchorfile>
      <anchor>a81ce17438663c39f7793e8db92ff1893aa6d83fc5b8d17db1f82c1b414a4e8b20</anchor>
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
  <compound kind="namespace">
    <name>operations_research</name>
    <filename>namespaceoperations__research.html</filename>
    <class kind="class">operations_research::KnapsackSolver</class>
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
</tagfile>
