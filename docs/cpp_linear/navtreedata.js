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
  [ "Linear solver", "index.html", [
    [ "Namespaces", "namespaces.html", [
      [ "MPModelRequest_SolverType", "namespaceoperations__research.html#ac417714eb4dbaf83717bb2aa9affc689", [
        [ "MPModelRequest_SolverType_GLOP_LINEAR_PROGRAMMING", "namespaceoperations__research.html#ac417714eb4dbaf83717bb2aa9affc689a162575d5bea8a8393ff4d9fc11275ec3", null ],
        [ "MPModelRequest_SolverType_CLP_LINEAR_PROGRAMMING", "namespaceoperations__research.html#ac417714eb4dbaf83717bb2aa9affc689a4d77685d54eb87c232beed1e460c5aaa", null ],
        [ "MPModelRequest_SolverType_GLPK_LINEAR_PROGRAMMING", "namespaceoperations__research.html#ac417714eb4dbaf83717bb2aa9affc689a7a5586fa6b3f31587894d20b33ebd8bf", null ],
        [ "MPModelRequest_SolverType_GUROBI_LINEAR_PROGRAMMING", "namespaceoperations__research.html#ac417714eb4dbaf83717bb2aa9affc689a1ccff29cebf50c35a55f15b83fbbae32", null ],
        [ "MPModelRequest_SolverType_XPRESS_LINEAR_PROGRAMMING", "namespaceoperations__research.html#ac417714eb4dbaf83717bb2aa9affc689a25de47e453fa0175e7d254c61e75c847", null ],
        [ "MPModelRequest_SolverType_CPLEX_LINEAR_PROGRAMMING", "namespaceoperations__research.html#ac417714eb4dbaf83717bb2aa9affc689ac40195f69d9c078b3f2249221baa4a0e", null ],
        [ "MPModelRequest_SolverType_SCIP_MIXED_INTEGER_PROGRAMMING", "namespaceoperations__research.html#ac417714eb4dbaf83717bb2aa9affc689a16663d704b6e0b28605e998a6bd36164", null ],
        [ "MPModelRequest_SolverType_GLPK_MIXED_INTEGER_PROGRAMMING", "namespaceoperations__research.html#ac417714eb4dbaf83717bb2aa9affc689a85fa72a05039663be93853d86e3c174c", null ],
        [ "MPModelRequest_SolverType_CBC_MIXED_INTEGER_PROGRAMMING", "namespaceoperations__research.html#ac417714eb4dbaf83717bb2aa9affc689a2ff8af502bfbbc76836bd658144b4f8a", null ],
        [ "MPModelRequest_SolverType_GUROBI_MIXED_INTEGER_PROGRAMMING", "namespaceoperations__research.html#ac417714eb4dbaf83717bb2aa9affc689aad4dc18cf5fd6463aa0b26440f23a8b1", null ],
        [ "MPModelRequest_SolverType_XPRESS_MIXED_INTEGER_PROGRAMMING", "namespaceoperations__research.html#ac417714eb4dbaf83717bb2aa9affc689a5343614c63eb3585cf34d7f48c30d9de", null ],
        [ "MPModelRequest_SolverType_CPLEX_MIXED_INTEGER_PROGRAMMING", "namespaceoperations__research.html#ac417714eb4dbaf83717bb2aa9affc689aeb076e6845a57af474212cd24d9de85c", null ],
        [ "MPModelRequest_SolverType_BOP_INTEGER_PROGRAMMING", "namespaceoperations__research.html#ac417714eb4dbaf83717bb2aa9affc689af523c539a31bee5db12cd7566af59a40", null ],
        [ "MPModelRequest_SolverType_SAT_INTEGER_PROGRAMMING", "namespaceoperations__research.html#ac417714eb4dbaf83717bb2aa9affc689a5985a25f8da9d50c769a78025b9fb0bf", null ],
        [ "MPModelRequest_SolverType_KNAPSACK_MIXED_INTEGER_PROGRAMMING", "namespaceoperations__research.html#ac417714eb4dbaf83717bb2aa9affc689afdb40bacb05f8e852322924fb3597433", null ]
      ] ],
      [ "MPSolverCommonParameters_LPAlgorithmValues", "namespaceoperations__research.html#a8913360b55a9b9861237e0ad039f6979", [
        [ "MPSolverCommonParameters_LPAlgorithmValues_LP_ALGO_UNSPECIFIED", "namespaceoperations__research.html#a8913360b55a9b9861237e0ad039f6979a18a46e7e7a130a3a38c7915f577301c2", null ],
        [ "MPSolverCommonParameters_LPAlgorithmValues_LP_ALGO_DUAL", "namespaceoperations__research.html#a8913360b55a9b9861237e0ad039f6979a533fac70679c30c889a2f75a7e46170e", null ],
        [ "MPSolverCommonParameters_LPAlgorithmValues_LP_ALGO_PRIMAL", "namespaceoperations__research.html#a8913360b55a9b9861237e0ad039f6979af3259b56473cfb82c63b503b80efd283", null ],
        [ "MPSolverCommonParameters_LPAlgorithmValues_LP_ALGO_BARRIER", "namespaceoperations__research.html#a8913360b55a9b9861237e0ad039f6979a3615540cdf96dce3f3ca1c2c05c6d434", null ]
      ] ],
      [ "MPSolverResponseStatus", "namespaceoperations__research.html#aeaeaf340789f2dd271dcf9204279cb1b", [
        [ "MPSOLVER_OPTIMAL", "namespaceoperations__research.html#aeaeaf340789f2dd271dcf9204279cb1ba9cff14a44a54cc44f4b91d65e8cd73b1", null ],
        [ "MPSOLVER_FEASIBLE", "namespaceoperations__research.html#aeaeaf340789f2dd271dcf9204279cb1badbeb0b2ee95779317b20e5876609bf04", null ],
        [ "MPSOLVER_INFEASIBLE", "namespaceoperations__research.html#aeaeaf340789f2dd271dcf9204279cb1ba12a89c0e1b72e6c40e8c0ed16afa48a6", null ],
        [ "MPSOLVER_UNBOUNDED", "namespaceoperations__research.html#aeaeaf340789f2dd271dcf9204279cb1ba4b81d5eafe0b99411fc94d676bc286db", null ],
        [ "MPSOLVER_ABNORMAL", "namespaceoperations__research.html#aeaeaf340789f2dd271dcf9204279cb1baf6f49dcf49ad7df71d2e5b5f2c81ff88", null ],
        [ "MPSOLVER_NOT_SOLVED", "namespaceoperations__research.html#aeaeaf340789f2dd271dcf9204279cb1ba3955ab5aa529fab85eb3566271a043e2", null ],
        [ "MPSOLVER_MODEL_IS_VALID", "namespaceoperations__research.html#aeaeaf340789f2dd271dcf9204279cb1ba81239917bc019f71d9f78b550c6acf37", null ],
        [ "MPSOLVER_CANCELLED_BY_USER", "namespaceoperations__research.html#aeaeaf340789f2dd271dcf9204279cb1ba44a70f17e7bb4d99a6635673a0447074", null ],
        [ "MPSOLVER_UNKNOWN_STATUS", "namespaceoperations__research.html#aeaeaf340789f2dd271dcf9204279cb1ba55c6337c519b0ef4070cfe89dead866f", null ],
        [ "MPSOLVER_MODEL_INVALID", "namespaceoperations__research.html#aeaeaf340789f2dd271dcf9204279cb1ba5d004f74784501a516258dff6b7740ec", null ],
        [ "MPSOLVER_MODEL_INVALID_SOLUTION_HINT", "namespaceoperations__research.html#aeaeaf340789f2dd271dcf9204279cb1badcf1ef4c6880afe0aeb3e0c80a9dd4e9", null ],
        [ "MPSOLVER_MODEL_INVALID_SOLVER_PARAMETERS", "namespaceoperations__research.html#aeaeaf340789f2dd271dcf9204279cb1bae98571c24fbf68a473b3d93ca45c6e7a", null ],
        [ "MPSOLVER_SOLVER_TYPE_UNAVAILABLE", "namespaceoperations__research.html#aeaeaf340789f2dd271dcf9204279cb1bacd2f1efd0290a03172495d05d131cbfe", null ],
        [ "MPSOLVER_INCOMPATIBLE_OPTIONS", "namespaceoperations__research.html#aeaeaf340789f2dd271dcf9204279cb1baaf7b72c19d9cf5d0231a5a84f809e1fc", null ]
      ] ],
      [ "MPSosConstraint_Type", "namespaceoperations__research.html#a7f0aabaee920119f0b683ba887250f0b", [
        [ "MPSosConstraint_Type_SOS1_DEFAULT", "namespaceoperations__research.html#a7f0aabaee920119f0b683ba887250f0bae59773cfdb0c5a52b6dafc8b9c853ae6", null ],
        [ "MPSosConstraint_Type_SOS2", "namespaceoperations__research.html#a7f0aabaee920119f0b683ba887250f0ba29baea5082ad9cfbd015d2e0f04a80f1", null ]
      ] ],
      [ "AbslParseFlag", "namespaceoperations__research.html#a61dc18a85425d0a7cf6aa3e7ce3199f6", null ],
      [ "AbslUnparseFlag", "namespaceoperations__research.html#af04d1dfc591c35038a974202e50e541f", null ],
      [ "ApplyVerifiedMPModelDelta", "namespaceoperations__research.html#a3ed9bad79131000a00e7f01a5f5b824c", null ],
      [ "ExportModelAsLpFormat", "namespaceoperations__research.html#a2e14c987281f826f0737c14f5abc00b7", null ],
      [ "ExportModelAsLpFormatReturnString", "namespaceoperations__research.html#a4d319c19b685fe608fe013b573081351", null ],
      [ "ExportModelAsMpsFormat", "namespaceoperations__research.html#a2c2e3f703497d288ee03330752d3a4c3", null ],
      [ "ExportModelAsMpsFormatReturnString", "namespaceoperations__research.html#a37abd61c0d982af79257814b6d3a733e", null ],
      [ "ExtractValidMPModelInPlaceOrPopulateResponseStatus", "namespaceoperations__research.html#a518848a6b3e172d127121637ab5c608d", null ],
      [ "ExtractValidMPModelOrPopulateResponseStatus", "namespaceoperations__research.html#a97d22724a2d11191dcf78eb8e5e5064a", null ],
      [ "FindErrorInMPModelDeltaProto", "namespaceoperations__research.html#a2506c50d4eb5505613003f685fd1af9f", null ],
      [ "FindErrorInMPModelProto", "namespaceoperations__research.html#aa7ee6518d30b6929f4bd0229da743f18", null ],
      [ "FindFeasibilityErrorInSolutionHint", "namespaceoperations__research.html#ae4ee4d82cf625670cdc1f52197454654", null ],
      [ "MergeMPConstraintProtoExceptTerms", "namespaceoperations__research.html#ae6841e1b78c89cf9139fc1b9ba1ae8cb", null ],
      [ "MPModelRequest_SolverType_descriptor", "namespaceoperations__research.html#aefe49f016a54c9e6c20c5dfad53a95dd", null ],
      [ "MPModelRequest_SolverType_IsValid", "namespaceoperations__research.html#ad26c438ab5f1b232d7eced80a2780ca0", null ],
      [ "MPModelRequest_SolverType_Name", "namespaceoperations__research.html#a2f8347efb6886eb3abfaea4b80507669", null ],
      [ "MPModelRequest_SolverType_Parse", "namespaceoperations__research.html#af48be224aa2c72fa71392b3239c098fa", null ],
      [ "MPSolverCommonParameters_LPAlgorithmValues_descriptor", "namespaceoperations__research.html#a7e785b5ed81a85431bee6f0c4531a5a2", null ],
      [ "MPSolverCommonParameters_LPAlgorithmValues_IsValid", "namespaceoperations__research.html#ab3ee5c7a9f799696432b082fd4835232", null ],
      [ "MPSolverCommonParameters_LPAlgorithmValues_Name", "namespaceoperations__research.html#a162d87fe93790d0d0d85c30d09c8422e", null ],
      [ "MPSolverCommonParameters_LPAlgorithmValues_Parse", "namespaceoperations__research.html#aaa501defe046d6885ab0c2ede8d9876e", null ],
      [ "MPSolverResponseStatus_descriptor", "namespaceoperations__research.html#a37819663db97d526b034c9b65c2ce26a", null ],
      [ "MPSolverResponseStatus_IsValid", "namespaceoperations__research.html#a7a295b0760db498bc4fa9479bb8c2329", null ],
      [ "MPSolverResponseStatus_Name", "namespaceoperations__research.html#a43fa3a0e388da216bc95624640cc262b", null ],
      [ "MPSolverResponseStatus_Parse", "namespaceoperations__research.html#a6f0faa69401ab983c6dc8f76dedb1ff8", null ],
      [ "MPSolverResponseStatusIsRpcError", "namespaceoperations__research.html#a52d451963bca16889b3f1e23450a8f2d", null ],
      [ "MPSosConstraint_Type_descriptor", "namespaceoperations__research.html#aead7ed5561d311ddcc4ae74af9616791", null ],
      [ "MPSosConstraint_Type_IsValid", "namespaceoperations__research.html#a69d74b24808a9eba4bcbc04c5bd1f9fb", null ],
      [ "MPSosConstraint_Type_Name", "namespaceoperations__research.html#a7be95500ce8da6b75afcc1cce8205cba", null ],
      [ "MPSosConstraint_Type_Parse", "namespaceoperations__research.html#ade647001e966274bd8a67297a5e06f85", null ],
      [ "operator*", "namespaceoperations__research.html#a62b131d2829f1cdacd2414d2d7bc6c7c", null ],
      [ "operator*", "namespaceoperations__research.html#a138bb0f103cd9d68e4d13fa773901186", null ],
      [ "operator+", "namespaceoperations__research.html#a97f9b83239285f5fdfcac1b8e8b4f162", null ],
      [ "operator-", "namespaceoperations__research.html#a515cdaf4f9c4000bb3482a0c450e23c3", null ],
      [ "operator/", "namespaceoperations__research.html#abebdd7f40e90df8dc7d557b6e26da942", null ],
      [ "operator<<", "namespaceoperations__research.html#af6e53662a1f604ececc66ebeef3902f3", null ],
      [ "operator<<", "namespaceoperations__research.html#a45a908ea6b50a2a7d3f6bd59de6db37c", null ],
      [ "operator<<", "namespaceoperations__research.html#a6dcd119b77400c438a8a316093d553f2", null ],
      [ "operator<=", "namespaceoperations__research.html#a6d1fa20f9c9faf7027c0b16f97139e80", null ],
      [ "operator==", "namespaceoperations__research.html#a08146f196bd9c3f492ee108732449ced", null ],
      [ "operator>=", "namespaceoperations__research.html#ac4052f92af6a7fbb1d45e17befcb68e0", null ],
      [ "SolverTypeIsMip", "namespaceoperations__research.html#a653e11eef608bfb88f21325e7fa12f2b", null ],
      [ "SolverTypeIsMip", "namespaceoperations__research.html#a417ee4c2129def5589f952ac70233b2e", null ],
      [ "ToString", "namespaceoperations__research.html#afc3e3b80841b587c6fbfd9e9f3ec9c59", null ],
      [ "_MPAbsConstraint_default_instance_", "namespaceoperations__research.html#ac5df5baf7bb0dd4aaf62baf45102b52d", null ],
      [ "_MPArrayConstraint_default_instance_", "namespaceoperations__research.html#ab0af6821d48a0f0600db1bdcbdba06d1", null ],
      [ "_MPArrayWithConstantConstraint_default_instance_", "namespaceoperations__research.html#a6e56c3af4a10aa1d08c107531153cdba", null ],
      [ "_MPConstraintProto_default_instance_", "namespaceoperations__research.html#a8d8b4a23e426846c3012be178e3c4be9", null ],
      [ "_MPGeneralConstraintProto_default_instance_", "namespaceoperations__research.html#a719bb6f5f7403ef8262232534210c96a", null ],
      [ "_MPIndicatorConstraint_default_instance_", "namespaceoperations__research.html#a7404bbeba91240266e929f71c26c9aef", null ],
      [ "_MPModelDeltaProto_ConstraintOverridesEntry_DoNotUse_default_instance_", "namespaceoperations__research.html#a19b9d68f11c7edb64190b307c96e4ba0", null ],
      [ "_MPModelDeltaProto_default_instance_", "namespaceoperations__research.html#ab084f583d906c5f9773a7c006669d0f1", null ],
      [ "_MPModelDeltaProto_VariableOverridesEntry_DoNotUse_default_instance_", "namespaceoperations__research.html#a018812fc95d782b328cd0c1dd13571cc", null ],
      [ "_MPModelProto_default_instance_", "namespaceoperations__research.html#a894d390b15ce81288eb8a7323c435ca4", null ],
      [ "_MPModelRequest_default_instance_", "namespaceoperations__research.html#a21ff6531f9433bcf5ac4f7654223d648", null ],
      [ "_MPQuadraticConstraint_default_instance_", "namespaceoperations__research.html#a7b4f130e3877c2149b14fbd6c296b63d", null ],
      [ "_MPQuadraticObjective_default_instance_", "namespaceoperations__research.html#aa5a6d69a58750a88c914eabc85cd4508", null ],
      [ "_MPSolution_default_instance_", "namespaceoperations__research.html#aba986f292963cc67918090468b1518d1", null ],
      [ "_MPSolutionResponse_default_instance_", "namespaceoperations__research.html#a6bb17e14243dc58537d7b29994ed5ccc", null ],
      [ "_MPSolveInfo_default_instance_", "namespaceoperations__research.html#a1deb641e0d4ed6b9a3ba79661e4461c9", null ],
      [ "_MPSolverCommonParameters_default_instance_", "namespaceoperations__research.html#ae2e01270d59862503d3ea97b9cc3e427", null ],
      [ "_MPSosConstraint_default_instance_", "namespaceoperations__research.html#ada7e9956ab85bd985af2f5e00246a197", null ],
      [ "_MPVariableProto_default_instance_", "namespaceoperations__research.html#a3649b60e4b709187336cee990cff6b63", null ],
      [ "_OptionalDouble_default_instance_", "namespaceoperations__research.html#ad063d17e904384f676f9d5951f5a6657", null ],
      [ "_PartialVariableAssignment_default_instance_", "namespaceoperations__research.html#ae5c743ef0c2dfd8d383ec4a665b31af1", null ],
      [ "kDefaultPrimalTolerance", "namespaceoperations__research.html#a221d711fbd5a16db9dc92a3c5095cbf5", null ],
      [ "MPModelRequest_SolverType_SolverType_ARRAYSIZE", "namespaceoperations__research.html#a2de998be000467c8282dffaa7cd5765e", null ],
      [ "MPModelRequest_SolverType_SolverType_MAX", "namespaceoperations__research.html#a7df20597435fbcb555e2f95e3ddb8bbc", null ],
      [ "MPModelRequest_SolverType_SolverType_MIN", "namespaceoperations__research.html#aa002f435b31936c88de1e4e6cba07385", null ],
      [ "MPSolverCommonParameters_LPAlgorithmValues_LPAlgorithmValues_ARRAYSIZE", "namespaceoperations__research.html#aeed81f9f9071b4a4177b6ef927e64abb", null ],
      [ "MPSolverCommonParameters_LPAlgorithmValues_LPAlgorithmValues_MAX", "namespaceoperations__research.html#a5e7277e793e483f8a46437f2994cd99e", null ],
      [ "MPSolverCommonParameters_LPAlgorithmValues_LPAlgorithmValues_MIN", "namespaceoperations__research.html#a0666b791aab277878d1353c2d9e653b9", null ],
      [ "MPSolverResponseStatus_ARRAYSIZE", "namespaceoperations__research.html#a37524b8ef9f0b60de566a8f2570ccfea", null ],
      [ "MPSolverResponseStatus_MAX", "namespaceoperations__research.html#a593d0ebcda514b4ecb1b57e7c96583fd", null ],
      [ "MPSolverResponseStatus_MIN", "namespaceoperations__research.html#a3161b62004f8339805b0ebc64ab5247f", null ],
      [ "MPSosConstraint_Type_Type_ARRAYSIZE", "namespaceoperations__research.html#a0d2a226e2846854fd5b6cc4979207fad", null ],
      [ "MPSosConstraint_Type_Type_MAX", "namespaceoperations__research.html#aae7222bc6e10499aa4c49aa93b6cb1f0", null ],
      [ "MPSosConstraint_Type_Type_MIN", "namespaceoperations__research.html#ab736c31cc61aee9390b859a14cf68703", null ]
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