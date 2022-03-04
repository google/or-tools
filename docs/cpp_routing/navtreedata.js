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
      [ "DimensionSchedulingStatus", "namespaceoperations__research.html#aa0787bf78fb09d1e30f2451b5a68d4b8", [
        [ "OPTIMAL", "namespaceoperations__research.html#aa0787bf78fb09d1e30f2451b5a68d4b8af00c8dbdd6e1f11bdae06be94277d293", null ],
        [ "RELAXED_OPTIMAL_ONLY", "namespaceoperations__research.html#aa0787bf78fb09d1e30f2451b5a68d4b8af8cfb2115ef7ab822bca8edd1edac285", null ],
        [ "INFEASIBLE", "namespaceoperations__research.html#aa0787bf78fb09d1e30f2451b5a68d4b8a6faaca695f728b47f47dd389f31e4a93", null ]
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
      [ "CpRandomSeed", "namespaceoperations__research.html#a6daa2481a6bbd7b307647006a8752630", null ],
      [ "DefaultRoutingModelParameters", "namespaceoperations__research.html#aa388c8707db255ae7742c04046bdd613", null ],
      [ "DefaultRoutingSearchParameters", "namespaceoperations__research.html#adcac4a11f1e4d36ceb47f7251461487d", null ],
      [ "DEFINE_STRONG_INT_TYPE", "namespaceoperations__research.html#ac31922bec0fce604355f05e442c6841e", null ],
      [ "DEFINE_STRONG_INT_TYPE", "namespaceoperations__research.html#a81f46879035192e05c66ea2057a932a2", null ],
      [ "DEFINE_STRONG_INT_TYPE", "namespaceoperations__research.html#a09339ebcf4d5e8a848ebcf71a7c552ce", null ],
      [ "DEFINE_STRONG_INT_TYPE", "namespaceoperations__research.html#a5ac15931bc80d274989abd5021c1a405", null ],
      [ "DEFINE_STRONG_INT_TYPE", "namespaceoperations__research.html#a85589635492c818a6f29133d763ad679", null ],
      [ "FillPathEvaluation", "namespaceoperations__research.html#ab0da8bffc5e8eafc798d8b3b1750f05b", null ],
      [ "FillTravelBoundsOfVehicle", "namespaceoperations__research.html#a3923814cbd98268f7c8b9eb9ad16bc17", null ],
      [ "FillValues", "namespaceoperations__research.html#a79edaa5bfddfcd382d36a2ae25df798c", null ],
      [ "FindErrorInRoutingSearchParameters", "namespaceoperations__research.html#ae2e060e8ee4ea901dc4df260b3385eac", null ],
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
      [ "MakeConstraintDemon0", "namespaceoperations__research.html#aa213d8f884283e0d72712243cbbefa7c", null ],
      [ "MakeConstraintDemon1", "namespaceoperations__research.html#ae0190f4a9c848c207d0bff97f625fcd1", null ],
      [ "MakeConstraintDemon2", "namespaceoperations__research.html#a68441e43b6c0228145d1101db5f3c4de", null ],
      [ "MakeConstraintDemon3", "namespaceoperations__research.html#a362b5a75841c543eec770b731d6e6865", null ],
      [ "MakeDelayedConstraintDemon0", "namespaceoperations__research.html#a6a001b36b291a4afe7dffdbb9194bc45", null ],
      [ "MakeDelayedConstraintDemon1", "namespaceoperations__research.html#ac316c82f31293db18e25c809592908dd", null ],
      [ "MakeDelayedConstraintDemon2", "namespaceoperations__research.html#a6c0bc84812eed9d626b00bc8fb5b9ae1", null ],
      [ "MakeLocalSearchOperator", "namespaceoperations__research.html#a5a2c92a28ee59ab17dfb5885ab3e20c8", null ],
      [ "MakePathStateFilter", "namespaceoperations__research.html#a6cdc2ad7aac33203a04919652bd0a916", null ],
      [ "MakeRestoreDimensionValuesForUnchangedRoutes", "namespaceoperations__research.html#a5ccb557dd21c73b6bcd5470476201cb8", null ],
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
      [ "SetAssignmentFromAssignment", "namespaceoperations__research.html#aea2bf322fab4e2319a23ad22acf8ccf8", null ],
      [ "SolveModelWithSat", "namespaceoperations__research.html#aa17cca151690da44e948d7fbe07abca5", null ],
      [ "ToInt64Vector", "namespaceoperations__research.html#aa9ef36940950625d5a6745caf0000a78", null ],
      [ "Zero", "namespaceoperations__research.html#a5a9881f8a07b166ef2cbde572cea27b6", null ]
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