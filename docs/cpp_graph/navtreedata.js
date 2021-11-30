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
  [ "Graph", "index.html", [
    [ "Namespaces", "namespaces.html", [
      [ "ArcIndex", "namespaceoperations__research.html#aecf320bda6e95d1edaf3a546673e3e6b", null ],
      [ "ArcIndexArray", "namespaceoperations__research.html#ad2ab85b2912dbf12804d3d1ddea9ec15", null ],
      [ "CostArray", "namespaceoperations__research.html#acbdd6fd1484828a3d5e809c551ba8cf7", null ],
      [ "CostValue", "namespaceoperations__research.html#a1d1a935ab48f768867fc7e8607ca97b4", null ],
      [ "FlowQuantity", "namespaceoperations__research.html#a259d58fc853dd928f2148dbcf1ef25cd", null ],
      [ "ForwardStarGraph", "namespaceoperations__research.html#ab49bc230c31b15a51efb44be25b30472", null ],
      [ "ForwardStarStaticGraph", "namespaceoperations__research.html#ab2ba507587a78ec3b72a0d62c024bc7e", null ],
      [ "NodeIndex", "namespaceoperations__research.html#a7ffcae041a5d746371436101400cdb6b", null ],
      [ "NodeIndexArray", "namespaceoperations__research.html#a48bfd7172b9a8af435198c373a8cf5e4", null ],
      [ "PathNodeIndex", "namespaceoperations__research.html#ae8625c5e71962a0f99954d34dab9f92d", null ],
      [ "QuantityArray", "namespaceoperations__research.html#a03fc0981d2d99da114ccd8b3abc0c6e1", null ],
      [ "StarGraph", "namespaceoperations__research.html#af24b13c27331f67db15d6c2a3f3507e3", null ],
      [ "BronKerboschAlgorithmStatus", "namespaceoperations__research.html#abd4e546b0e3afb0208c7a44ee6ab4ea8", [
        [ "COMPLETED", "namespaceoperations__research.html#abd4e546b0e3afb0208c7a44ee6ab4ea8a8f7afecbc8fbc4cd0f50a57d1172482e", null ],
        [ "INTERRUPTED", "namespaceoperations__research.html#abd4e546b0e3afb0208c7a44ee6ab4ea8a658f2cadfdf09b6046246e9314f7cd43", null ]
      ] ],
      [ "CliqueResponse", "namespaceoperations__research.html#ae6df4b4cb7c39ca06812199bbee9119c", [
        [ "CONTINUE", "namespaceoperations__research.html#ae6df4b4cb7c39ca06812199bbee9119ca2f453cfe638e57e27bb0c9512436111e", null ],
        [ "STOP", "namespaceoperations__research.html#ae6df4b4cb7c39ca06812199bbee9119ca615a46af313786fc4e349f34118be111", null ]
      ] ],
      [ "FlowModel_ProblemType", "namespaceoperations__research.html#a4f683c5103a92e63d9df46f2652d476c", [
        [ "FlowModel_ProblemType_LINEAR_SUM_ASSIGNMENT", "namespaceoperations__research.html#a4f683c5103a92e63d9df46f2652d476cadae9b1b24153d3681a075b6531ee0b92", null ],
        [ "FlowModel_ProblemType_MAX_FLOW", "namespaceoperations__research.html#a4f683c5103a92e63d9df46f2652d476ca0ce79649a2d56f0b32763b711a4c6841", null ],
        [ "FlowModel_ProblemType_MIN_COST_FLOW", "namespaceoperations__research.html#a4f683c5103a92e63d9df46f2652d476ca6e4c70501999b698977ecfa8d0d19479", null ]
      ] ],
      [ "AddArcsFromMinimumSpanningTree", "namespaceoperations__research.html#af916b84aff43c128a27c2f02a55ab000", null ],
      [ "AStarShortestPath", "namespaceoperations__research.html#a634910e6bc1101eb940ac9359c1cf675", null ],
      [ "BellmanFordShortestPath", "namespaceoperations__research.html#ac17f48e66614d1a9115e67d9c2d5f737", null ],
      [ "BuildEulerianPath", "namespaceoperations__research.html#a1c554960a5c3ff8d8f9eacf5bf77377a", null ],
      [ "BuildEulerianPathFromNode", "namespaceoperations__research.html#aea46f8caebe966fcd0739c713011693e", null ],
      [ "BuildEulerianTour", "namespaceoperations__research.html#ad2edec3419b74442a91b597979950c5b", null ],
      [ "BuildEulerianTourFromNode", "namespaceoperations__research.html#a5b4f8ac7471140527e6105ccc6e69c59", null ],
      [ "BuildKruskalMinimumSpanningTree", "namespaceoperations__research.html#aea7ea9ecb4c13ebf26b36a576c4fdc5f", null ],
      [ "BuildKruskalMinimumSpanningTreeFromSortedArcs", "namespaceoperations__research.html#aefd088882d7ba8d27157eba391b02792", null ],
      [ "BuildLineGraph", "namespaceoperations__research.html#acb53c505b8fd29ceb3abdcc7dfd809ce", null ],
      [ "BuildPrimMinimumSpanningTree", "namespaceoperations__research.html#aa7f6b276e52d86253d0798bc37f4994e", null ],
      [ "ComputeMinimumWeightMatching", "namespaceoperations__research.html#acc1ef62538cd0faf409f9900fd6e2ae8", null ],
      [ "ComputeMinimumWeightMatchingWithMIP", "namespaceoperations__research.html#a53bf12f941f978cc1b1b985816c1fbdf", null ],
      [ "ComputeOneTree", "namespaceoperations__research.html#a9ef4076dcc63501e6d1ecf4a3c6da31b", null ],
      [ "ComputeOneTreeLowerBound", "namespaceoperations__research.html#ae9af26e7687cb65967941eb175148fe5", null ],
      [ "ComputeOneTreeLowerBoundWithAlgorithm", "namespaceoperations__research.html#a3ed3d609fa06ad508b3d21119f94a560", null ],
      [ "ComputeOneTreeLowerBoundWithParameters", "namespaceoperations__research.html#a516a7ec8626d689aa84729fb6f358f89", null ],
      [ "CoverArcsByCliques", "namespaceoperations__research.html#afe4b5a6c0e4019314f288e3f4307c114", null ],
      [ "DijkstraShortestPath", "namespaceoperations__research.html#aad8df474bac5fb8fe87bedd18faf29a6", null ],
      [ "FindCliques", "namespaceoperations__research.html#a509097448ff5705cf3e64d889362bdec", null ],
      [ "FlowModel_ProblemType_descriptor", "namespaceoperations__research.html#a20b4ffb71fd61bc4140c85a01bd29c20", null ],
      [ "FlowModel_ProblemType_IsValid", "namespaceoperations__research.html#ac206da01730a9479e94e730343a14738", null ],
      [ "FlowModel_ProblemType_Name", "namespaceoperations__research.html#ae6f6a6d5b8ff340509dee4e36a62ef65", null ],
      [ "FlowModel_ProblemType_Parse", "namespaceoperations__research.html#ade67a7afb07d33044226a6fcc55238c9", null ],
      [ "GetNodeMinimizingEdgeCostToSource", "namespaceoperations__research.html#aeae6cf89ac4d73d2e95cffaa0edbd687", null ],
      [ "IsEulerianGraph", "namespaceoperations__research.html#ab1cf773de0cae72d0c44efe5b8f4bb89", null ],
      [ "IsSemiEulerianGraph", "namespaceoperations__research.html#a6b312dd19c90b2af099e6f159869f7ee", null ],
      [ "MakeHamiltonianPathSolver", "namespaceoperations__research.html#ae3fee0d3bb89e4913ad2269f8a1be421", null ],
      [ "NearestNeighbors", "namespaceoperations__research.html#ac5b08aa63fdd1b499b4653688c13af81", null ],
      [ "StableDijkstraShortestPath", "namespaceoperations__research.html#af98f2130078a8bf53c590abd744cabfa", null ],
      [ "_Arc_default_instance_", "namespaceoperations__research.html#aa22235731898cbd9e50abc8dfb2fc5b8", null ],
      [ "_FlowModel_default_instance_", "namespaceoperations__research.html#a660549b573f43440c90e14578f5950bb", null ],
      [ "_Node_default_instance_", "namespaceoperations__research.html#a2b73bfb603f4a8921877c01f4dd774af", null ],
      [ "FlowModel_ProblemType_ProblemType_ARRAYSIZE", "namespaceoperations__research.html#a96cc196af88d5d4114c8f15d66635ad4", null ],
      [ "FlowModel_ProblemType_ProblemType_MAX", "namespaceoperations__research.html#ae5f834d473db3f9dd920b4cfb6f51032", null ],
      [ "FlowModel_ProblemType_ProblemType_MIN", "namespaceoperations__research.html#a8f81990e9a7e53ba4956be213beab4fd", null ],
      [ "Graph", "namespaceutil.html#a2f76166dbe0c4055a1f256235ad00478", null ],
      [ "GraphToStringFormat", "namespaceutil.html#a2d1e9c029dfaa2e8dfd58862836440b9", [
        [ "PRINT_GRAPH_ARCS", "namespaceutil.html#a2d1e9c029dfaa2e8dfd58862836440b9a59afa9bae775818b44690c5d14cdf8d0", null ],
        [ "PRINT_GRAPH_ADJACENCY_LISTS", "namespaceutil.html#a2d1e9c029dfaa2e8dfd58862836440b9ac932364714f74e3ca75990c8126019a1", null ],
        [ "PRINT_GRAPH_ADJACENCY_LISTS_SORTED", "namespaceutil.html#a2d1e9c029dfaa2e8dfd58862836440b9ada36744a3f529ceb03e7c1faa842854d", null ]
      ] ],
      [ "BeginEndRange", "namespaceutil.html#a30a4999be011343be06bd28753bf8ecc", null ],
      [ "BeginEndRange", "namespaceutil.html#a68be0ef9f4566f20fbf5238b24385216", null ],
      [ "ComputeOnePossibleReverseArcMapping", "namespaceutil.html#a00a901881f9035f66a4204da4c0ea3e5", null ],
      [ "CopyGraph", "namespaceutil.html#ae5f98804c317dda817bff628d868c4dd", null ],
      [ "DEFINE_RANGE_BASED_ARC_ITERATION", "namespaceutil.html#a37be0131ae922e30a286797a0bef0c96", null ],
      [ "DEFINE_RANGE_BASED_ARC_ITERATION", "namespaceutil.html#a4d0ae05975a2063f2edbeb749f690fc7", null ],
      [ "DEFINE_RANGE_BASED_ARC_ITERATION", "namespaceutil.html#a863ccdb51afb5ef92fe6c94188a5f7e0", null ],
      [ "DEFINE_RANGE_BASED_ARC_ITERATION", "namespaceutil.html#a3098e161a6aceeca482be78d2d221b3b", null ],
      [ "DEFINE_RANGE_BASED_ARC_ITERATION", "namespaceutil.html#a22b5dcc01043ab8da01ebab71ec3ad31", null ],
      [ "DEFINE_RANGE_BASED_ARC_ITERATION", "namespaceutil.html#a3c022b68f68916770fe09996df2f35a3", null ],
      [ "DEFINE_RANGE_BASED_ARC_ITERATION", "namespaceutil.html#a6ce1a67d16c75b202f56301321a457c6", null ],
      [ "DEFINE_RANGE_BASED_ARC_ITERATION", "namespaceutil.html#ab3308688d13e59e2351bef038ce1fdb0", null ],
      [ "DEFINE_RANGE_BASED_ARC_ITERATION", "namespaceutil.html#a97910ddfce7560b406aa3f4939434eb8", null ],
      [ "DEFINE_RANGE_BASED_ARC_ITERATION", "namespaceutil.html#a2a51d676cd5d9354bfe1f80d09c44f39", null ],
      [ "DEFINE_RANGE_BASED_ARC_ITERATION", "namespaceutil.html#a1728675285eb75f9f18d6ed7c134d0b6", null ],
      [ "DEFINE_RANGE_BASED_ARC_ITERATION", "namespaceutil.html#a2cc2a1037195d237820edc97d35404be", null ],
      [ "DEFINE_RANGE_BASED_ARC_ITERATION", "namespaceutil.html#a1db1a919e67261878ff8abda53e664c7", null ],
      [ "DEFINE_RANGE_BASED_ARC_ITERATION", "namespaceutil.html#af3c40fc068f645d9dcd15c332e44fc25", null ],
      [ "EqualRange", "namespaceutil.html#a203f5498a7e1bc70f8ff62c0cfaaf8b1", null ],
      [ "EqualRange", "namespaceutil.html#a628255019800e5a053d08b1f0c5a05f2", null ],
      [ "GetConnectedComponents", "namespaceutil.html#a366d433bd2afb387ea527c581447dffc", null ],
      [ "GetSubgraphOfNodes", "namespaceutil.html#a99094e047812eb44a7c2b3b82091f560", null ],
      [ "GetWeaklyConnectedComponents", "namespaceutil.html#a8a31111f9a74492f1de9238e463edd2a", null ],
      [ "GraphHasDuplicateArcs", "namespaceutil.html#a7cab01cf2313ef202fdb8d1540430c1c", null ],
      [ "GraphHasSelfArcs", "namespaceutil.html#a9aedc5b3920b0ce89959be43ece7625b", null ],
      [ "GraphIsSymmetric", "namespaceutil.html#aa94a02ae5c14feb50676668f2de166dc", null ],
      [ "GraphIsWeaklyConnected", "namespaceutil.html#ab66702bf387ec38027c1f94444872510", null ],
      [ "GraphToString", "namespaceutil.html#a372b5c94ec5d30f923449516ebc2a963", null ],
      [ "IsSubsetOf0N", "namespaceutil.html#aa9fb4c9a176acaf72053b11727436e9e", null ],
      [ "IsValidPermutation", "namespaceutil.html#ad7986b01cf61a31c09a27b4a97db6a83", null ],
      [ "PathHasCycle", "namespaceutil.html#a3215b610ebe65cde55008dc1367c434e", null ],
      [ "Permute", "namespaceutil.html#a8c227a057c1ce9d46b1185abf77ad91e", null ],
      [ "Permute", "namespaceutil.html#ac497881c4166bc694adc4bee62746118", null ],
      [ "PermuteWithExplicitElementType", "namespaceutil.html#a9470623ca7db3c4a62ce3b326c6b07d8", null ],
      [ "ReadGraphFile", "namespaceutil.html#ac1f20692bcd36ed6d01b970af9eb2972", null ],
      [ "RemapGraph", "namespaceutil.html#aab5724a929530fa1d28749dc82852388", null ],
      [ "RemoveCyclesFromPath", "namespaceutil.html#a77ac83968fcb358183853127d83d595a", null ],
      [ "RemoveSelfArcsAndDuplicateArcs", "namespaceutil.html#a95c44a2c444a459f0866bd5607537314", null ],
      [ "Reverse", "namespaceutil.html#a208121f27c615b309e2ab37bb85280f1", null ],
      [ "WriteGraphToFile", "namespaceutil.html#a45fca0c4762006176ad5622ec40dea9c", null ]
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