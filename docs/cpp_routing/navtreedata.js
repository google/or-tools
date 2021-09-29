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
  [ "Routing", "index.html", [
    [ "Namespaces", "namespaces.html", [
      [ "RoutingIndexPair", "namespaceoperations__research.html#a630fe793e232b361cd9fd99f18599df1", null ],
      [ "RoutingIndexPairs", "namespaceoperations__research.html#aef7db0bee0a22d1791d040fd3853f3b7", null ],
      [ "RoutingTransitCallback1", "namespaceoperations__research.html#aae02b84a58c3008fb747c0f6917bfe6c", null ],
      [ "RoutingTransitCallback2", "namespaceoperations__research.html#a26868b9d744edcd8d59145e068678885", null ],
      [ "SequenceVarLocalSearchOperatorTemplate", "namespaceoperations__research.html#ad502b08bb4d69dfbaf025415310b8da8", null ],
      [ "ConstraintSolverParameters_TrailCompression", "namespaceoperations__research.html#ac5e380bc50cb14374c22d16ed40a8422", [
        [ "ConstraintSolverParameters_TrailCompression_NO_COMPRESSION", "namespaceoperations__research.html#ac5e380bc50cb14374c22d16ed40a8422a9f5b4ac9f746c5e1a5c22a3a4ec733da", null ],
        [ "ConstraintSolverParameters_TrailCompression_COMPRESS_WITH_ZLIB", "namespaceoperations__research.html#ac5e380bc50cb14374c22d16ed40a8422a084bffc16d26b51902734151ee0e7cef", null ],
        [ "ConstraintSolverParameters_TrailCompression_ConstraintSolverParameters_TrailCompression_INT_MIN_SENTINEL_DO_NOT_USE_", "namespaceoperations__research.html#ac5e380bc50cb14374c22d16ed40a8422a73aba6d2e66d5d3c676a9f4f901c1f4b", null ],
        [ "ConstraintSolverParameters_TrailCompression_ConstraintSolverParameters_TrailCompression_INT_MAX_SENTINEL_DO_NOT_USE_", "namespaceoperations__research.html#ac5e380bc50cb14374c22d16ed40a8422a58218851ba5bf9598c535edd93376fc0", null ]
      ] ],
      [ "DimensionSchedulingStatus", "namespaceoperations__research.html#aa0787bf78fb09d1e30f2451b5a68d4b8", [
        [ "OPTIMAL", "namespaceoperations__research.html#aa0787bf78fb09d1e30f2451b5a68d4b8af00c8dbdd6e1f11bdae06be94277d293", null ],
        [ "RELAXED_OPTIMAL_ONLY", "namespaceoperations__research.html#aa0787bf78fb09d1e30f2451b5a68d4b8af8cfb2115ef7ab822bca8edd1edac285", null ],
        [ "INFEASIBLE", "namespaceoperations__research.html#aa0787bf78fb09d1e30f2451b5a68d4b8a6faaca695f728b47f47dd389f31e4a93", null ]
      ] ],
      [ "FirstSolutionStrategy_Value", "namespaceoperations__research.html#a49e7da620d9baa1bb2715b89fcbc307c", [
        [ "FirstSolutionStrategy_Value_UNSET", "namespaceoperations__research.html#a49e7da620d9baa1bb2715b89fcbc307ca31c43e778aca17f824b8af4ab2e42381", null ],
        [ "FirstSolutionStrategy_Value_AUTOMATIC", "namespaceoperations__research.html#a49e7da620d9baa1bb2715b89fcbc307ca5390ddfbc0c9be09a0c1016290ed801d", null ],
        [ "FirstSolutionStrategy_Value_PATH_CHEAPEST_ARC", "namespaceoperations__research.html#a49e7da620d9baa1bb2715b89fcbc307cab5f1b5d9f869690d2db7153179c41aba", null ],
        [ "FirstSolutionStrategy_Value_PATH_MOST_CONSTRAINED_ARC", "namespaceoperations__research.html#a49e7da620d9baa1bb2715b89fcbc307cae07145d13cc6775478804a5969b1cfd2", null ],
        [ "FirstSolutionStrategy_Value_EVALUATOR_STRATEGY", "namespaceoperations__research.html#a49e7da620d9baa1bb2715b89fcbc307cac4a4de196ce46d1cdee9d009791bea4f", null ],
        [ "FirstSolutionStrategy_Value_SAVINGS", "namespaceoperations__research.html#a49e7da620d9baa1bb2715b89fcbc307ca295a33958d67dda6a73918221b21f8e2", null ],
        [ "FirstSolutionStrategy_Value_SWEEP", "namespaceoperations__research.html#a49e7da620d9baa1bb2715b89fcbc307ca6b03b344919c0e27bd4533bb89c527ef", null ],
        [ "FirstSolutionStrategy_Value_CHRISTOFIDES", "namespaceoperations__research.html#a49e7da620d9baa1bb2715b89fcbc307cab8a163273dd98f8e998a4993316fa001", null ],
        [ "FirstSolutionStrategy_Value_ALL_UNPERFORMED", "namespaceoperations__research.html#a49e7da620d9baa1bb2715b89fcbc307cad8b402bcdd3bcd857fc78954202f8235", null ],
        [ "FirstSolutionStrategy_Value_BEST_INSERTION", "namespaceoperations__research.html#a49e7da620d9baa1bb2715b89fcbc307cad556fa2b71d1a07f6427b93c1ba8c94e", null ],
        [ "FirstSolutionStrategy_Value_PARALLEL_CHEAPEST_INSERTION", "namespaceoperations__research.html#a49e7da620d9baa1bb2715b89fcbc307ca94dcd03319ecc0b002b02726490d8831", null ],
        [ "FirstSolutionStrategy_Value_SEQUENTIAL_CHEAPEST_INSERTION", "namespaceoperations__research.html#a49e7da620d9baa1bb2715b89fcbc307caddf6fded79a96eb2c733b21c9cc741c5", null ],
        [ "FirstSolutionStrategy_Value_LOCAL_CHEAPEST_INSERTION", "namespaceoperations__research.html#a49e7da620d9baa1bb2715b89fcbc307ca7ba5c420813d86371a5752207c61be84", null ],
        [ "FirstSolutionStrategy_Value_GLOBAL_CHEAPEST_ARC", "namespaceoperations__research.html#a49e7da620d9baa1bb2715b89fcbc307caaf319737c87c096479faa3655a9d7a24", null ],
        [ "FirstSolutionStrategy_Value_LOCAL_CHEAPEST_ARC", "namespaceoperations__research.html#a49e7da620d9baa1bb2715b89fcbc307ca357b642d8a66f042b6127e8efe1e77a9", null ],
        [ "FirstSolutionStrategy_Value_FIRST_UNBOUND_MIN_VALUE", "namespaceoperations__research.html#a49e7da620d9baa1bb2715b89fcbc307ca127c3496a5dcd277f057806b45c3e76b", null ],
        [ "FirstSolutionStrategy_Value_FirstSolutionStrategy_Value_INT_MIN_SENTINEL_DO_NOT_USE_", "namespaceoperations__research.html#a49e7da620d9baa1bb2715b89fcbc307ca3dbe5e483fd65acbd1f51ae4f5c6491a", null ],
        [ "FirstSolutionStrategy_Value_FirstSolutionStrategy_Value_INT_MAX_SENTINEL_DO_NOT_USE_", "namespaceoperations__research.html#a49e7da620d9baa1bb2715b89fcbc307cac0850adbff55b9fcb7356a72008906a2", null ]
      ] ],
      [ "LocalSearchMetaheuristic_Value", "namespaceoperations__research.html#aee2d8e1dc18095fd66f5a19750e23540", [
        [ "LocalSearchMetaheuristic_Value_UNSET", "namespaceoperations__research.html#aee2d8e1dc18095fd66f5a19750e23540a85240f13d8d1f1ed1386fca1887d7246", null ],
        [ "LocalSearchMetaheuristic_Value_AUTOMATIC", "namespaceoperations__research.html#aee2d8e1dc18095fd66f5a19750e23540ae691eeff628e553468aa8aed9d9a71f1", null ],
        [ "LocalSearchMetaheuristic_Value_GREEDY_DESCENT", "namespaceoperations__research.html#aee2d8e1dc18095fd66f5a19750e23540a4b7545ede1c6e4baab8a133c446282fd", null ],
        [ "LocalSearchMetaheuristic_Value_GUIDED_LOCAL_SEARCH", "namespaceoperations__research.html#aee2d8e1dc18095fd66f5a19750e23540af1c5715467e7c3a31ece0c281150ceb7", null ],
        [ "LocalSearchMetaheuristic_Value_SIMULATED_ANNEALING", "namespaceoperations__research.html#aee2d8e1dc18095fd66f5a19750e23540a4c4b8a20a3738ce3a5995f458c6a88ec", null ],
        [ "LocalSearchMetaheuristic_Value_TABU_SEARCH", "namespaceoperations__research.html#aee2d8e1dc18095fd66f5a19750e23540a32c14398bf7dd09099bd3919f72bfb35", null ],
        [ "LocalSearchMetaheuristic_Value_GENERIC_TABU_SEARCH", "namespaceoperations__research.html#aee2d8e1dc18095fd66f5a19750e23540a4975ff28a1127ba0430e1adb606fe2d7", null ],
        [ "LocalSearchMetaheuristic_Value_LocalSearchMetaheuristic_Value_INT_MIN_SENTINEL_DO_NOT_USE_", "namespaceoperations__research.html#aee2d8e1dc18095fd66f5a19750e23540aa0ddab6ad51b99cb543a60851dcf1ae2", null ],
        [ "LocalSearchMetaheuristic_Value_LocalSearchMetaheuristic_Value_INT_MAX_SENTINEL_DO_NOT_USE_", "namespaceoperations__research.html#aee2d8e1dc18095fd66f5a19750e23540abf08b412e90ec07b8afda5b72683e4cb", null ]
      ] ],
      [ "RoutingSearchParameters_SchedulingSolver", "namespaceoperations__research.html#a761463065b9e80673178ba0dda3634ba", [
        [ "RoutingSearchParameters_SchedulingSolver_UNSET", "namespaceoperations__research.html#a761463065b9e80673178ba0dda3634baa1e18203beb29faa90c1a509c1e6c7e71", null ],
        [ "RoutingSearchParameters_SchedulingSolver_GLOP", "namespaceoperations__research.html#a761463065b9e80673178ba0dda3634baabdac8ec2c26881691d73f3cf6ac5203f", null ],
        [ "RoutingSearchParameters_SchedulingSolver_CP_SAT", "namespaceoperations__research.html#a761463065b9e80673178ba0dda3634baa8913aaf3e19f0956882f928e2b7c5ca3", null ],
        [ "RoutingSearchParameters_SchedulingSolver_RoutingSearchParameters_SchedulingSolver_INT_MIN_SENTINEL_DO_NOT_USE_", "namespaceoperations__research.html#a761463065b9e80673178ba0dda3634baa4abf1d2bce3986a56f73c3d211934318", null ],
        [ "RoutingSearchParameters_SchedulingSolver_RoutingSearchParameters_SchedulingSolver_INT_MAX_SENTINEL_DO_NOT_USE_", "namespaceoperations__research.html#a761463065b9e80673178ba0dda3634baae7070559246287c5da11ef6544f810e7", null ]
      ] ],
      [ "VarTypes", "namespaceoperations__research.html#a403e52e933033645c3388146d5e2edd2", [
        [ "UNSPECIFIED", "namespaceoperations__research.html#a403e52e933033645c3388146d5e2edd2aa876f4fb4e5f7f0c5c48fcf66c9ce7ce", null ],
        [ "DOMAIN_INT_VAR", "namespaceoperations__research.html#a403e52e933033645c3388146d5e2edd2afd9ce19c75c8a2e8ff4c7307eff08e38", null ],
        [ "BOOLEAN_VAR", "namespaceoperations__research.html#a403e52e933033645c3388146d5e2edd2a00e6c449ab034942ac313f8b48643f4b", null ],
        [ "CONST_VAR", "namespaceoperations__research.html#a403e52e933033645c3388146d5e2edd2ac84956f1086e3f828921e0b3d51d806b", null ],
        [ "VAR_ADD_CST", "namespaceoperations__research.html#a403e52e933033645c3388146d5e2edd2a16071208281c29136c1be022b7d170f0", null ],
        [ "VAR_TIMES_CST", "namespaceoperations__research.html#a403e52e933033645c3388146d5e2edd2a0ae20d0967db3441a2b885e5074c4b36", null ],
        [ "CST_SUB_VAR", "namespaceoperations__research.html#a403e52e933033645c3388146d5e2edd2a89a5a9b8c00be595eb52b4d464613d30", null ],
        [ "OPP_VAR", "namespaceoperations__research.html#a403e52e933033645c3388146d5e2edd2ae8e4c6f3e5a6d22d24204ec432f57860", null ],
        [ "TRACE_VAR", "namespaceoperations__research.html#a403e52e933033645c3388146d5e2edd2af2d15b703802d6a1f8f402f90de90dc6", null ]
      ] ],
      [ "AppendTasksFromIntervals", "namespaceoperations__research.html#aec575fd72a48b07ceca957691d785d57", null ],
      [ "AppendTasksFromPath", "namespaceoperations__research.html#aa24417adbd90d64820e9b7e274652934", null ],
      [ "AreAllBooleans", "namespaceoperations__research.html#a7abde7313cef64d25202a18f07481fc3", null ],
      [ "AreAllBound", "namespaceoperations__research.html#ae4c7a8bfc6877606e512d3279549f44d", null ],
      [ "AreAllBoundOrNull", "namespaceoperations__research.html#a54470bffc3ea32cc37d0222e5dbb62a6", null ],
      [ "AreAllBoundTo", "namespaceoperations__research.html#a78ff06a9b302c6c96d8d917da235b749", null ],
      [ "AreAllGreaterOrEqual", "namespaceoperations__research.html#a3aea406979285a28c91fd1ee8115af74", null ],
      [ "AreAllLessOrEqual", "namespaceoperations__research.html#a15f08cfbb35e2b8b1eb76f79caea924a", null ],
      [ "AreAllNegative", "namespaceoperations__research.html#a38972723946490ea4df4e34298d8805d", null ],
      [ "AreAllNull", "namespaceoperations__research.html#ab0ae787392a8dd8a499eb55ac0916aa4", null ],
      [ "AreAllOnes", "namespaceoperations__research.html#ae3e4f71c4c79e0b4ec00c4e715a7c298", null ],
      [ "AreAllPositive", "namespaceoperations__research.html#ab62b402f767cda48eb67ef8b50397f8f", null ],
      [ "AreAllStrictlyNegative", "namespaceoperations__research.html#a8351829c324863ddda52e201df4f9f84", null ],
      [ "AreAllStrictlyPositive", "namespaceoperations__research.html#a3de09f9134b976e5ba64751ac0f4440b", null ],
      [ "BuildModelParametersFromFlags", "namespaceoperations__research.html#ae39a6c4d8ba890ec5150ea91a7aad643", null ],
      [ "BuildSearchParametersFromFlags", "namespaceoperations__research.html#a95da1d3a46432afd40024f79279a48b2", null ],
      [ "ConstraintSolverParameters_TrailCompression_descriptor", "namespaceoperations__research.html#a993b987019482ab8dfaa8286dc18f128", null ],
      [ "ConstraintSolverParameters_TrailCompression_IsValid", "namespaceoperations__research.html#addcabce78790b75a1d064b7e903d8f19", null ],
      [ "ConstraintSolverParameters_TrailCompression_Name", "namespaceoperations__research.html#a931fe91697541bfc1361e8f036236c7b", null ],
      [ "ConstraintSolverParameters_TrailCompression_Parse", "namespaceoperations__research.html#a37bdc44de577a8a28a6dcd9ce4ed12cc", null ],
      [ "CpRandomSeed", "namespaceoperations__research.html#a6daa2481a6bbd7b307647006a8752630", null ],
      [ "DefaultRoutingModelParameters", "namespaceoperations__research.html#aa388c8707db255ae7742c04046bdd613", null ],
      [ "DefaultRoutingSearchParameters", "namespaceoperations__research.html#adcac4a11f1e4d36ceb47f7251461487d", null ],
      [ "DEFINE_INT_TYPE", "namespaceoperations__research.html#afa9196adb7aa76d8e60cd4c0c6687c0d", null ],
      [ "DEFINE_INT_TYPE", "namespaceoperations__research.html#a3d98b6fb94b9cdabfaca3d9f3c9632e9", null ],
      [ "DEFINE_INT_TYPE", "namespaceoperations__research.html#a1edd1d7c020633019991b13d14b4b15b", null ],
      [ "DEFINE_INT_TYPE", "namespaceoperations__research.html#a8fee47a5359613bc7f8df356595c7ff0", null ],
      [ "DEFINE_INT_TYPE", "namespaceoperations__research.html#aff19b78b3d56ff95c23727ca4ff64ea7", null ],
      [ "FillPathEvaluation", "namespaceoperations__research.html#ab0da8bffc5e8eafc798d8b3b1750f05b", null ],
      [ "FillTravelBoundsOfVehicle", "namespaceoperations__research.html#a3923814cbd98268f7c8b9eb9ad16bc17", null ],
      [ "FillValues", "namespaceoperations__research.html#a79edaa5bfddfcd382d36a2ae25df798c", null ],
      [ "FindErrorInRoutingSearchParameters", "namespaceoperations__research.html#ae2e060e8ee4ea901dc4df260b3385eac", null ],
      [ "FirstSolutionStrategy_Value_descriptor", "namespaceoperations__research.html#a9e36d131390995cbab313f9a914d4808", null ],
      [ "FirstSolutionStrategy_Value_IsValid", "namespaceoperations__research.html#ac8fb428ce4826abddd79ff391cfc1c51", null ],
      [ "FirstSolutionStrategy_Value_Name", "namespaceoperations__research.html#a2078876aa16aca05fd12188c1c6e5594", null ],
      [ "FirstSolutionStrategy_Value_Parse", "namespaceoperations__research.html#a2158093ba2bab11258244b113e45735d", null ],
      [ "Hash1", "namespaceoperations__research.html#a597f70b9007402fadc265ccb27687966", null ],
      [ "Hash1", "namespaceoperations__research.html#a14927dac339bd5be7348433e5ae46551", null ],
      [ "Hash1", "namespaceoperations__research.html#a53a6358ea0e13e600820df98156f132d", null ],
      [ "Hash1", "namespaceoperations__research.html#a8e95e16a711ae93395f3735e07708708", null ],
      [ "Hash1", "namespaceoperations__research.html#a5c150546a98dce59439f838f68493d84", null ],
      [ "Hash1", "namespaceoperations__research.html#aee1401375b23909949cce272a3b787db", null ],
      [ "Hash1", "namespaceoperations__research.html#a24d85d1e77f31f346dba6bdc02067473", null ],
      [ "IsArrayBoolean", "namespaceoperations__research.html#a3f4525e71a6b05d97c868f0832750a60", null ],
      [ "IsArrayConstant", "namespaceoperations__research.html#a12527c82ffc8b31c5d8dc836c366d624", null ],
      [ "IsArrayInRange", "namespaceoperations__research.html#adf2aea6c68fe502389c9264b971b2f85", null ],
      [ "IsIncreasing", "namespaceoperations__research.html#a3d434774c07815a25ffaa7adb343c19e", null ],
      [ "IsIncreasingContiguous", "namespaceoperations__research.html#aafac7375c23337f25821aa6f86ca627c", null ],
      [ "LocalSearchMetaheuristic_Value_descriptor", "namespaceoperations__research.html#a0916e97436c8bb1beabad668aa870b9b", null ],
      [ "LocalSearchMetaheuristic_Value_IsValid", "namespaceoperations__research.html#aed46fa1bbfbc75c04bd66f5055ddb3c1", null ],
      [ "LocalSearchMetaheuristic_Value_Name", "namespaceoperations__research.html#a4fed2fb51f43a3bbafdddfae4537e77a", null ],
      [ "LocalSearchMetaheuristic_Value_Parse", "namespaceoperations__research.html#a52e55543815a167041edac3693ff9bd8", null ],
      [ "MakeConstraintDemon0", "namespaceoperations__research.html#aa213d8f884283e0d72712243cbbefa7c", null ],
      [ "MakeConstraintDemon1", "namespaceoperations__research.html#ae0190f4a9c848c207d0bff97f625fcd1", null ],
      [ "MakeConstraintDemon2", "namespaceoperations__research.html#a68441e43b6c0228145d1101db5f3c4de", null ],
      [ "MakeConstraintDemon3", "namespaceoperations__research.html#a362b5a75841c543eec770b731d6e6865", null ],
      [ "MakeDelayedConstraintDemon0", "namespaceoperations__research.html#a6a001b36b291a4afe7dffdbb9194bc45", null ],
      [ "MakeDelayedConstraintDemon1", "namespaceoperations__research.html#ac316c82f31293db18e25c809592908dd", null ],
      [ "MakeDelayedConstraintDemon2", "namespaceoperations__research.html#a6c0bc84812eed9d626b00bc8fb5b9ae1", null ],
      [ "MakeLocalSearchOperator", "namespaceoperations__research.html#a5a2c92a28ee59ab17dfb5885ab3e20c8", null ],
      [ "MakePathStateFilter", "namespaceoperations__research.html#a6cdc2ad7aac33203a04919652bd0a916", null ],
      [ "MakeSetValuesFromTargets", "namespaceoperations__research.html#afbea4d6dff74754bcb65e6d432d71ebe", null ],
      [ "MakeUnaryDimensionFilter", "namespaceoperations__research.html#acc7429aa3368c33f6444304ad8669259", null ],
      [ "MakeVehicleBreaksFilter", "namespaceoperations__research.html#a4ea29fb0eedce9f806f0e0f5fbaff870", null ],
      [ "MaxVarArray", "namespaceoperations__research.html#a587a6a73cbcb4e4a4c7d3b596fa407aa", null ],
      [ "MinVarArray", "namespaceoperations__research.html#a8e8f645f06f9749b562b6625cd822daa", null ],
      [ "One", "namespaceoperations__research.html#a9e48359348ad94d97e6c44ffd52b33e3", null ],
      [ "operator<<", "namespaceoperations__research.html#a82d722796fae06c7fb9d1d8a37c91c99", null ],
      [ "operator<<", "namespaceoperations__research.html#ab563b868509e5ca6c0db57a038d863e4", null ],
      [ "operator<<", "namespaceoperations__research.html#a87fdc0126f6fc98ffb86ba1aa618f322", null ],
      [ "ParameterDebugString", "namespaceoperations__research.html#a3c2f93547af434566184b7dee7039c93", null ],
      [ "ParameterDebugString", "namespaceoperations__research.html#a0953b50b08320d1109c678555137f1db", null ],
      [ "PosIntDivDown", "namespaceoperations__research.html#ade1945fe75ec08245775fc4df20153d6", null ],
      [ "PosIntDivUp", "namespaceoperations__research.html#afb0903025d265c67199f5f09cee57ed0", null ],
      [ "RoutingSearchParameters_SchedulingSolver_descriptor", "namespaceoperations__research.html#ae4842963dc0dee893fa1d95ba2c070c0", null ],
      [ "RoutingSearchParameters_SchedulingSolver_IsValid", "namespaceoperations__research.html#a49b5d4bf85df896e3d80e9ecd63417b4", null ],
      [ "RoutingSearchParameters_SchedulingSolver_Name", "namespaceoperations__research.html#a0246264dc3fbdcc558bca5b954231a4a", null ],
      [ "RoutingSearchParameters_SchedulingSolver_Parse", "namespaceoperations__research.html#aa0e0c69331d6f79d82ad980d9d573f65", null ],
      [ "SetAssignmentFromAssignment", "namespaceoperations__research.html#aea2bf322fab4e2319a23ad22acf8ccf8", null ],
      [ "SolveModelWithSat", "namespaceoperations__research.html#aa17cca151690da44e948d7fbe07abca5", null ],
      [ "ToInt64Vector", "namespaceoperations__research.html#aa9ef36940950625d5a6745caf0000a78", null ],
      [ "Zero", "namespaceoperations__research.html#a5a9881f8a07b166ef2cbde572cea27b6", null ],
      [ "_AssignmentProto_default_instance_", "namespaceoperations__research.html#a71b4716e350a5a5e04973547d1f49b13", null ],
      [ "_ConstraintRuns_default_instance_", "namespaceoperations__research.html#aa9ae85dc85fabfea38f3d5bda107bd4c", null ],
      [ "_ConstraintSolverParameters_default_instance_", "namespaceoperations__research.html#a151f73b26c6eb6c934785005cac3988a", null ],
      [ "_DemonRuns_default_instance_", "namespaceoperations__research.html#a4b2928e7c087b629b9741ba749aa9b04", null ],
      [ "_FirstSolutionStrategy_default_instance_", "namespaceoperations__research.html#ae787854ee3808fd8e6b07e3a39c9ea2b", null ],
      [ "_IntervalVarAssignment_default_instance_", "namespaceoperations__research.html#a5ad249dc5100b4e80763fbc1492426e0", null ],
      [ "_IntVarAssignment_default_instance_", "namespaceoperations__research.html#ad24b0c8b9d2dab9dd0d96d40d52bb743", null ],
      [ "_LocalSearchMetaheuristic_default_instance_", "namespaceoperations__research.html#afa63323dd847b26e9cb2726c83ae0313", null ],
      [ "_RegularLimitParameters_default_instance_", "namespaceoperations__research.html#a93afbd5ed51fecb51d674cf50bf32160", null ],
      [ "_RoutingModelParameters_default_instance_", "namespaceoperations__research.html#a8eb8e40a85ef151b52e190dccc28683a", null ],
      [ "_RoutingSearchParameters_default_instance_", "namespaceoperations__research.html#a553448702fc9d639a4ac4baef4a97e6c", null ],
      [ "_RoutingSearchParameters_ImprovementSearchLimitParameters_default_instance_", "namespaceoperations__research.html#a2a3c398ee49ab27d5e0f9504b9c35ce9", null ],
      [ "_RoutingSearchParameters_LocalSearchNeighborhoodOperators_default_instance_", "namespaceoperations__research.html#ac624c12549343e55d93677e7d7b9eea9", null ],
      [ "_SequenceVarAssignment_default_instance_", "namespaceoperations__research.html#a9044b5e9e7d8fb33c212df31caa2a96f", null ],
      [ "_WorkerInfo_default_instance_", "namespaceoperations__research.html#af96f2eb9df50f9992bf3529e9e48c1a4", null ],
      [ "ConstraintSolverParameters_TrailCompression_TrailCompression_ARRAYSIZE", "namespaceoperations__research.html#a49ef7e29cdcbfd555f27836e2b93dc0f", null ],
      [ "ConstraintSolverParameters_TrailCompression_TrailCompression_MAX", "namespaceoperations__research.html#ae5a34309858c983ecc3c7b041a92f6ce", null ],
      [ "ConstraintSolverParameters_TrailCompression_TrailCompression_MIN", "namespaceoperations__research.html#a61b96714f5df9485a33fc01aabb6add5", null ],
      [ "FirstSolutionStrategy_Value_Value_ARRAYSIZE", "namespaceoperations__research.html#a288aa8299841c0561fbe3505220f708a", null ],
      [ "FirstSolutionStrategy_Value_Value_MAX", "namespaceoperations__research.html#a5d9cbe1519514004c2dafee35d59bb85", null ],
      [ "FirstSolutionStrategy_Value_Value_MIN", "namespaceoperations__research.html#a5993f13606f510a486975f093213b857", null ],
      [ "LocalSearchMetaheuristic_Value_Value_ARRAYSIZE", "namespaceoperations__research.html#a2d5a774e6e23a5297b5c14bc073daa0b", null ],
      [ "LocalSearchMetaheuristic_Value_Value_MAX", "namespaceoperations__research.html#a2aa95ee300a361d3c1090d956379432c", null ],
      [ "LocalSearchMetaheuristic_Value_Value_MIN", "namespaceoperations__research.html#aad6f0fe5f7bc2ded4a3dff23f60f79a1", null ],
      [ "RoutingSearchParameters_SchedulingSolver_SchedulingSolver_ARRAYSIZE", "namespaceoperations__research.html#ae56303ac211f7d967085f6a3a1d384ed", null ],
      [ "RoutingSearchParameters_SchedulingSolver_SchedulingSolver_MAX", "namespaceoperations__research.html#a91b149de1cba5c6c31bcb2d8c8b71de4", null ],
      [ "RoutingSearchParameters_SchedulingSolver_SchedulingSolver_MIN", "namespaceoperations__research.html#af1e8a9851cb9c298550f6ebdeb9471a3", null ]
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