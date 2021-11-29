/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "CP-SAT", "index.html", [
    [ "Namespaces", "namespaces.html", [
      [ "CpSolverStatus", "namespaceoperations__research_1_1sat.html#aedc4ddb96acc28481c09828d2e016815", [
        [ "UNKNOWN", "namespaceoperations__research_1_1sat.html#aedc4ddb96acc28481c09828d2e016815a6ce26a62afab55d7606ad4e92428b30c", null ],
        [ "MODEL_INVALID", "namespaceoperations__research_1_1sat.html#aedc4ddb96acc28481c09828d2e016815ae071e79c23f061c9dd00ee09519a0031", null ],
        [ "FEASIBLE", "namespaceoperations__research_1_1sat.html#aedc4ddb96acc28481c09828d2e016815a03f919221217f95d21a593a7120165e1", null ],
        [ "INFEASIBLE", "namespaceoperations__research_1_1sat.html#aedc4ddb96acc28481c09828d2e016815a2884fa43446c0cbc9c7a9b74d41d7483", null ],
        [ "OPTIMAL", "namespaceoperations__research_1_1sat.html#aedc4ddb96acc28481c09828d2e016815a2579881e7c83261bc21bafb5a5c92cad", null ],
        [ "CpSolverStatus_INT_MIN_SENTINEL_DO_NOT_USE_", "namespaceoperations__research_1_1sat.html#aedc4ddb96acc28481c09828d2e016815a3c013bc15052315782a00d86f3fca3ab", null ],
        [ "CpSolverStatus_INT_MAX_SENTINEL_DO_NOT_USE_", "namespaceoperations__research_1_1sat.html#aedc4ddb96acc28481c09828d2e016815a3c910aa4be26fdd6efed0262315b1ffd", null ]
      ] ],
      [ "DecisionStrategyProto_DomainReductionStrategy", "namespaceoperations__research_1_1sat.html#a0ab0c4789d577b30dde661c19f88d00e", [
        [ "DecisionStrategyProto_DomainReductionStrategy_SELECT_MIN_VALUE", "namespaceoperations__research_1_1sat.html#a0ab0c4789d577b30dde661c19f88d00ea9c560a476724e955a1f69e4057eaa372", null ],
        [ "DecisionStrategyProto_DomainReductionStrategy_SELECT_MAX_VALUE", "namespaceoperations__research_1_1sat.html#a0ab0c4789d577b30dde661c19f88d00ea9ce19914b81dbcf78ebde3ed15e10b3b", null ],
        [ "DecisionStrategyProto_DomainReductionStrategy_SELECT_LOWER_HALF", "namespaceoperations__research_1_1sat.html#a0ab0c4789d577b30dde661c19f88d00ea76013e303afc2f8b54afdeecd37224d3", null ],
        [ "DecisionStrategyProto_DomainReductionStrategy_SELECT_UPPER_HALF", "namespaceoperations__research_1_1sat.html#a0ab0c4789d577b30dde661c19f88d00eae2ddcc4b888df56eb4300f94b24f8005", null ],
        [ "DecisionStrategyProto_DomainReductionStrategy_SELECT_MEDIAN_VALUE", "namespaceoperations__research_1_1sat.html#a0ab0c4789d577b30dde661c19f88d00ea2dd975e36bdd9ac9e65463fcc4f0541c", null ],
        [ "DecisionStrategyProto_DomainReductionStrategy_DecisionStrategyProto_DomainReductionStrategy_INT_MIN_SENTINEL_DO_NOT_USE_", "namespaceoperations__research_1_1sat.html#a0ab0c4789d577b30dde661c19f88d00ea12f599538c023b465123a3c9cfa9869f", null ],
        [ "DecisionStrategyProto_DomainReductionStrategy_DecisionStrategyProto_DomainReductionStrategy_INT_MAX_SENTINEL_DO_NOT_USE_", "namespaceoperations__research_1_1sat.html#a0ab0c4789d577b30dde661c19f88d00ea610f4d8bf804f4c0261df253a3e06462", null ]
      ] ],
      [ "DecisionStrategyProto_VariableSelectionStrategy", "namespaceoperations__research_1_1sat.html#a94523f1ebceff999bc59a3db7d2b98b6", [
        [ "DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_FIRST", "namespaceoperations__research_1_1sat.html#a94523f1ebceff999bc59a3db7d2b98b6a9cc9a32b4cec62f6bcd8410311de9b51", null ],
        [ "DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_LOWEST_MIN", "namespaceoperations__research_1_1sat.html#a94523f1ebceff999bc59a3db7d2b98b6a3e671416caa639665eb8dcd550940467", null ],
        [ "DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_HIGHEST_MAX", "namespaceoperations__research_1_1sat.html#a94523f1ebceff999bc59a3db7d2b98b6a8a26bd6d9e48e2c4f2f144c021d74d1a", null ],
        [ "DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_MIN_DOMAIN_SIZE", "namespaceoperations__research_1_1sat.html#a94523f1ebceff999bc59a3db7d2b98b6aac0157e9af8921b714667cdaa10d09f0", null ],
        [ "DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_MAX_DOMAIN_SIZE", "namespaceoperations__research_1_1sat.html#a94523f1ebceff999bc59a3db7d2b98b6a3b1dbb74050c9b83b333d6137c47e10b", null ],
        [ "DecisionStrategyProto_VariableSelectionStrategy_DecisionStrategyProto_VariableSelectionStrategy_INT_MIN_SENTINEL_DO_NOT_USE_", "namespaceoperations__research_1_1sat.html#a94523f1ebceff999bc59a3db7d2b98b6a3da8a28b16b5c0072a721e7657f77763", null ],
        [ "DecisionStrategyProto_VariableSelectionStrategy_DecisionStrategyProto_VariableSelectionStrategy_INT_MAX_SENTINEL_DO_NOT_USE_", "namespaceoperations__research_1_1sat.html#a94523f1ebceff999bc59a3db7d2b98b6ae49c5ca9c4434f188df518c8d6d597c1", null ]
      ] ],
      [ "SatParameters_BinaryMinizationAlgorithm", "namespaceoperations__research_1_1sat.html#a7c83cff2372e8b21bda7588f8f46cbd1", [
        [ "SatParameters_BinaryMinizationAlgorithm_NO_BINARY_MINIMIZATION", "namespaceoperations__research_1_1sat.html#a7c83cff2372e8b21bda7588f8f46cbd1ac0651f31c0042e3c1dd2d456cb12af07", null ],
        [ "SatParameters_BinaryMinizationAlgorithm_BINARY_MINIMIZATION_FIRST", "namespaceoperations__research_1_1sat.html#a7c83cff2372e8b21bda7588f8f46cbd1ab08970cf18de7f75841a2d3a44862032", null ],
        [ "SatParameters_BinaryMinizationAlgorithm_BINARY_MINIMIZATION_FIRST_WITH_TRANSITIVE_REDUCTION", "namespaceoperations__research_1_1sat.html#a7c83cff2372e8b21bda7588f8f46cbd1a370200fe72f67822887dfed558c738cb", null ],
        [ "SatParameters_BinaryMinizationAlgorithm_BINARY_MINIMIZATION_WITH_REACHABILITY", "namespaceoperations__research_1_1sat.html#a7c83cff2372e8b21bda7588f8f46cbd1a710594fda58e0131356b03977dedda5a", null ],
        [ "SatParameters_BinaryMinizationAlgorithm_EXPERIMENTAL_BINARY_MINIMIZATION", "namespaceoperations__research_1_1sat.html#a7c83cff2372e8b21bda7588f8f46cbd1a00f0a8716925b175278b9bcb8238a5a1", null ]
      ] ],
      [ "SatParameters_ClauseOrdering", "namespaceoperations__research_1_1sat.html#a3a011c93394882e6e17aa641684bbda3", [
        [ "SatParameters_ClauseOrdering_CLAUSE_ACTIVITY", "namespaceoperations__research_1_1sat.html#a3a011c93394882e6e17aa641684bbda3a62e13be0004504ff2451231f1b897281", null ],
        [ "SatParameters_ClauseOrdering_CLAUSE_LBD", "namespaceoperations__research_1_1sat.html#a3a011c93394882e6e17aa641684bbda3a0f01365a80df297922695c855f948aac", null ]
      ] ],
      [ "SatParameters_ClauseProtection", "namespaceoperations__research_1_1sat.html#afcff2717cc2226f72383b1d027e0d780", [
        [ "SatParameters_ClauseProtection_PROTECTION_NONE", "namespaceoperations__research_1_1sat.html#afcff2717cc2226f72383b1d027e0d780a5ef89d13a60083f2af2e870b8aadee29", null ],
        [ "SatParameters_ClauseProtection_PROTECTION_ALWAYS", "namespaceoperations__research_1_1sat.html#afcff2717cc2226f72383b1d027e0d780a9a77b78ddd79b50c203a0682a40aba88", null ],
        [ "SatParameters_ClauseProtection_PROTECTION_LBD", "namespaceoperations__research_1_1sat.html#afcff2717cc2226f72383b1d027e0d780aca284b7d9b183355a1b049935bba64a4", null ]
      ] ],
      [ "SatParameters_ConflictMinimizationAlgorithm", "namespaceoperations__research_1_1sat.html#a2d3c95989650500f29dd8b993b213043", [
        [ "SatParameters_ConflictMinimizationAlgorithm_NONE", "namespaceoperations__research_1_1sat.html#a2d3c95989650500f29dd8b993b213043a730fa2794fe9e6c8f351460542e1f332", null ],
        [ "SatParameters_ConflictMinimizationAlgorithm_SIMPLE", "namespaceoperations__research_1_1sat.html#a2d3c95989650500f29dd8b993b213043a5930823a544395cdea4af88777795298", null ],
        [ "SatParameters_ConflictMinimizationAlgorithm_RECURSIVE", "namespaceoperations__research_1_1sat.html#a2d3c95989650500f29dd8b993b213043af30e086a564a6f95da4de5c8a7f65d9c", null ],
        [ "SatParameters_ConflictMinimizationAlgorithm_EXPERIMENTAL", "namespaceoperations__research_1_1sat.html#a2d3c95989650500f29dd8b993b213043a59ebf3a2d7eaf5d63b91cef1ab560020", null ]
      ] ],
      [ "SatParameters_FPRoundingMethod", "namespaceoperations__research_1_1sat.html#a236d88ad95ff283caa57f4fe75b0450b", [
        [ "SatParameters_FPRoundingMethod_NEAREST_INTEGER", "namespaceoperations__research_1_1sat.html#a236d88ad95ff283caa57f4fe75b0450bad44af9a22b702d749107b115932d46de", null ],
        [ "SatParameters_FPRoundingMethod_LOCK_BASED", "namespaceoperations__research_1_1sat.html#a236d88ad95ff283caa57f4fe75b0450bad5b360475a5a9b2006383cfff84bde9d", null ],
        [ "SatParameters_FPRoundingMethod_ACTIVE_LOCK_BASED", "namespaceoperations__research_1_1sat.html#a236d88ad95ff283caa57f4fe75b0450ba1822ccdb609df88696eb70f8eec64ae2", null ],
        [ "SatParameters_FPRoundingMethod_PROPAGATION_ASSISTED", "namespaceoperations__research_1_1sat.html#a236d88ad95ff283caa57f4fe75b0450bad4d862bae03bde0138e8065e6bbcd02e", null ]
      ] ],
      [ "SatParameters_MaxSatAssumptionOrder", "namespaceoperations__research_1_1sat.html#a8a5143b55dce052dbcdf222161dabe09", [
        [ "SatParameters_MaxSatAssumptionOrder_DEFAULT_ASSUMPTION_ORDER", "namespaceoperations__research_1_1sat.html#a8a5143b55dce052dbcdf222161dabe09ac75a9c7251bbc0803c5e982fd129030b", null ],
        [ "SatParameters_MaxSatAssumptionOrder_ORDER_ASSUMPTION_BY_DEPTH", "namespaceoperations__research_1_1sat.html#a8a5143b55dce052dbcdf222161dabe09ad52467e5e6eb72d0c5e4aec7d910be98", null ],
        [ "SatParameters_MaxSatAssumptionOrder_ORDER_ASSUMPTION_BY_WEIGHT", "namespaceoperations__research_1_1sat.html#a8a5143b55dce052dbcdf222161dabe09a323520de1090afc183cf8525457dede4", null ]
      ] ],
      [ "SatParameters_MaxSatStratificationAlgorithm", "namespaceoperations__research_1_1sat.html#ad97f7ecb96756f18e1ece010ed44b4df", [
        [ "SatParameters_MaxSatStratificationAlgorithm_STRATIFICATION_NONE", "namespaceoperations__research_1_1sat.html#ad97f7ecb96756f18e1ece010ed44b4dfa36099f2ffa1ef3281b6e29a2b4e0da25", null ],
        [ "SatParameters_MaxSatStratificationAlgorithm_STRATIFICATION_DESCENT", "namespaceoperations__research_1_1sat.html#ad97f7ecb96756f18e1ece010ed44b4dfa82b33b75c8e4fd3fe6dee745d547c9a1", null ],
        [ "SatParameters_MaxSatStratificationAlgorithm_STRATIFICATION_ASCENT", "namespaceoperations__research_1_1sat.html#ad97f7ecb96756f18e1ece010ed44b4dfad516873d596dd539d90b82ec5da8294f", null ]
      ] ],
      [ "SatParameters_Polarity", "namespaceoperations__research_1_1sat.html#aa1fba7d2cdcaea2d0482431bb2138ac2", [
        [ "SatParameters_Polarity_POLARITY_TRUE", "namespaceoperations__research_1_1sat.html#aa1fba7d2cdcaea2d0482431bb2138ac2a93031db672f9c17a806a9a75ac6da3a6", null ],
        [ "SatParameters_Polarity_POLARITY_FALSE", "namespaceoperations__research_1_1sat.html#aa1fba7d2cdcaea2d0482431bb2138ac2a12c4c848eac53f9fefefae7139c1c18b", null ],
        [ "SatParameters_Polarity_POLARITY_RANDOM", "namespaceoperations__research_1_1sat.html#aa1fba7d2cdcaea2d0482431bb2138ac2ab968708771d64cd6caae50ebd7599e3e", null ],
        [ "SatParameters_Polarity_POLARITY_WEIGHTED_SIGN", "namespaceoperations__research_1_1sat.html#aa1fba7d2cdcaea2d0482431bb2138ac2a13a12b1e8d553a2a375a449a4fbb69e5", null ],
        [ "SatParameters_Polarity_POLARITY_REVERSE_WEIGHTED_SIGN", "namespaceoperations__research_1_1sat.html#aa1fba7d2cdcaea2d0482431bb2138ac2acaab30226f28c306a7530ce3ace133c8", null ]
      ] ],
      [ "SatParameters_RestartAlgorithm", "namespaceoperations__research_1_1sat.html#a94ab601b3fd87a63ae2e200a6c665482", [
        [ "SatParameters_RestartAlgorithm_NO_RESTART", "namespaceoperations__research_1_1sat.html#a94ab601b3fd87a63ae2e200a6c665482afe77edd85da7d6e54fe502bd065648e5", null ],
        [ "SatParameters_RestartAlgorithm_LUBY_RESTART", "namespaceoperations__research_1_1sat.html#a94ab601b3fd87a63ae2e200a6c665482afd99e41b01c3d86e00de4b6036d3a03c", null ],
        [ "SatParameters_RestartAlgorithm_DL_MOVING_AVERAGE_RESTART", "namespaceoperations__research_1_1sat.html#a94ab601b3fd87a63ae2e200a6c665482a057761762df19f1b04beec2380226f47", null ],
        [ "SatParameters_RestartAlgorithm_LBD_MOVING_AVERAGE_RESTART", "namespaceoperations__research_1_1sat.html#a94ab601b3fd87a63ae2e200a6c665482ae2aeb7c811ef1240c6a91c6170521b85", null ],
        [ "SatParameters_RestartAlgorithm_FIXED_RESTART", "namespaceoperations__research_1_1sat.html#a94ab601b3fd87a63ae2e200a6c665482a78ec1224f5bd54f88956bb4a123ba634", null ]
      ] ],
      [ "SatParameters_SearchBranching", "namespaceoperations__research_1_1sat.html#a1866199eac0f3efc86ec8d901a6a0bff", [
        [ "SatParameters_SearchBranching_AUTOMATIC_SEARCH", "namespaceoperations__research_1_1sat.html#a1866199eac0f3efc86ec8d901a6a0bffada1cd4a33c050b14dacbb992e70c0060", null ],
        [ "SatParameters_SearchBranching_FIXED_SEARCH", "namespaceoperations__research_1_1sat.html#a1866199eac0f3efc86ec8d901a6a0bffa63a515c6c56086f7463a468b7461bd03", null ],
        [ "SatParameters_SearchBranching_PORTFOLIO_SEARCH", "namespaceoperations__research_1_1sat.html#a1866199eac0f3efc86ec8d901a6a0bffa0c0fcf083bc8f5a7994a00f7781b7c59", null ],
        [ "SatParameters_SearchBranching_LP_SEARCH", "namespaceoperations__research_1_1sat.html#a1866199eac0f3efc86ec8d901a6a0bffadad69c7f7f598999b7ede14d5d1e4390", null ],
        [ "SatParameters_SearchBranching_PSEUDO_COST_SEARCH", "namespaceoperations__research_1_1sat.html#a1866199eac0f3efc86ec8d901a6a0bffa5b4c8de135a9b0107e6ddc9cd505531f", null ],
        [ "SatParameters_SearchBranching_PORTFOLIO_WITH_QUICK_RESTART_SEARCH", "namespaceoperations__research_1_1sat.html#a1866199eac0f3efc86ec8d901a6a0bffab301b8e03cd5b3c2167c6880b72db128", null ],
        [ "SatParameters_SearchBranching_HINT_SEARCH", "namespaceoperations__research_1_1sat.html#a1866199eac0f3efc86ec8d901a6a0bffa4dbde43fb7659e228d83f4e9c9420c30", null ]
      ] ],
      [ "SatParameters_VariableOrder", "namespaceoperations__research_1_1sat.html#a7457979a394e7bbe88562849cf43b20c", [
        [ "SatParameters_VariableOrder_IN_ORDER", "namespaceoperations__research_1_1sat.html#a7457979a394e7bbe88562849cf43b20ca798a3459d6bcf3b5a2f62ee09630666d", null ],
        [ "SatParameters_VariableOrder_IN_REVERSE_ORDER", "namespaceoperations__research_1_1sat.html#a7457979a394e7bbe88562849cf43b20cae7fc0163217be265036cf0e6f9e4762e", null ],
        [ "SatParameters_VariableOrder_IN_RANDOM_ORDER", "namespaceoperations__research_1_1sat.html#a7457979a394e7bbe88562849cf43b20ca076ca067a18df3fe2c1bf3215a8f7f95", null ]
      ] ],
      [ "CpModelStats", "namespaceoperations__research_1_1sat.html#a287579e5f181fc7c89feccf1128faffb", null ],
      [ "CpSolverResponseStats", "namespaceoperations__research_1_1sat.html#a74d5c1c69142b6e668f8bc7de4e0ec7e", null ],
      [ "CpSolverStatus_descriptor", "namespaceoperations__research_1_1sat.html#a2962df9005923f600a22922facca338d", null ],
      [ "CpSolverStatus_IsValid", "namespaceoperations__research_1_1sat.html#a8f7f7995f8e9a03c15cdddf39b675702", null ],
      [ "CpSolverStatus_Name", "namespaceoperations__research_1_1sat.html#aaea2a71a5a51dc4c838286e316040803", null ],
      [ "CpSolverStatus_Parse", "namespaceoperations__research_1_1sat.html#ad80554b07cb275a8f8e4b2bc6f38cd97", null ],
      [ "DecisionStrategyProto_DomainReductionStrategy_descriptor", "namespaceoperations__research_1_1sat.html#ad61052221d25c4a96e8b80c840ececb7", null ],
      [ "DecisionStrategyProto_DomainReductionStrategy_IsValid", "namespaceoperations__research_1_1sat.html#af161ecb897e60ce83c87c17d11ae7d91", null ],
      [ "DecisionStrategyProto_DomainReductionStrategy_Name", "namespaceoperations__research_1_1sat.html#a06d4ad878766b595107a0a51b67542e5", null ],
      [ "DecisionStrategyProto_DomainReductionStrategy_Parse", "namespaceoperations__research_1_1sat.html#a78f07b013d1f3f208298db7cd977e86d", null ],
      [ "DecisionStrategyProto_VariableSelectionStrategy_descriptor", "namespaceoperations__research_1_1sat.html#a664838438d6500fd09c6c85dc87b409e", null ],
      [ "DecisionStrategyProto_VariableSelectionStrategy_IsValid", "namespaceoperations__research_1_1sat.html#a9644b126f05b927a27fc7eba8e62dd57", null ],
      [ "DecisionStrategyProto_VariableSelectionStrategy_Name", "namespaceoperations__research_1_1sat.html#a9ae9aa8191f1ec3781610b259bb9e8d8", null ],
      [ "DecisionStrategyProto_VariableSelectionStrategy_Parse", "namespaceoperations__research_1_1sat.html#a417dc46af8b9457ba372ce439154f86f", null ],
      [ "NewFeasibleSolutionObserver", "namespaceoperations__research_1_1sat.html#a0558490428f07c5db7b6ae292001375c", null ],
      [ "NewSatParameters", "namespaceoperations__research_1_1sat.html#adbf4fa68898b3aaa2e6de2b5d3064580", null ],
      [ "NewSatParameters", "namespaceoperations__research_1_1sat.html#a10700832ca6bc420f2931eb707957b0b", null ],
      [ "Not", "namespaceoperations__research_1_1sat.html#a5e3de118c1f8dd5a7ec21704e05684b9", null ],
      [ "operator<<", "namespaceoperations__research_1_1sat.html#a807d4ae4dc98ad0c05fa05c3f1dfabc9", null ],
      [ "operator<<", "namespaceoperations__research_1_1sat.html#afeefd0a183a2d1c9f09fec0aa52b200a", null ],
      [ "operator<<", "namespaceoperations__research_1_1sat.html#a0c1b0a196a70f7edd0ff1bc0250e76ac", null ],
      [ "operator<<", "namespaceoperations__research_1_1sat.html#a616a1843aa394d2d018e052050588bb2", null ],
      [ "SatParameters_BinaryMinizationAlgorithm_descriptor", "namespaceoperations__research_1_1sat.html#addd2ecc793fb95bbc92485ada7b1537f", null ],
      [ "SatParameters_BinaryMinizationAlgorithm_IsValid", "namespaceoperations__research_1_1sat.html#a3e37f554c39fbb05faf07674ac550f47", null ],
      [ "SatParameters_BinaryMinizationAlgorithm_Name", "namespaceoperations__research_1_1sat.html#a50b3b370b558be05c0094fe791eb1512", null ],
      [ "SatParameters_BinaryMinizationAlgorithm_Parse", "namespaceoperations__research_1_1sat.html#a46dc96bbf62dd45b444749fdf29ef505", null ],
      [ "SatParameters_ClauseOrdering_descriptor", "namespaceoperations__research_1_1sat.html#a7fe460e2ac472ff6b9101c159ba63052", null ],
      [ "SatParameters_ClauseOrdering_IsValid", "namespaceoperations__research_1_1sat.html#aa6f7c43106217e8a55877110b7d87e7c", null ],
      [ "SatParameters_ClauseOrdering_Name", "namespaceoperations__research_1_1sat.html#a65d2f86169c98d15d223fc48cd815022", null ],
      [ "SatParameters_ClauseOrdering_Parse", "namespaceoperations__research_1_1sat.html#a5fee897ccb9f9ce0d0beaab6cbe73f29", null ],
      [ "SatParameters_ClauseProtection_descriptor", "namespaceoperations__research_1_1sat.html#a6c16e91d165e1c1c7fc96468ce75e4b5", null ],
      [ "SatParameters_ClauseProtection_IsValid", "namespaceoperations__research_1_1sat.html#ac1aa9d5ea93fbc96a68237c2beda3836", null ],
      [ "SatParameters_ClauseProtection_Name", "namespaceoperations__research_1_1sat.html#a670d5436afa6d3ab242c2a9144815ae2", null ],
      [ "SatParameters_ClauseProtection_Parse", "namespaceoperations__research_1_1sat.html#a2417cda476d3921aa1f41416b0e5ecd4", null ],
      [ "SatParameters_ConflictMinimizationAlgorithm_descriptor", "namespaceoperations__research_1_1sat.html#a39c591c8b069d85e5e39dde786b6b07c", null ],
      [ "SatParameters_ConflictMinimizationAlgorithm_IsValid", "namespaceoperations__research_1_1sat.html#a90d6f173fbfa33e26ff6508013c81ffd", null ],
      [ "SatParameters_ConflictMinimizationAlgorithm_Name", "namespaceoperations__research_1_1sat.html#aa1c00db4e701713c238ef4f063fea3f1", null ],
      [ "SatParameters_ConflictMinimizationAlgorithm_Parse", "namespaceoperations__research_1_1sat.html#ad84bafa3f87aeb7662b19cc70e2155ee", null ],
      [ "SatParameters_FPRoundingMethod_descriptor", "namespaceoperations__research_1_1sat.html#a15befb716f89bf0c165b799d3c1c615f", null ],
      [ "SatParameters_FPRoundingMethod_IsValid", "namespaceoperations__research_1_1sat.html#a2e999de0fc3558bd2002a1a472a2214f", null ],
      [ "SatParameters_FPRoundingMethod_Name", "namespaceoperations__research_1_1sat.html#a145fa00b451e55cdefd2a668eb9d9bb3", null ],
      [ "SatParameters_FPRoundingMethod_Parse", "namespaceoperations__research_1_1sat.html#afe9841ddf9445cb321a9d4e630fe22aa", null ],
      [ "SatParameters_MaxSatAssumptionOrder_descriptor", "namespaceoperations__research_1_1sat.html#aacb298c9ec646c51cbb64da889e561b8", null ],
      [ "SatParameters_MaxSatAssumptionOrder_IsValid", "namespaceoperations__research_1_1sat.html#a4104fcd7cb88b2edc4cbc86e6b331cdf", null ],
      [ "SatParameters_MaxSatAssumptionOrder_Name", "namespaceoperations__research_1_1sat.html#a2d28086235c4bce7aeb04976ede987ae", null ],
      [ "SatParameters_MaxSatAssumptionOrder_Parse", "namespaceoperations__research_1_1sat.html#aacf99a68c013178918b84f1efd823a05", null ],
      [ "SatParameters_MaxSatStratificationAlgorithm_descriptor", "namespaceoperations__research_1_1sat.html#a9c7494bde0321d5014f2ba2270590f79", null ],
      [ "SatParameters_MaxSatStratificationAlgorithm_IsValid", "namespaceoperations__research_1_1sat.html#a5fcee51ba7784a7c403731301af6e14c", null ],
      [ "SatParameters_MaxSatStratificationAlgorithm_Name", "namespaceoperations__research_1_1sat.html#a59f1c278d7a5008c4c915a5de0047e71", null ],
      [ "SatParameters_MaxSatStratificationAlgorithm_Parse", "namespaceoperations__research_1_1sat.html#a26b98c6b5f2239d22b80a2f0ad5c30da", null ],
      [ "SatParameters_Polarity_descriptor", "namespaceoperations__research_1_1sat.html#adb7ff75a6d4b5043d5bede0df3e21996", null ],
      [ "SatParameters_Polarity_IsValid", "namespaceoperations__research_1_1sat.html#a4585806adf77d6f7a56bd21230a31175", null ],
      [ "SatParameters_Polarity_Name", "namespaceoperations__research_1_1sat.html#ae1913d4540dcfa1caacca789f44072a9", null ],
      [ "SatParameters_Polarity_Parse", "namespaceoperations__research_1_1sat.html#a156dff4b5d8c6e564c5330b0a6e491ab", null ],
      [ "SatParameters_RestartAlgorithm_descriptor", "namespaceoperations__research_1_1sat.html#ae856f1b94c992444c2c77c6a8a5226a7", null ],
      [ "SatParameters_RestartAlgorithm_IsValid", "namespaceoperations__research_1_1sat.html#ab199957e5457d8356687f12d67d1aaac", null ],
      [ "SatParameters_RestartAlgorithm_Name", "namespaceoperations__research_1_1sat.html#aef8e329e31b024d3167143164a46a240", null ],
      [ "SatParameters_RestartAlgorithm_Parse", "namespaceoperations__research_1_1sat.html#a0ebb6c61a4f4a5d656a078f0a90e0c13", null ],
      [ "SatParameters_SearchBranching_descriptor", "namespaceoperations__research_1_1sat.html#a13bb055e7a2326c5b67e1ae13e8fe550", null ],
      [ "SatParameters_SearchBranching_IsValid", "namespaceoperations__research_1_1sat.html#a9018824bcc1b169f32af87ad4faf7561", null ],
      [ "SatParameters_SearchBranching_Name", "namespaceoperations__research_1_1sat.html#acd5e8cd7198780ef361ab51e20533a09", null ],
      [ "SatParameters_SearchBranching_Parse", "namespaceoperations__research_1_1sat.html#ae2ab630d09edd89ab0d5085736216e1a", null ],
      [ "SatParameters_VariableOrder_descriptor", "namespaceoperations__research_1_1sat.html#a40b65ae6507773e0376ba10e646a6038", null ],
      [ "SatParameters_VariableOrder_IsValid", "namespaceoperations__research_1_1sat.html#a711b59624fbd706f0754647084c665d8", null ],
      [ "SatParameters_VariableOrder_Name", "namespaceoperations__research_1_1sat.html#a238fbc2472f81fbba74743f5589b69b4", null ],
      [ "SatParameters_VariableOrder_Parse", "namespaceoperations__research_1_1sat.html#adff74d54012d9ac2684d6cea57d6afb7", null ],
      [ "SolutionBooleanValue", "namespaceoperations__research_1_1sat.html#afa415e372a9d64eede869ed98666c29c", null ],
      [ "SolutionIntegerMax", "namespaceoperations__research_1_1sat.html#a0205c1cb83c849b1f47dab55ad6ada5c", null ],
      [ "SolutionIntegerMin", "namespaceoperations__research_1_1sat.html#af7857084f34282d9c30370db7d63faa7", null ],
      [ "SolutionIntegerValue", "namespaceoperations__research_1_1sat.html#ab6fe86bc876c281163a053a9581346c3", null ],
      [ "Solve", "namespaceoperations__research_1_1sat.html#a09d851f944ab4f305c3d9f8df99b7bf8", null ],
      [ "SolveCpModel", "namespaceoperations__research_1_1sat.html#a9d67b9c66f1cb9c1dcc3415cd5af11bf", null ],
      [ "SolveWithParameters", "namespaceoperations__research_1_1sat.html#aa3062797aa0396abf37dbcc99a746f12", null ],
      [ "SolveWithParameters", "namespaceoperations__research_1_1sat.html#af52c27ecb43d6486c1a70e022b4aad39", null ],
      [ "_AllDifferentConstraintProto_default_instance_", "namespaceoperations__research_1_1sat.html#a5022cdbf0a4511363b80caf245fb2854", null ],
      [ "_AutomatonConstraintProto_default_instance_", "namespaceoperations__research_1_1sat.html#abf09b453afccb2f50177ffcd3dfbd27a", null ],
      [ "_BoolArgumentProto_default_instance_", "namespaceoperations__research_1_1sat.html#a655d0feb045c5101029918aae3cead88", null ],
      [ "_BooleanAssignment_default_instance_", "namespaceoperations__research_1_1sat.html#a440252e6a87c7ee2f290f750e4520326", null ],
      [ "_CircuitConstraintProto_default_instance_", "namespaceoperations__research_1_1sat.html#aea6eecafe1713c4565393c72a379122b", null ],
      [ "_ConstraintProto_default_instance_", "namespaceoperations__research_1_1sat.html#af34fca4c31d5c2ab765ec99b192bab6a", null ],
      [ "_CpModelProto_default_instance_", "namespaceoperations__research_1_1sat.html#a41f9dc3f87845be93073600332540b9c", null ],
      [ "_CpObjectiveProto_default_instance_", "namespaceoperations__research_1_1sat.html#a223479678a6c4c9d8b47a77db02b914d", null ],
      [ "_CpSolverResponse_default_instance_", "namespaceoperations__research_1_1sat.html#a8cc08aed16e89a81f7cde799a790a3d1", null ],
      [ "_CumulativeConstraintProto_default_instance_", "namespaceoperations__research_1_1sat.html#aee153940e8ef35428c50dd448409c6f2", null ],
      [ "_DecisionStrategyProto_AffineTransformation_default_instance_", "namespaceoperations__research_1_1sat.html#a42bfe2cdee7a1e2cecf327269512932d", null ],
      [ "_DecisionStrategyProto_default_instance_", "namespaceoperations__research_1_1sat.html#a6e1584f6741309b50e2db39c1670f097", null ],
      [ "_DenseMatrixProto_default_instance_", "namespaceoperations__research_1_1sat.html#ae3a02742030c124a9b6029a2397376a2", null ],
      [ "_ElementConstraintProto_default_instance_", "namespaceoperations__research_1_1sat.html#ab8a8f5cff578449bc5514c4df3e823b6", null ],
      [ "_IntegerArgumentProto_default_instance_", "namespaceoperations__research_1_1sat.html#a37ff576212cf01bac7c37c4bfb670511", null ],
      [ "_IntegerVariableProto_default_instance_", "namespaceoperations__research_1_1sat.html#a3bef289699c48c5389fa270be5b59c3a", null ],
      [ "_IntervalConstraintProto_default_instance_", "namespaceoperations__research_1_1sat.html#a5f4bbdfa15618bf8312bf8e6cc742097", null ],
      [ "_InverseConstraintProto_default_instance_", "namespaceoperations__research_1_1sat.html#a221276dc5424848b110e741e47d11c12", null ],
      [ "_LinearArgumentProto_default_instance_", "namespaceoperations__research_1_1sat.html#a2e813d01ac2d5d980dc56019ce6a40cb", null ],
      [ "_LinearBooleanConstraint_default_instance_", "namespaceoperations__research_1_1sat.html#a44c77c642d778b82561e14d4d88fe982", null ],
      [ "_LinearBooleanProblem_default_instance_", "namespaceoperations__research_1_1sat.html#aecb63fab84829b29e02f6481963463f5", null ],
      [ "_LinearConstraintProto_default_instance_", "namespaceoperations__research_1_1sat.html#a7fe2d41dc416b27433f95a6c2b060338", null ],
      [ "_LinearExpressionProto_default_instance_", "namespaceoperations__research_1_1sat.html#ac65a4238749ba58cb9d108e4441b8a3c", null ],
      [ "_LinearObjective_default_instance_", "namespaceoperations__research_1_1sat.html#acff383180e0a501ae04bf4a31adfe5a2", null ],
      [ "_ListOfVariablesProto_default_instance_", "namespaceoperations__research_1_1sat.html#af6433eb0c6144d7db88633b9369381ea", null ],
      [ "_NoOverlap2DConstraintProto_default_instance_", "namespaceoperations__research_1_1sat.html#a5682de205955fe92a2903141da1737ca", null ],
      [ "_NoOverlapConstraintProto_default_instance_", "namespaceoperations__research_1_1sat.html#a921e9c249f7c69309d0c2712f78867a2", null ],
      [ "_PartialVariableAssignment_default_instance_", "namespaceoperations__research_1_1sat.html#ae5c743ef0c2dfd8d383ec4a665b31af1", null ],
      [ "_ReservoirConstraintProto_default_instance_", "namespaceoperations__research_1_1sat.html#afa291a640d37abb7a53142f7b4acba70", null ],
      [ "_RoutesConstraintProto_default_instance_", "namespaceoperations__research_1_1sat.html#a614d5011dd52e4c6e4643497410deadb", null ],
      [ "_SatParameters_default_instance_", "namespaceoperations__research_1_1sat.html#a25a5bc8a1aaa31b67951f61ecd67c18a", null ],
      [ "_SparsePermutationProto_default_instance_", "namespaceoperations__research_1_1sat.html#a03d88ac28fa6a2504710e1dd6fb9c3b0", null ],
      [ "_SymmetryProto_default_instance_", "namespaceoperations__research_1_1sat.html#aba0c8ba09a0f2d756522bfe2ba5e41c6", null ],
      [ "_TableConstraintProto_default_instance_", "namespaceoperations__research_1_1sat.html#aecd8e85982106d608b8b23678f1508f7", null ],
      [ "CpSolverStatus_ARRAYSIZE", "namespaceoperations__research_1_1sat.html#a74dd1a529939101db35e9d731ffac186", null ],
      [ "CpSolverStatus_MAX", "namespaceoperations__research_1_1sat.html#aaa8ca38a83038dce1f21a6ff727d9cd4", null ],
      [ "CpSolverStatus_MIN", "namespaceoperations__research_1_1sat.html#a6b76cd25015012648a3d14bc20d7f0bd", null ],
      [ "DecisionStrategyProto_DomainReductionStrategy_DomainReductionStrategy_ARRAYSIZE", "namespaceoperations__research_1_1sat.html#abc149d79ce813acfacf966a6f0114f9a", null ],
      [ "DecisionStrategyProto_DomainReductionStrategy_DomainReductionStrategy_MAX", "namespaceoperations__research_1_1sat.html#a32d06c0a033135b152dc6aaa0cce11cb", null ],
      [ "DecisionStrategyProto_DomainReductionStrategy_DomainReductionStrategy_MIN", "namespaceoperations__research_1_1sat.html#ae812a198d8b85b66696afdc8a7f21480", null ],
      [ "DecisionStrategyProto_VariableSelectionStrategy_VariableSelectionStrategy_ARRAYSIZE", "namespaceoperations__research_1_1sat.html#a31ba6359043b091cd5c02ff98f8dafa1", null ],
      [ "DecisionStrategyProto_VariableSelectionStrategy_VariableSelectionStrategy_MAX", "namespaceoperations__research_1_1sat.html#a04487ffe93d385896ec57f978f248a1f", null ],
      [ "DecisionStrategyProto_VariableSelectionStrategy_VariableSelectionStrategy_MIN", "namespaceoperations__research_1_1sat.html#ae21dd421323a77bde4c9253b6255c785", null ],
      [ "SatParameters_BinaryMinizationAlgorithm_BinaryMinizationAlgorithm_ARRAYSIZE", "namespaceoperations__research_1_1sat.html#ae171e60f6d49e497f15e596d7411f708", null ],
      [ "SatParameters_BinaryMinizationAlgorithm_BinaryMinizationAlgorithm_MAX", "namespaceoperations__research_1_1sat.html#a42624dc671d813edb4e1c17c4c398a68", null ],
      [ "SatParameters_BinaryMinizationAlgorithm_BinaryMinizationAlgorithm_MIN", "namespaceoperations__research_1_1sat.html#ae74dd8c0974dea7aa003eb0c930419eb", null ],
      [ "SatParameters_ClauseOrdering_ClauseOrdering_ARRAYSIZE", "namespaceoperations__research_1_1sat.html#a021bea2496cf30a913a3b7b9486ed4da", null ],
      [ "SatParameters_ClauseOrdering_ClauseOrdering_MAX", "namespaceoperations__research_1_1sat.html#aad635fe5bf7f4edaa53c84f45ef48389", null ],
      [ "SatParameters_ClauseOrdering_ClauseOrdering_MIN", "namespaceoperations__research_1_1sat.html#a052be0d1fc9671cd3306f1491ac11795", null ],
      [ "SatParameters_ClauseProtection_ClauseProtection_ARRAYSIZE", "namespaceoperations__research_1_1sat.html#a11061897413266dd8ab3ac870a0a4a25", null ],
      [ "SatParameters_ClauseProtection_ClauseProtection_MAX", "namespaceoperations__research_1_1sat.html#a05c95ab3c9b6a4e1989446f01979fde4", null ],
      [ "SatParameters_ClauseProtection_ClauseProtection_MIN", "namespaceoperations__research_1_1sat.html#a2d8b8347bd8e9c8991f5b438e14af38a", null ],
      [ "SatParameters_ConflictMinimizationAlgorithm_ConflictMinimizationAlgorithm_ARRAYSIZE", "namespaceoperations__research_1_1sat.html#a9a3e73b1b8cf708b5cf35058d85d28b7", null ],
      [ "SatParameters_ConflictMinimizationAlgorithm_ConflictMinimizationAlgorithm_MAX", "namespaceoperations__research_1_1sat.html#aaf5139a06a25ff8dbc6bc1bf5151b25f", null ],
      [ "SatParameters_ConflictMinimizationAlgorithm_ConflictMinimizationAlgorithm_MIN", "namespaceoperations__research_1_1sat.html#a4de38fe554fc6866f2e44972ceca7b25", null ],
      [ "SatParameters_FPRoundingMethod_FPRoundingMethod_ARRAYSIZE", "namespaceoperations__research_1_1sat.html#aabd42fb6ed5c7d3a8f1c139bfdc3582a", null ],
      [ "SatParameters_FPRoundingMethod_FPRoundingMethod_MAX", "namespaceoperations__research_1_1sat.html#a8bd0979a47a65468fc6d6b0a83fcb91d", null ],
      [ "SatParameters_FPRoundingMethod_FPRoundingMethod_MIN", "namespaceoperations__research_1_1sat.html#a7d6aa5fab75f3c10c95e5cadf4272c1f", null ],
      [ "SatParameters_MaxSatAssumptionOrder_MaxSatAssumptionOrder_ARRAYSIZE", "namespaceoperations__research_1_1sat.html#af71c4d06c43be88645380f4fa01ccbe2", null ],
      [ "SatParameters_MaxSatAssumptionOrder_MaxSatAssumptionOrder_MAX", "namespaceoperations__research_1_1sat.html#a287313110907019189102e6a425db7d6", null ],
      [ "SatParameters_MaxSatAssumptionOrder_MaxSatAssumptionOrder_MIN", "namespaceoperations__research_1_1sat.html#abac2fd696ab95863658458d5de6417ab", null ],
      [ "SatParameters_MaxSatStratificationAlgorithm_MaxSatStratificationAlgorithm_ARRAYSIZE", "namespaceoperations__research_1_1sat.html#abd8780a816b1cf20e935ba67607bac0c", null ],
      [ "SatParameters_MaxSatStratificationAlgorithm_MaxSatStratificationAlgorithm_MAX", "namespaceoperations__research_1_1sat.html#a88c6e37e9f5c881ab71399a5a356a5f7", null ],
      [ "SatParameters_MaxSatStratificationAlgorithm_MaxSatStratificationAlgorithm_MIN", "namespaceoperations__research_1_1sat.html#aa245f61e6a0078511d811afed295d34f", null ],
      [ "SatParameters_Polarity_Polarity_ARRAYSIZE", "namespaceoperations__research_1_1sat.html#af5fc6fa213f90d8c1abe109e3e82ca3e", null ],
      [ "SatParameters_Polarity_Polarity_MAX", "namespaceoperations__research_1_1sat.html#a362989e72881f70bdf61e7507b97623d", null ],
      [ "SatParameters_Polarity_Polarity_MIN", "namespaceoperations__research_1_1sat.html#a6474747ed78c56627b1ffd4767b3a11a", null ],
      [ "SatParameters_RestartAlgorithm_RestartAlgorithm_ARRAYSIZE", "namespaceoperations__research_1_1sat.html#a3e9e15d4472972d5b26b7eebf6c9c62e", null ],
      [ "SatParameters_RestartAlgorithm_RestartAlgorithm_MAX", "namespaceoperations__research_1_1sat.html#ab0b0301295683516f07c69d6eb8d25e1", null ],
      [ "SatParameters_RestartAlgorithm_RestartAlgorithm_MIN", "namespaceoperations__research_1_1sat.html#ac13fa765cc171fb796beef804d90dfe2", null ],
      [ "SatParameters_SearchBranching_SearchBranching_ARRAYSIZE", "namespaceoperations__research_1_1sat.html#a33cc002767cfe78bc8c170cb6e56cc7d", null ],
      [ "SatParameters_SearchBranching_SearchBranching_MAX", "namespaceoperations__research_1_1sat.html#aa9fab6c25b48bce868385ea04e17a444", null ],
      [ "SatParameters_SearchBranching_SearchBranching_MIN", "namespaceoperations__research_1_1sat.html#aea3d7eadc6bb30c4184c05f12dfdc0c9", null ],
      [ "SatParameters_VariableOrder_VariableOrder_ARRAYSIZE", "namespaceoperations__research_1_1sat.html#ab4b0493580311e8cf5ff9a1e507be76e", null ],
      [ "SatParameters_VariableOrder_VariableOrder_MAX", "namespaceoperations__research_1_1sat.html#a4c1b3b893b2e69e2c1fed676a459eb5d", null ],
      [ "SatParameters_VariableOrder_VariableOrder_MIN", "namespaceoperations__research_1_1sat.html#a4363184e7c0101cfbf4ae17dc10288ed", null ],
      [ "IntervalsAreSortedAndNonAdjacent", "namespaceoperations__research.html#ab8c23924c4b61ed5c531424a6f18bde1", null ],
      [ "operator<<", "namespaceoperations__research.html#aa0a912cba095e3c41bffc8d447be109c", null ],
      [ "operator<<", "namespaceoperations__research.html#ae63c644a083ff23befa7a5bf51758afb", null ],
      [ "operator<<", "namespaceoperations__research.html#a915518d4960abbd5e49852e0e00614a3", null ],
      [ "SumOfKMaxValueInDomain", "namespaceoperations__research.html#aeab50e8a6cadc29a421918a966df360f", null ],
      [ "SumOfKMinValueInDomain", "namespaceoperations__research.html#a6fb2cc3382534da86167bf6644e057e7", null ]
    ] ],
    [ "Classes", "annotated.html", null ],
    [ "Files", "files.html", null ]
  ] ]
];

var NAVTREEINDEX =
[
"index.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';