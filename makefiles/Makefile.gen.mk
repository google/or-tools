BASE_DEPS = \
    $(SRC_DIR)/base/adjustable_priority_queue.h \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/casts.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/status.h \
    $(SRC_DIR)/base/stringpiece.h \
    $(SRC_DIR)/base/thorough_hash.h \
    $(SRC_DIR)/base/time_support.h

BASE_LIB_OBJS = \
    $(OBJ_DIR)/base/bitmap.$O \
    $(OBJ_DIR)/base/callback.$O \
    $(OBJ_DIR)/base/file.$O \
    $(OBJ_DIR)/base/filelinereader.$O \
    $(OBJ_DIR)/base/join.$O \
    $(OBJ_DIR)/base/logging.$O \
    $(OBJ_DIR)/base/mutex.$O \
    $(OBJ_DIR)/base/numbers.$O \
    $(OBJ_DIR)/base/random.$O \
    $(OBJ_DIR)/base/recordio.$O \
    $(OBJ_DIR)/base/split.$O \
    $(OBJ_DIR)/base/stringpiece.$O \
    $(OBJ_DIR)/base/stringprintf.$O \
    $(OBJ_DIR)/base/sysinfo.$O \
    $(OBJ_DIR)/base/threadpool.$O \
    $(OBJ_DIR)/base/timer.$O \
    $(OBJ_DIR)/base/time_support.$O

$(SRC_DIR)/base/adjustable_priority_queue.h: \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h

$(SRC_DIR)/base/adjustable_priority_queue-inl.h: \
    $(SRC_DIR)/base/adjustable_priority_queue.h

$(SRC_DIR)/base/basictypes.h: \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h

$(SRC_DIR)/base/bitmap.h: \
    $(SRC_DIR)/base/basictypes.h

$(SRC_DIR)/base/callback.h: \
    $(SRC_DIR)/base/logging.h

$(SRC_DIR)/base/file.h: \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/status.h

$(SRC_DIR)/base/filelinereader.h: \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/integral_types.h

$(SRC_DIR)/base/hash.h: \
    $(SRC_DIR)/base/basictypes.h

$(SRC_DIR)/base/int_type.h: \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/macros.h

$(SRC_DIR)/base/int_type_indexed_vector.h: \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/macros.h

$(SRC_DIR)/base/jniutil.h: \
    $(SRC_DIR)/base/logging.h

$(SRC_DIR)/base/join.h: \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/stringpiece.h

$(SRC_DIR)/base/logging.h: \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/macros.h

$(SRC_DIR)/base/map_util.h: \
    $(SRC_DIR)/base/logging.h

$(SRC_DIR)/base/mathutil.h: \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/casts.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h

$(SRC_DIR)/base/murmur.h: \
    $(SRC_DIR)/base/thorough_hash.h

$(SRC_DIR)/base/mutex.h: \
    $(SRC_DIR)/base/macros.h

$(SRC_DIR)/base/numbers.h: \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/join.h

$(SRC_DIR)/base/random.h: \
    $(SRC_DIR)/base/basictypes.h

$(SRC_DIR)/base/recordio.h: \
    $(SRC_DIR)/base/file.h

$(SRC_DIR)/base/sparsetable.h: \
    $(SRC_DIR)/base/logging.h

$(SRC_DIR)/base/split.h: \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringpiece.h

$(SRC_DIR)/base/status.h: \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h

$(SRC_DIR)/base/statusor.h: \
    $(SRC_DIR)/base/status.h

$(SRC_DIR)/base/stringpiece_utils.h: \
    $(SRC_DIR)/base/stringpiece.h

$(SRC_DIR)/base/stringprintf.h: \
    $(SRC_DIR)/base/stringpiece.h

$(SRC_DIR)/base/strongly_connected_components.h: \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h

$(SRC_DIR)/base/strtoint.h: \
    $(SRC_DIR)/base/basictypes.h

$(SRC_DIR)/base/strutil.h: \
    $(SRC_DIR)/base/stringpiece.h

$(SRC_DIR)/base/synchronization.h: \
    $(SRC_DIR)/base/logging.h

$(SRC_DIR)/base/sysinfo.h: \
    $(SRC_DIR)/base/basictypes.h

$(SRC_DIR)/base/thorough_hash.h: \
    $(SRC_DIR)/base/integral_types.h

$(SRC_DIR)/base/timer.h: \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/time_support.h

$(SRC_DIR)/base/time_support.h: \
    $(SRC_DIR)/base/integral_types.h

$(OBJ_DIR)/base/bitmap.$O: \
    $(SRC_DIR)/base/bitmap.cc \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/bitmap.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/bitmap.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Sbitmap.$O

$(OBJ_DIR)/base/callback.$O: \
    $(SRC_DIR)/base/callback.cc \
    $(SRC_DIR)/base/callback.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/callback.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Scallback.$O

$(OBJ_DIR)/base/file.$O: \
    $(SRC_DIR)/base/file.cc \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/file.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Sfile.$O

$(OBJ_DIR)/base/filelinereader.$O: \
    $(SRC_DIR)/base/filelinereader.cc \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/filelinereader.h \
    $(SRC_DIR)/base/logging.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/filelinereader.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Sfilelinereader.$O

$(OBJ_DIR)/base/join.$O: \
    $(SRC_DIR)/base/join.cc \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/stringpiece.h \
    $(SRC_DIR)/base/stringprintf.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/join.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Sjoin.$O

$(OBJ_DIR)/base/logging.$O: \
    $(SRC_DIR)/base/logging.cc \
    $(SRC_DIR)/base/logging.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/logging.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Slogging.$O

$(OBJ_DIR)/base/mutex.$O: \
    $(SRC_DIR)/base/mutex.cc \
    $(SRC_DIR)/base/mutex.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/mutex.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Smutex.$O

$(OBJ_DIR)/base/numbers.$O: \
    $(SRC_DIR)/base/numbers.cc \
    $(SRC_DIR)/base/numbers.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/numbers.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Snumbers.$O

$(OBJ_DIR)/base/random.$O: \
    $(SRC_DIR)/base/random.cc \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/random.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/random.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Srandom.$O

$(OBJ_DIR)/base/recordio.$O: \
    $(SRC_DIR)/base/recordio.cc \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/recordio.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/recordio.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Srecordio.$O

$(OBJ_DIR)/base/split.$O: \
    $(SRC_DIR)/base/split.cc \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/split.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/split.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Ssplit.$O

$(OBJ_DIR)/base/stringpiece.$O: \
    $(SRC_DIR)/base/stringpiece.cc \
    $(SRC_DIR)/base/stringpiece.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/stringpiece.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Sstringpiece.$O

$(OBJ_DIR)/base/stringprintf.$O: \
    $(SRC_DIR)/base/stringprintf.cc \
    $(SRC_DIR)/base/stringprintf.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/stringprintf.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Sstringprintf.$O

$(OBJ_DIR)/base/sysinfo.$O: \
    $(SRC_DIR)/base/sysinfo.cc \
    $(SRC_DIR)/base/stringpiece.h \
    $(SRC_DIR)/base/sysinfo.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/sysinfo.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Ssysinfo.$O

$(OBJ_DIR)/base/threadpool.$O: \
    $(SRC_DIR)/base/threadpool.cc \
    $(SRC_DIR)/base/threadpool.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/threadpool.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Sthreadpool.$O

$(OBJ_DIR)/base/timer.$O: \
    $(SRC_DIR)/base/timer.cc \
    $(SRC_DIR)/base/timer.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/timer.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Stimer.$O

$(OBJ_DIR)/base/time_support.$O: \
    $(SRC_DIR)/base/time_support.cc \
    $(SRC_DIR)/base/time_support.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/time_support.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Stime_support.$O

UTIL_DEPS = \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/running_stat.h \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/base/adjustable_priority_queue.h \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/casts.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/status.h \
    $(SRC_DIR)/base/stringpiece.h \
    $(SRC_DIR)/base/thorough_hash.h \
    $(SRC_DIR)/base/time_support.h

UTIL_LIB_OBJS = \
    $(OBJ_DIR)/util/bitset.$O \
    $(OBJ_DIR)/util/cached_log.$O \
    $(OBJ_DIR)/util/fp_utils.$O \
    $(OBJ_DIR)/util/graph_export.$O \
    $(OBJ_DIR)/util/piecewise_linear_function.$O \
    $(OBJ_DIR)/util/proto_tools.$O \
    $(OBJ_DIR)/util/range_query_function.$O \
    $(OBJ_DIR)/util/rational_approximation.$O \
    $(OBJ_DIR)/util/rcpsp_parser.$O \
    $(OBJ_DIR)/util/sorted_interval_list.$O \
    $(OBJ_DIR)/util/stats.$O \
    $(OBJ_DIR)/util/time_limit.$O \
    $(OBJ_DIR)/util/xml_helper.$O

$(SRC_DIR)/util/bitset.h: \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h

$(SRC_DIR)/util/cached_log.h: \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h

$(SRC_DIR)/util/filelineiter.h: \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringpiece_utils.h \
    $(SRC_DIR)/base/strutil.h

$(SRC_DIR)/util/fp_utils.h: \
    $(SRC_DIR)/base/logging.h

$(SRC_DIR)/util/functions_swig_helpers.h: \
    $(SRC_DIR)/base/integral_types.h

$(SRC_DIR)/util/functions_swig_test_helpers.h: \
    $(SRC_DIR)/base/integral_types.h

$(SRC_DIR)/util/graph_export.h: \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h

$(SRC_DIR)/util/monoid_operation_tree.h: \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/stringprintf.h

$(SRC_DIR)/util/permutation.h: \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h

$(SRC_DIR)/util/piecewise_linear_function.h: \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/macros.h

$(SRC_DIR)/util/range_minimum_query.h: \
    $(SRC_DIR)/util/bitset.h

$(SRC_DIR)/util/range_query_function.h: \
    $(SRC_DIR)/base/integral_types.h

$(SRC_DIR)/util/rational_approximation.h: \
    $(SRC_DIR)/base/integral_types.h

$(SRC_DIR)/util/rcpsp_parser.h: \
    $(SRC_DIR)/base/integral_types.h

$(SRC_DIR)/util/rev.h: \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/map_util.h

$(SRC_DIR)/util/running_stat.h: \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h

$(SRC_DIR)/util/saturated_arithmetic.h: \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/base/casts.h \
    $(SRC_DIR)/base/integral_types.h

$(SRC_DIR)/util/sorted_interval_list.h: \
    $(SRC_DIR)/base/integral_types.h

$(SRC_DIR)/util/stats.h: \
    $(SRC_DIR)/base/timer.h

$(SRC_DIR)/util/time_limit.h: \
    $(SRC_DIR)/util/running_stat.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/port.h \
    $(SRC_DIR)/base/timer.h \
    $(SRC_DIR)/base/time_support.h

$(SRC_DIR)/util/tuple_set.h: \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/map_util.h

$(SRC_DIR)/util/vector_map.h: \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/map_util.h

$(SRC_DIR)/util/vector_or_function.h: \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h

$(SRC_DIR)/util/xml_helper.h: \
    $(SRC_DIR)/base/macros.h

$(SRC_DIR)/util/zvector.h: \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h

$(OBJ_DIR)/util/bitset.$O: \
    $(SRC_DIR)/util/bitset.cc \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/logging.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/bitset.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Sbitset.$O

$(OBJ_DIR)/util/cached_log.$O: \
    $(SRC_DIR)/util/cached_log.cc \
    $(SRC_DIR)/util/cached_log.h \
    $(SRC_DIR)/base/logging.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/cached_log.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Scached_log.$O

$(OBJ_DIR)/util/fp_utils.$O: \
    $(SRC_DIR)/util/fp_utils.cc \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/fp_utils.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/fp_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Sfp_utils.$O

$(OBJ_DIR)/util/graph_export.$O: \
    $(SRC_DIR)/util/graph_export.cc \
    $(SRC_DIR)/util/graph_export.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/status.h \
    $(SRC_DIR)/base/stringprintf.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/graph_export.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Sgraph_export.$O

$(OBJ_DIR)/util/piecewise_linear_function.$O: \
    $(SRC_DIR)/util/piecewise_linear_function.cc \
    $(SRC_DIR)/util/piecewise_linear_function.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringprintf.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/piecewise_linear_function.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Spiecewise_linear_function.$O

$(OBJ_DIR)/util/proto_tools.$O: \
    $(SRC_DIR)/util/proto_tools.cc \
    $(SRC_DIR)/util/proto_tools.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/proto_tools.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Sproto_tools.$O

$(OBJ_DIR)/util/range_query_function.$O: \
    $(SRC_DIR)/util/range_query_function.cc \
    $(SRC_DIR)/util/range_minimum_query.h \
    $(SRC_DIR)/util/range_query_function.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/range_query_function.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Srange_query_function.$O

$(OBJ_DIR)/util/rational_approximation.$O: \
    $(SRC_DIR)/util/rational_approximation.cc \
    $(SRC_DIR)/util/rational_approximation.h \
    $(SRC_DIR)/base/logging.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/rational_approximation.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Srational_approximation.$O

$(OBJ_DIR)/util/rcpsp_parser.$O: \
    $(SRC_DIR)/util/rcpsp_parser.cc \
    $(SRC_DIR)/util/filelineiter.h \
    $(SRC_DIR)/util/rcpsp_parser.h \
    $(SRC_DIR)/base/numbers.h \
    $(SRC_DIR)/base/split.h \
    $(SRC_DIR)/base/stringpiece_utils.h \
    $(SRC_DIR)/base/strtoint.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/rcpsp_parser.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Srcpsp_parser.$O

$(OBJ_DIR)/util/sorted_interval_list.$O: \
    $(SRC_DIR)/util/sorted_interval_list.cc \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/util/sorted_interval_list.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringprintf.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/sorted_interval_list.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Ssorted_interval_list.$O

$(OBJ_DIR)/util/stats.$O: \
    $(SRC_DIR)/util/stats.cc \
    $(SRC_DIR)/util/stats.h \
    $(SRC_DIR)/base/encodingutils.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/base/sysinfo.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/stats.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Sstats.$O

$(OBJ_DIR)/util/time_limit.$O: \
    $(SRC_DIR)/util/time_limit.cc \
    $(SRC_DIR)/util/time_limit.h \
    $(SRC_DIR)/base/join.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/time_limit.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Stime_limit.$O

$(OBJ_DIR)/util/xml_helper.$O: \
    $(SRC_DIR)/util/xml_helper.cc \
    $(SRC_DIR)/util/xml_helper.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/base/strutil.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/xml_helper.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Sxml_helper.$O

LP_DATA_DEPS = \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/lp_data/matrix_scaler.h \
    $(SRC_DIR)/lp_data/permutation.h \
    $(SRC_DIR)/lp_data/sparse_column.h \
    $(SRC_DIR)/lp_data/sparse.h \
    $(SRC_DIR)/lp_data/sparse_vector.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/running_stat.h \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/base/adjustable_priority_queue.h \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/casts.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/status.h \
    $(SRC_DIR)/base/stringpiece.h \
    $(SRC_DIR)/base/thorough_hash.h \
    $(SRC_DIR)/base/time_support.h \
    $(SRC_DIR)/algorithms/dynamic_partition.h \
    $(SRC_DIR)/algorithms/dynamic_permutation.h \
    $(SRC_DIR)/linear_solver/linear_solver.h \
    $(GEN_DIR)/linear_solver/linear_solver.pb.h

LP_DATA_LIB_OBJS = \
    $(OBJ_DIR)/lp_data/lp_data.$O \
    $(OBJ_DIR)/lp_data/lp_decomposer.$O \
    $(OBJ_DIR)/lp_data/lp_print_utils.$O \
    $(OBJ_DIR)/lp_data/lp_types.$O \
    $(OBJ_DIR)/lp_data/lp_utils.$O \
    $(OBJ_DIR)/lp_data/matrix_scaler.$O \
    $(OBJ_DIR)/lp_data/matrix_utils.$O \
    $(OBJ_DIR)/lp_data/mps_reader.$O \
    $(OBJ_DIR)/lp_data/proto_utils.$O \
    $(OBJ_DIR)/lp_data/sparse.$O \
    $(OBJ_DIR)/lp_data/sparse_column.$O

$(SRC_DIR)/lp_data/lp_data.h: \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/lp_data/matrix_scaler.h \
    $(SRC_DIR)/lp_data/sparse.h \
    $(SRC_DIR)/util/fp_utils.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/int_type_indexed_vector.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h

$(SRC_DIR)/lp_data/lp_decomposer.h: \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/base/mutex.h

$(SRC_DIR)/lp_data/lp_print_utils.h: \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/stringprintf.h

$(SRC_DIR)/lp_data/lp_types.h: \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/int_type_indexed_vector.h

$(SRC_DIR)/lp_data/lp_utils.h: \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/lp_data/sparse_column.h \
    $(SRC_DIR)/base/accurate_sum.h

$(SRC_DIR)/lp_data/matrix_scaler.h: \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type_indexed_vector.h \
    $(SRC_DIR)/base/macros.h

$(SRC_DIR)/lp_data/matrix_utils.h: \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/lp_data/sparse.h

$(SRC_DIR)/lp_data/mps_reader.h: \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/int_type_indexed_vector.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/stringprintf.h

$(SRC_DIR)/lp_data/permutation.h: \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/util/return_macros.h

$(SRC_DIR)/lp_data/proto_utils.h: \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(GEN_DIR)/linear_solver/linear_solver.pb.h

$(SRC_DIR)/lp_data/sparse_column.h: \
    $(SRC_DIR)/lp_data/sparse_vector.h

$(SRC_DIR)/lp_data/sparse.h: \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/lp_data/permutation.h \
    $(SRC_DIR)/lp_data/sparse_column.h \
    $(SRC_DIR)/util/return_macros.h \
    $(SRC_DIR)/base/integral_types.h

$(SRC_DIR)/lp_data/sparse_vector.h: \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/lp_data/permutation.h \
    $(SRC_DIR)/util/iterators.h \
    $(SRC_DIR)/util/return_macros.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringprintf.h

$(OBJ_DIR)/lp_data/lp_data.$O: \
    $(SRC_DIR)/lp_data/lp_data.cc \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(SRC_DIR)/lp_data/lp_print_utils.h \
    $(SRC_DIR)/lp_data/lp_utils.h \
    $(SRC_DIR)/lp_data/matrix_utils.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringprintf.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/lp_data/lp_data.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Slp_data.$O

$(OBJ_DIR)/lp_data/lp_decomposer.$O: \
    $(SRC_DIR)/lp_data/lp_decomposer.cc \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(SRC_DIR)/lp_data/lp_decomposer.h \
    $(SRC_DIR)/lp_data/lp_utils.h \
    $(SRC_DIR)/algorithms/dynamic_partition.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/lp_data/lp_decomposer.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Slp_decomposer.$O

$(OBJ_DIR)/lp_data/lp_print_utils.$O: \
    $(SRC_DIR)/lp_data/lp_print_utils.cc \
    $(SRC_DIR)/lp_data/lp_print_utils.h \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/util/rational_approximation.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/lp_data/lp_print_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Slp_print_utils.$O

$(OBJ_DIR)/lp_data/lp_types.$O: \
    $(SRC_DIR)/lp_data/lp_types.cc \
    $(SRC_DIR)/lp_data/lp_types.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/lp_data/lp_types.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Slp_types.$O

$(OBJ_DIR)/lp_data/lp_utils.$O: \
    $(SRC_DIR)/lp_data/lp_utils.cc \
    $(SRC_DIR)/lp_data/lp_utils.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/lp_data/lp_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Slp_utils.$O

$(OBJ_DIR)/lp_data/matrix_scaler.$O: \
    $(SRC_DIR)/lp_data/matrix_scaler.cc \
    $(SRC_DIR)/lp_data/lp_utils.h \
    $(SRC_DIR)/lp_data/matrix_scaler.h \
    $(SRC_DIR)/lp_data/sparse.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringprintf.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/lp_data/matrix_scaler.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Smatrix_scaler.$O

$(OBJ_DIR)/lp_data/matrix_utils.$O: \
    $(SRC_DIR)/lp_data/matrix_utils.cc \
    $(SRC_DIR)/lp_data/matrix_utils.h \
    $(SRC_DIR)/base/hash.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/lp_data/matrix_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Smatrix_utils.$O

$(OBJ_DIR)/lp_data/mps_reader.$O: \
    $(SRC_DIR)/lp_data/mps_reader.cc \
    $(SRC_DIR)/lp_data/lp_print_utils.h \
    $(SRC_DIR)/lp_data/mps_reader.h \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/filelinereader.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/numbers.h \
    $(SRC_DIR)/base/split.h \
    $(SRC_DIR)/base/status.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/base/strutil.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/lp_data/mps_reader.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Smps_reader.$O

$(OBJ_DIR)/lp_data/proto_utils.$O: \
    $(SRC_DIR)/lp_data/proto_utils.cc \
    $(SRC_DIR)/lp_data/proto_utils.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/lp_data/proto_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Sproto_utils.$O

$(OBJ_DIR)/lp_data/sparse.$O: \
    $(SRC_DIR)/lp_data/sparse.cc \
    $(SRC_DIR)/lp_data/sparse.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/stringprintf.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/lp_data/sparse.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Ssparse.$O

$(OBJ_DIR)/lp_data/sparse_column.$O: \
    $(SRC_DIR)/lp_data/sparse_column.cc \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/lp_data/sparse_column.h \
    $(SRC_DIR)/base/stringprintf.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/lp_data/sparse_column.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Ssparse_column.$O

GLOP_DEPS = \
    $(SRC_DIR)/glop/basis_representation.h \
    $(SRC_DIR)/glop/dual_edge_norms.h \
    $(SRC_DIR)/glop/entering_variable.h \
    $(SRC_DIR)/glop/lu_factorization.h \
    $(SRC_DIR)/glop/markowitz.h \
    $(GEN_DIR)/glop/parameters.pb.h \
    $(SRC_DIR)/glop/preprocessor.h \
    $(SRC_DIR)/glop/primal_edge_norms.h \
    $(SRC_DIR)/glop/rank_one_update.h \
    $(SRC_DIR)/glop/reduced_costs.h \
    $(SRC_DIR)/glop/revised_simplex.h \
    $(SRC_DIR)/glop/status.h \
    $(SRC_DIR)/glop/update_row.h \
    $(SRC_DIR)/glop/variables_info.h \
    $(SRC_DIR)/glop/variable_values.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/running_stat.h \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/base/adjustable_priority_queue.h \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/casts.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/status.h \
    $(SRC_DIR)/base/stringpiece.h \
    $(SRC_DIR)/base/thorough_hash.h \
    $(SRC_DIR)/base/time_support.h \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/lp_data/matrix_scaler.h \
    $(SRC_DIR)/lp_data/permutation.h \
    $(SRC_DIR)/lp_data/sparse_column.h \
    $(SRC_DIR)/lp_data/sparse.h \
    $(SRC_DIR)/lp_data/sparse_vector.h \
    $(SRC_DIR)/linear_solver/linear_solver.h \
    $(GEN_DIR)/linear_solver/linear_solver.pb.h

GLOP_LIB_OBJS = \
    $(OBJ_DIR)/glop/basis_representation.$O \
    $(OBJ_DIR)/glop/dual_edge_norms.$O \
    $(OBJ_DIR)/glop/entering_variable.$O \
    $(OBJ_DIR)/glop/initial_basis.$O \
    $(OBJ_DIR)/glop/lp_solver.$O \
    $(OBJ_DIR)/glop/lu_factorization.$O \
    $(OBJ_DIR)/glop/markowitz.$O \
    $(OBJ_DIR)/glop/preprocessor.$O \
    $(OBJ_DIR)/glop/primal_edge_norms.$O \
    $(OBJ_DIR)/glop/reduced_costs.$O \
    $(OBJ_DIR)/glop/revised_simplex.$O \
    $(OBJ_DIR)/glop/status.$O \
    $(OBJ_DIR)/glop/update_row.$O \
    $(OBJ_DIR)/glop/variables_info.$O \
    $(OBJ_DIR)/glop/variable_values.$O \
    $(OBJ_DIR)/glop/parameters.pb.$O

$(SRC_DIR)/glop/basis_representation.h: \
    $(SRC_DIR)/glop/lu_factorization.h \
    $(GEN_DIR)/glop/parameters.pb.h \
    $(SRC_DIR)/glop/rank_one_update.h \
    $(SRC_DIR)/glop/status.h \
    $(SRC_DIR)/util/stats.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/lp_data/sparse.h

$(SRC_DIR)/glop/dual_edge_norms.h: \
    $(SRC_DIR)/glop/basis_representation.h \
    $(GEN_DIR)/glop/parameters.pb.h \
    $(SRC_DIR)/util/stats.h \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(SRC_DIR)/lp_data/lp_types.h

$(SRC_DIR)/glop/entering_variable.h: \
    $(SRC_DIR)/glop/basis_representation.h \
    $(GEN_DIR)/glop/parameters.pb.h \
    $(SRC_DIR)/glop/primal_edge_norms.h \
    $(SRC_DIR)/glop/reduced_costs.h \
    $(SRC_DIR)/glop/status.h \
    $(SRC_DIR)/glop/update_row.h \
    $(SRC_DIR)/glop/variables_info.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/stats.h \
    $(SRC_DIR)/base/random.h \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(SRC_DIR)/lp_data/lp_types.h

$(SRC_DIR)/glop/initial_basis.h: \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/lp_data/sparse.h

$(SRC_DIR)/glop/lp_solver.h: \
    $(GEN_DIR)/glop/parameters.pb.h \
    $(SRC_DIR)/glop/preprocessor.h \
    $(SRC_DIR)/util/time_limit.h \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(SRC_DIR)/lp_data/lp_types.h

$(SRC_DIR)/glop/lu_factorization.h: \
    $(SRC_DIR)/glop/markowitz.h \
    $(GEN_DIR)/glop/parameters.pb.h \
    $(SRC_DIR)/glop/status.h \
    $(SRC_DIR)/util/stats.h \
    $(SRC_DIR)/lp_data/sparse.h

$(SRC_DIR)/glop/markowitz.h: \
    $(GEN_DIR)/glop/parameters.pb.h \
    $(SRC_DIR)/glop/status.h \
    $(SRC_DIR)/util/stats.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/lp_data/sparse.h

$(SRC_DIR)/glop/preprocessor.h: \
    $(GEN_DIR)/glop/parameters.pb.h \
    $(SRC_DIR)/glop/revised_simplex.h \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/lp_data/matrix_scaler.h

$(SRC_DIR)/glop/primal_edge_norms.h: \
    $(SRC_DIR)/glop/basis_representation.h \
    $(GEN_DIR)/glop/parameters.pb.h \
    $(SRC_DIR)/glop/update_row.h \
    $(SRC_DIR)/glop/variables_info.h \
    $(SRC_DIR)/util/stats.h \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(SRC_DIR)/lp_data/lp_types.h

$(SRC_DIR)/glop/rank_one_update.h: \
    $(SRC_DIR)/glop/status.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/lp_data/lp_utils.h \
    $(SRC_DIR)/lp_data/sparse.h

$(SRC_DIR)/glop/reduced_costs.h: \
    $(SRC_DIR)/glop/basis_representation.h \
    $(GEN_DIR)/glop/parameters.pb.h \
    $(SRC_DIR)/glop/primal_edge_norms.h \
    $(SRC_DIR)/glop/status.h \
    $(SRC_DIR)/glop/update_row.h \
    $(SRC_DIR)/glop/variables_info.h \
    $(SRC_DIR)/util/stats.h \
    $(SRC_DIR)/base/random.h \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(SRC_DIR)/lp_data/lp_types.h

$(SRC_DIR)/glop/revised_simplex.h: \
    $(SRC_DIR)/glop/basis_representation.h \
    $(SRC_DIR)/glop/dual_edge_norms.h \
    $(SRC_DIR)/glop/entering_variable.h \
    $(GEN_DIR)/glop/parameters.pb.h \
    $(SRC_DIR)/glop/primal_edge_norms.h \
    $(SRC_DIR)/glop/reduced_costs.h \
    $(SRC_DIR)/glop/status.h \
    $(SRC_DIR)/glop/update_row.h \
    $(SRC_DIR)/glop/variables_info.h \
    $(SRC_DIR)/glop/variable_values.h \
    $(SRC_DIR)/util/time_limit.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/random.h \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(SRC_DIR)/lp_data/lp_print_utils.h \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/lp_data/matrix_scaler.h

$(SRC_DIR)/glop/status.h: \
    $(SRC_DIR)/base/port.h

$(SRC_DIR)/glop/update_row.h: \
    $(SRC_DIR)/glop/basis_representation.h \
    $(GEN_DIR)/glop/parameters.pb.h \
    $(SRC_DIR)/glop/variables_info.h \
    $(SRC_DIR)/util/stats.h \
    $(SRC_DIR)/lp_data/lp_types.h

$(SRC_DIR)/glop/variables_info.h: \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/lp_data/sparse.h

$(SRC_DIR)/glop/variable_values.h: \
    $(SRC_DIR)/glop/basis_representation.h \
    $(SRC_DIR)/glop/variables_info.h \
    $(SRC_DIR)/util/stats.h \
    $(SRC_DIR)/lp_data/lp_types.h

$(OBJ_DIR)/glop/basis_representation.$O: \
    $(SRC_DIR)/glop/basis_representation.cc \
    $(SRC_DIR)/glop/basis_representation.h \
    $(SRC_DIR)/glop/status.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/lp_data/lp_utils.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/glop/basis_representation.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sbasis_representation.$O

$(OBJ_DIR)/glop/dual_edge_norms.$O: \
    $(SRC_DIR)/glop/dual_edge_norms.cc \
    $(SRC_DIR)/glop/dual_edge_norms.h \
    $(SRC_DIR)/lp_data/lp_utils.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/glop/dual_edge_norms.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sdual_edge_norms.$O

$(OBJ_DIR)/glop/entering_variable.$O: \
    $(SRC_DIR)/glop/entering_variable.cc \
    $(SRC_DIR)/glop/entering_variable.h \
    $(SRC_DIR)/base/numbers.h \
    $(SRC_DIR)/base/timer.h \
    $(SRC_DIR)/lp_data/lp_utils.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/glop/entering_variable.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sentering_variable.$O

$(OBJ_DIR)/glop/initial_basis.$O: \
    $(SRC_DIR)/glop/initial_basis.cc \
    $(SRC_DIR)/glop/initial_basis.h \
    $(SRC_DIR)/glop/markowitz.h \
    $(SRC_DIR)/lp_data/lp_utils.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/glop/initial_basis.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sinitial_basis.$O

$(OBJ_DIR)/glop/lp_solver.$O: \
    $(SRC_DIR)/glop/lp_solver.cc \
    $(SRC_DIR)/glop/lp_solver.h \
    $(SRC_DIR)/glop/preprocessor.h \
    $(SRC_DIR)/glop/status.h \
    $(SRC_DIR)/util/fp_utils.h \
    $(SRC_DIR)/util/proto_tools.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/strutil.h \
    $(SRC_DIR)/base/timer.h \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/lp_data/lp_utils.h \
    $(SRC_DIR)/lp_data/proto_utils.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/glop/lp_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Slp_solver.$O

$(OBJ_DIR)/glop/lu_factorization.$O: \
    $(SRC_DIR)/glop/lu_factorization.cc \
    $(SRC_DIR)/glop/lu_factorization.h \
    $(SRC_DIR)/lp_data/lp_utils.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/glop/lu_factorization.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Slu_factorization.$O

$(OBJ_DIR)/glop/markowitz.$O: \
    $(SRC_DIR)/glop/markowitz.cc \
    $(SRC_DIR)/glop/markowitz.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/lp_data/lp_utils.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/glop/markowitz.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Smarkowitz.$O

$(OBJ_DIR)/glop/preprocessor.$O: \
    $(SRC_DIR)/glop/preprocessor.cc \
    $(SRC_DIR)/glop/preprocessor.h \
    $(SRC_DIR)/glop/revised_simplex.h \
    $(SRC_DIR)/glop/status.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/lp_data/lp_utils.h \
    $(SRC_DIR)/lp_data/matrix_utils.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/glop/preprocessor.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Spreprocessor.$O

$(OBJ_DIR)/glop/primal_edge_norms.$O: \
    $(SRC_DIR)/glop/primal_edge_norms.cc \
    $(SRC_DIR)/glop/primal_edge_norms.h \
    $(SRC_DIR)/base/timer.h \
    $(SRC_DIR)/lp_data/lp_utils.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/glop/primal_edge_norms.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sprimal_edge_norms.$O

$(OBJ_DIR)/glop/reduced_costs.$O: \
    $(SRC_DIR)/glop/reduced_costs.cc \
    $(SRC_DIR)/glop/reduced_costs.h \
    $(SRC_DIR)/lp_data/lp_utils.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/glop/reduced_costs.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sreduced_costs.$O

$(OBJ_DIR)/glop/revised_simplex.$O: \
    $(SRC_DIR)/glop/revised_simplex.cc \
    $(SRC_DIR)/glop/initial_basis.h \
    $(GEN_DIR)/glop/parameters.pb.h \
    $(SRC_DIR)/glop/revised_simplex.h \
    $(SRC_DIR)/util/fp_utils.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(SRC_DIR)/lp_data/lp_print_utils.h \
    $(SRC_DIR)/lp_data/lp_utils.h \
    $(SRC_DIR)/lp_data/matrix_utils.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/glop/revised_simplex.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Srevised_simplex.$O

$(OBJ_DIR)/glop/status.$O: \
    $(SRC_DIR)/glop/status.cc \
    $(SRC_DIR)/glop/status.h \
    $(SRC_DIR)/base/logging.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/glop/status.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sstatus.$O

$(OBJ_DIR)/glop/update_row.$O: \
    $(SRC_DIR)/glop/update_row.cc \
    $(SRC_DIR)/glop/update_row.h \
    $(SRC_DIR)/lp_data/lp_utils.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/glop/update_row.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Supdate_row.$O

$(OBJ_DIR)/glop/variables_info.$O: \
    $(SRC_DIR)/glop/variables_info.cc \
    $(SRC_DIR)/glop/variables_info.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/glop/variables_info.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Svariables_info.$O

$(OBJ_DIR)/glop/variable_values.$O: \
    $(SRC_DIR)/glop/variable_values.cc \
    $(SRC_DIR)/glop/variable_values.h \
    $(SRC_DIR)/util/iterators.h \
    $(SRC_DIR)/lp_data/lp_utils.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/glop/variable_values.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Svariable_values.$O

$(GEN_DIR)/glop/parameters.pb.cc: $(SRC_DIR)/glop/parameters.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/glop/parameters.proto

$(GEN_DIR)/glop/parameters.pb.h: $(GEN_DIR)/glop/parameters.pb.cc

$(OBJ_DIR)/glop/parameters.pb.$O: $(GEN_DIR)/glop/parameters.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/glop/parameters.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sparameters.pb.$O

GRAPH_DEPS = \
    $(SRC_DIR)/graph/christofides.h \
    $(SRC_DIR)/graph/connectivity.h \
    $(SRC_DIR)/graph/ebert_graph.h \
    $(SRC_DIR)/graph/eulerian_path.h \
    $(GEN_DIR)/graph/flow_problem.pb.h \
    $(SRC_DIR)/graph/graph.h \
    $(SRC_DIR)/graph/minimum_spanning_tree.h \
    $(SRC_DIR)/base/adjustable_priority_queue.h \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/casts.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/status.h \
    $(SRC_DIR)/base/stringpiece.h \
    $(SRC_DIR)/base/thorough_hash.h \
    $(SRC_DIR)/base/time_support.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/running_stat.h \
    $(SRC_DIR)/util/saturated_arithmetic.h

GRAPH_LIB_OBJS = \
    $(OBJ_DIR)/graph/assignment.$O \
    $(OBJ_DIR)/graph/astar.$O \
    $(OBJ_DIR)/graph/bellman_ford.$O \
    $(OBJ_DIR)/graph/cliques.$O \
    $(OBJ_DIR)/graph/dijkstra.$O \
    $(OBJ_DIR)/graph/linear_assignment.$O \
    $(OBJ_DIR)/graph/max_flow.$O \
    $(OBJ_DIR)/graph/min_cost_flow.$O \
    $(OBJ_DIR)/graph/shortestpaths.$O \
    $(OBJ_DIR)/graph/util.$O \
    $(OBJ_DIR)/graph/flow_problem.pb.$O

$(SRC_DIR)/graph/assignment.h: \
    $(SRC_DIR)/graph/ebert_graph.h

$(SRC_DIR)/graph/christofides.h: \
    $(SRC_DIR)/graph/eulerian_path.h \
    $(SRC_DIR)/graph/minimum_spanning_tree.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h

$(SRC_DIR)/graph/cliques.h: \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/int_type_indexed_vector.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/util/time_limit.h

$(SRC_DIR)/graph/connectivity.h: \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h

$(SRC_DIR)/graph/ebert_graph.h: \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/permutation.h \
    $(SRC_DIR)/util/zvector.h

$(SRC_DIR)/graph/eulerian_path.h: \
    $(SRC_DIR)/base/logging.h

$(SRC_DIR)/graph/graph.h: \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/port.h \
    $(SRC_DIR)/util/iterators.h

$(SRC_DIR)/graph/graphs.h: \
    $(SRC_DIR)/graph/ebert_graph.h \
    $(SRC_DIR)/graph/graph.h

$(SRC_DIR)/graph/hamiltonian_path.h: \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/util/vector_or_function.h

$(SRC_DIR)/graph/io.h: \
    $(SRC_DIR)/graph/graph.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/numbers.h \
    $(SRC_DIR)/base/split.h \
    $(SRC_DIR)/base/status.h \
    $(SRC_DIR)/base/statusor.h \
    $(SRC_DIR)/util/filelineiter.h

$(SRC_DIR)/graph/linear_assignment.h: \
    $(SRC_DIR)/graph/ebert_graph.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/permutation.h \
    $(SRC_DIR)/util/zvector.h

$(SRC_DIR)/graph/max_flow.h: \
    $(SRC_DIR)/graph/ebert_graph.h \
    $(GEN_DIR)/graph/flow_problem.pb.h \
    $(SRC_DIR)/graph/graph.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/util/stats.h \
    $(SRC_DIR)/util/zvector.h

$(SRC_DIR)/graph/min_cost_flow.h: \
    $(SRC_DIR)/graph/ebert_graph.h \
    $(SRC_DIR)/graph/graph.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/util/stats.h \
    $(SRC_DIR)/util/zvector.h

$(SRC_DIR)/graph/minimum_spanning_tree.h: \
    $(SRC_DIR)/graph/connectivity.h \
    $(SRC_DIR)/graph/graph.h \
    $(SRC_DIR)/base/adjustable_priority_queue.h \
    $(SRC_DIR)/base/adjustable_priority_queue-inl.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/util/vector_or_function.h

$(SRC_DIR)/graph/one_tree_lower_bound.h: \
    $(SRC_DIR)/graph/christofides.h \
    $(SRC_DIR)/graph/minimum_spanning_tree.h \
    $(SRC_DIR)/base/integral_types.h

$(SRC_DIR)/graph/shortestpaths.h: \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/macros.h

$(SRC_DIR)/graph/util.h: \
    $(SRC_DIR)/graph/graph.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/map_util.h

$(OBJ_DIR)/graph/assignment.$O: \
    $(SRC_DIR)/graph/assignment.cc \
    $(SRC_DIR)/graph/assignment.h \
    $(SRC_DIR)/graph/ebert_graph.h \
    $(SRC_DIR)/graph/linear_assignment.h \
    $(SRC_DIR)/base/commandlineflags.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/assignment.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Sassignment.$O

$(OBJ_DIR)/graph/astar.$O: \
    $(SRC_DIR)/graph/astar.cc \
    $(SRC_DIR)/base/adjustable_priority_queue.h \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/astar.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Sastar.$O

$(OBJ_DIR)/graph/bellman_ford.$O: \
    $(SRC_DIR)/graph/bellman_ford.cc \
    $(SRC_DIR)/base/integral_types.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/bellman_ford.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Sbellman_ford.$O

$(OBJ_DIR)/graph/cliques.$O: \
    $(SRC_DIR)/graph/cliques.cc \
    $(SRC_DIR)/graph/cliques.h \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/hash.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/cliques.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Scliques.$O

$(OBJ_DIR)/graph/dijkstra.$O: \
    $(SRC_DIR)/graph/dijkstra.cc \
    $(SRC_DIR)/base/adjustable_priority_queue.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/dijkstra.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Sdijkstra.$O

$(OBJ_DIR)/graph/linear_assignment.$O: \
    $(SRC_DIR)/graph/linear_assignment.cc \
    $(SRC_DIR)/graph/linear_assignment.h \
    $(SRC_DIR)/base/commandlineflags.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/linear_assignment.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Slinear_assignment.$O

$(OBJ_DIR)/graph/max_flow.$O: \
    $(SRC_DIR)/graph/max_flow.cc \
    $(SRC_DIR)/graph/graphs.h \
    $(SRC_DIR)/graph/max_flow.h \
    $(SRC_DIR)/base/stringprintf.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/max_flow.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Smax_flow.$O

$(OBJ_DIR)/graph/min_cost_flow.$O: \
    $(SRC_DIR)/graph/min_cost_flow.cc \
    $(SRC_DIR)/graph/graphs.h \
    $(SRC_DIR)/graph/max_flow.h \
    $(SRC_DIR)/graph/min_cost_flow.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/mathutil.h \
    $(SRC_DIR)/base/stringprintf.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/min_cost_flow.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Smin_cost_flow.$O

$(OBJ_DIR)/graph/shortestpaths.$O: \
    $(SRC_DIR)/graph/shortestpaths.cc \
    $(SRC_DIR)/graph/shortestpaths.h \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/casts.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/shortestpaths.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Sshortestpaths.$O

$(OBJ_DIR)/graph/util.$O: \
    $(SRC_DIR)/graph/util.cc \
    $(SRC_DIR)/graph/util.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/util.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Sutil.$O

$(GEN_DIR)/graph/flow_problem.pb.cc: $(SRC_DIR)/graph/flow_problem.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/graph/flow_problem.proto

$(GEN_DIR)/graph/flow_problem.pb.h: $(GEN_DIR)/graph/flow_problem.pb.cc

$(OBJ_DIR)/graph/flow_problem.pb.$O: $(GEN_DIR)/graph/flow_problem.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/graph/flow_problem.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Sflow_problem.pb.$O

ALGORITHMS_DEPS = \
    $(SRC_DIR)/algorithms/dynamic_partition.h \
    $(SRC_DIR)/algorithms/dynamic_permutation.h \
    $(SRC_DIR)/base/adjustable_priority_queue.h \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/casts.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/status.h \
    $(SRC_DIR)/base/stringpiece.h \
    $(SRC_DIR)/base/thorough_hash.h \
    $(SRC_DIR)/base/time_support.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/running_stat.h \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/graph/christofides.h \
    $(SRC_DIR)/graph/connectivity.h \
    $(SRC_DIR)/graph/ebert_graph.h \
    $(SRC_DIR)/graph/eulerian_path.h \
    $(GEN_DIR)/graph/flow_problem.pb.h \
    $(SRC_DIR)/graph/graph.h \
    $(SRC_DIR)/graph/minimum_spanning_tree.h \
    $(SRC_DIR)/linear_solver/linear_solver.h \
    $(GEN_DIR)/linear_solver/linear_solver.pb.h

ALGORITHMS_LIB_OBJS = \
    $(OBJ_DIR)/algorithms/dynamic_partition.$O \
    $(OBJ_DIR)/algorithms/dynamic_permutation.$O \
    $(OBJ_DIR)/algorithms/find_graph_symmetries.$O \
    $(OBJ_DIR)/algorithms/hungarian.$O \
    $(OBJ_DIR)/algorithms/knapsack_solver.$O \
    $(OBJ_DIR)/algorithms/sparse_permutation.$O

$(SRC_DIR)/algorithms/dense_doubly_linked_list.h: \
    $(SRC_DIR)/base/logging.h

$(SRC_DIR)/algorithms/dynamic_partition.h: \
    $(SRC_DIR)/base/logging.h

$(SRC_DIR)/algorithms/dynamic_permutation.h: \
    $(SRC_DIR)/base/logging.h

$(SRC_DIR)/algorithms/find_graph_symmetries.h: \
    $(SRC_DIR)/algorithms/dynamic_partition.h \
    $(SRC_DIR)/algorithms/dynamic_permutation.h \
    $(SRC_DIR)/base/status.h \
    $(SRC_DIR)/util/iterators.h \
    $(SRC_DIR)/util/stats.h \
    $(SRC_DIR)/util/time_limit.h \
    $(SRC_DIR)/graph/graph.h

$(SRC_DIR)/algorithms/hungarian.h: \
    $(SRC_DIR)/base/hash.h

$(SRC_DIR)/algorithms/knapsack_solver.h: \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/util/time_limit.h

$(SRC_DIR)/algorithms/sparse_permutation.h: \
    $(SRC_DIR)/base/logging.h

$(OBJ_DIR)/algorithms/dynamic_partition.$O: \
    $(SRC_DIR)/algorithms/dynamic_partition.cc \
    $(SRC_DIR)/algorithms/dynamic_partition.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/murmur.h \
    $(SRC_DIR)/base/stringprintf.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/algorithms/dynamic_partition.cc $(OBJ_OUT)$(OBJ_DIR)$Salgorithms$Sdynamic_partition.$O

$(OBJ_DIR)/algorithms/dynamic_permutation.$O: \
    $(SRC_DIR)/algorithms/dynamic_permutation.cc \
    $(SRC_DIR)/algorithms/dynamic_permutation.h \
    $(SRC_DIR)/algorithms/sparse_permutation.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/algorithms/dynamic_permutation.cc $(OBJ_OUT)$(OBJ_DIR)$Salgorithms$Sdynamic_permutation.$O

$(OBJ_DIR)/algorithms/find_graph_symmetries.$O: \
    $(SRC_DIR)/algorithms/find_graph_symmetries.cc \
    $(SRC_DIR)/algorithms/dense_doubly_linked_list.h \
    $(SRC_DIR)/algorithms/dynamic_partition.h \
    $(SRC_DIR)/algorithms/dynamic_permutation.h \
    $(SRC_DIR)/algorithms/find_graph_symmetries.h \
    $(SRC_DIR)/algorithms/sparse_permutation.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/iterators.h \
    $(SRC_DIR)/graph/util.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/algorithms/find_graph_symmetries.cc $(OBJ_OUT)$(OBJ_DIR)$Salgorithms$Sfind_graph_symmetries.$O

$(OBJ_DIR)/algorithms/hungarian.$O: \
    $(SRC_DIR)/algorithms/hungarian.cc \
    $(SRC_DIR)/algorithms/hungarian.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/algorithms/hungarian.cc $(OBJ_OUT)$(OBJ_DIR)$Salgorithms$Shungarian.$O

$(OBJ_DIR)/algorithms/knapsack_solver.$O: \
    $(SRC_DIR)/algorithms/knapsack_solver.cc \
    $(SRC_DIR)/algorithms/knapsack_solver.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/time_limit.h \
    $(SRC_DIR)/linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/algorithms/knapsack_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Salgorithms$Sknapsack_solver.$O

$(OBJ_DIR)/algorithms/sparse_permutation.$O: \
    $(SRC_DIR)/algorithms/sparse_permutation.cc \
    $(SRC_DIR)/algorithms/sparse_permutation.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/algorithms/sparse_permutation.cc $(OBJ_OUT)$(OBJ_DIR)$Salgorithms$Ssparse_permutation.$O

SAT_DEPS = \
    $(SRC_DIR)/sat/boolean_problem.h \
    $(GEN_DIR)/sat/boolean_problem.pb.h \
    $(SRC_DIR)/sat/clause.h \
    $(SRC_DIR)/sat/cp_constraints.h \
    $(SRC_DIR)/sat/drat.h \
    $(SRC_DIR)/sat/integer_expr.h \
    $(SRC_DIR)/sat/integer.h \
    $(SRC_DIR)/sat/intervals.h \
    $(SRC_DIR)/sat/model.h \
    $(SRC_DIR)/sat/pb_constraint.h \
    $(SRC_DIR)/sat/precedences.h \
    $(SRC_DIR)/sat/sat_base.h \
    $(GEN_DIR)/sat/sat_parameters.pb.h \
    $(SRC_DIR)/sat/sat_solver.h \
    $(SRC_DIR)/sat/simplification.h \
    $(SRC_DIR)/base/adjustable_priority_queue.h \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/casts.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/status.h \
    $(SRC_DIR)/base/stringpiece.h \
    $(SRC_DIR)/base/thorough_hash.h \
    $(SRC_DIR)/base/time_support.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/running_stat.h \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/algorithms/dynamic_partition.h \
    $(SRC_DIR)/algorithms/dynamic_permutation.h \
    $(SRC_DIR)/graph/christofides.h \
    $(SRC_DIR)/graph/connectivity.h \
    $(SRC_DIR)/graph/ebert_graph.h \
    $(SRC_DIR)/graph/eulerian_path.h \
    $(GEN_DIR)/graph/flow_problem.pb.h \
    $(SRC_DIR)/graph/graph.h \
    $(SRC_DIR)/graph/minimum_spanning_tree.h \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/lp_data/matrix_scaler.h \
    $(SRC_DIR)/lp_data/permutation.h \
    $(SRC_DIR)/lp_data/sparse_column.h \
    $(SRC_DIR)/lp_data/sparse.h \
    $(SRC_DIR)/lp_data/sparse_vector.h \
    $(SRC_DIR)/glop/basis_representation.h \
    $(SRC_DIR)/glop/dual_edge_norms.h \
    $(SRC_DIR)/glop/entering_variable.h \
    $(SRC_DIR)/glop/lu_factorization.h \
    $(SRC_DIR)/glop/markowitz.h \
    $(GEN_DIR)/glop/parameters.pb.h \
    $(SRC_DIR)/glop/preprocessor.h \
    $(SRC_DIR)/glop/primal_edge_norms.h \
    $(SRC_DIR)/glop/rank_one_update.h \
    $(SRC_DIR)/glop/reduced_costs.h \
    $(SRC_DIR)/glop/revised_simplex.h \
    $(SRC_DIR)/glop/status.h \
    $(SRC_DIR)/glop/update_row.h \
    $(SRC_DIR)/glop/variables_info.h \
    $(SRC_DIR)/glop/variable_values.h \
    $(SRC_DIR)/linear_solver/linear_solver.h \
    $(GEN_DIR)/linear_solver/linear_solver.pb.h

SAT_LIB_OBJS = \
    $(OBJ_DIR)/sat/boolean_problem.$O \
    $(OBJ_DIR)/sat/clause.$O \
    $(OBJ_DIR)/sat/cp_constraints.$O \
    $(OBJ_DIR)/sat/cumulative.$O \
    $(OBJ_DIR)/sat/disjunctive.$O \
    $(OBJ_DIR)/sat/drat.$O \
    $(OBJ_DIR)/sat/encoding.$O \
    $(OBJ_DIR)/sat/flow_costs.$O \
    $(OBJ_DIR)/sat/integer.$O \
    $(OBJ_DIR)/sat/integer_expr.$O \
    $(OBJ_DIR)/sat/intervals.$O \
    $(OBJ_DIR)/sat/lp_utils.$O \
    $(OBJ_DIR)/sat/no_cycle.$O \
    $(OBJ_DIR)/sat/optimization.$O \
    $(OBJ_DIR)/sat/overload_checker.$O \
    $(OBJ_DIR)/sat/pb_constraint.$O \
    $(OBJ_DIR)/sat/precedences.$O \
    $(OBJ_DIR)/sat/sat_solver.$O \
    $(OBJ_DIR)/sat/simplification.$O \
    $(OBJ_DIR)/sat/symmetry.$O \
    $(OBJ_DIR)/sat/table.$O \
    $(OBJ_DIR)/sat/timetable.$O \
    $(OBJ_DIR)/sat/timetable_edgefinding.$O \
    $(OBJ_DIR)/sat/util.$O \
    $(OBJ_DIR)/sat/boolean_problem.pb.$O \
    $(OBJ_DIR)/sat/sat_parameters.pb.$O

$(SRC_DIR)/sat/boolean_problem.h: \
    $(GEN_DIR)/sat/boolean_problem.pb.h \
    $(SRC_DIR)/sat/sat_solver.h \
    $(SRC_DIR)/sat/simplification.h \
    $(SRC_DIR)/base/status.h \
    $(SRC_DIR)/algorithms/sparse_permutation.h

$(SRC_DIR)/sat/clause.h: \
    $(SRC_DIR)/sat/sat_base.h \
    $(GEN_DIR)/sat/sat_parameters.pb.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/int_type_indexed_vector.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/random.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/base/timer.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/stats.h

$(SRC_DIR)/sat/cp_constraints.h: \
    $(SRC_DIR)/sat/integer.h \
    $(SRC_DIR)/sat/model.h \
    $(SRC_DIR)/util/sorted_interval_list.h

$(SRC_DIR)/sat/cumulative.h: \
    $(SRC_DIR)/sat/integer.h \
    $(SRC_DIR)/sat/intervals.h \
    $(SRC_DIR)/sat/model.h \
    $(SRC_DIR)/sat/sat_base.h

$(SRC_DIR)/sat/disjunctive.h: \
    $(SRC_DIR)/sat/integer.h \
    $(SRC_DIR)/sat/intervals.h \
    $(SRC_DIR)/sat/model.h \
    $(SRC_DIR)/sat/precedences.h \
    $(SRC_DIR)/sat/sat_base.h \
    $(SRC_DIR)/util/stats.h

$(SRC_DIR)/sat/drat.h: \
    $(SRC_DIR)/sat/model.h \
    $(SRC_DIR)/sat/sat_base.h \
    $(SRC_DIR)/base/file.h

$(SRC_DIR)/sat/encoding.h: \
    $(GEN_DIR)/sat/boolean_problem.pb.h \
    $(SRC_DIR)/sat/sat_solver.h

$(SRC_DIR)/sat/flow_costs.h: \
    $(SRC_DIR)/sat/integer.h \
    $(SRC_DIR)/sat/model.h \
    $(SRC_DIR)/sat/sat_base.h \
    $(SRC_DIR)/linear_solver/linear_solver.h

$(SRC_DIR)/sat/integer_expr.h: \
    $(SRC_DIR)/sat/integer.h \
    $(SRC_DIR)/sat/model.h \
    $(SRC_DIR)/sat/precedences.h \
    $(SRC_DIR)/sat/sat_base.h

$(SRC_DIR)/sat/integer.h: \
    $(SRC_DIR)/sat/model.h \
    $(SRC_DIR)/sat/sat_base.h \
    $(SRC_DIR)/sat/sat_solver.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/port.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/iterators.h \
    $(SRC_DIR)/util/rev.h \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/util/sorted_interval_list.h

$(SRC_DIR)/sat/intervals.h: \
    $(SRC_DIR)/sat/cp_constraints.h \
    $(SRC_DIR)/sat/integer_expr.h \
    $(SRC_DIR)/sat/integer.h \
    $(SRC_DIR)/sat/model.h \
    $(SRC_DIR)/sat/precedences.h \
    $(SRC_DIR)/sat/sat_base.h \
    $(SRC_DIR)/sat/sat_solver.h

$(SRC_DIR)/sat/lp_utils.h: \
    $(GEN_DIR)/sat/boolean_problem.pb.h \
    $(SRC_DIR)/sat/sat_solver.h \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(GEN_DIR)/linear_solver/linear_solver.pb.h

$(SRC_DIR)/sat/model.h: \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/typeid.h

$(SRC_DIR)/sat/no_cycle.h: \
    $(SRC_DIR)/sat/model.h \
    $(SRC_DIR)/sat/sat_base.h \
    $(SRC_DIR)/sat/sat_solver.h

$(SRC_DIR)/sat/optimization.h: \
    $(SRC_DIR)/sat/boolean_problem.h \
    $(SRC_DIR)/sat/integer.h \
    $(SRC_DIR)/sat/model.h \
    $(SRC_DIR)/sat/sat_solver.h

$(SRC_DIR)/sat/overload_checker.h: \
    $(SRC_DIR)/sat/integer.h \
    $(SRC_DIR)/sat/intervals.h \
    $(SRC_DIR)/sat/model.h \
    $(SRC_DIR)/sat/sat_base.h

$(SRC_DIR)/sat/pb_constraint.h: \
    $(SRC_DIR)/sat/sat_base.h \
    $(GEN_DIR)/sat/sat_parameters.pb.h \
    $(SRC_DIR)/util/stats.h

$(SRC_DIR)/sat/precedences.h: \
    $(SRC_DIR)/sat/integer.h \
    $(SRC_DIR)/sat/model.h \
    $(SRC_DIR)/sat/sat_base.h \
    $(SRC_DIR)/sat/sat_solver.h \
    $(SRC_DIR)/util/bitset.h

$(SRC_DIR)/sat/sat_base.h: \
    $(SRC_DIR)/sat/model.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/int_type_indexed_vector.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/bitset.h

$(SRC_DIR)/sat/sat_solver.h: \
    $(SRC_DIR)/sat/clause.h \
    $(SRC_DIR)/sat/drat.h \
    $(SRC_DIR)/sat/model.h \
    $(SRC_DIR)/sat/pb_constraint.h \
    $(GEN_DIR)/sat/sat_parameters.pb.h \
    $(SRC_DIR)/base/adjustable_priority_queue.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/int_type_indexed_vector.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/random.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/base/timer.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/running_stat.h \
    $(SRC_DIR)/util/stats.h \
    $(SRC_DIR)/util/time_limit.h

$(SRC_DIR)/sat/simplification.h: \
    $(SRC_DIR)/sat/drat.h \
    $(SRC_DIR)/sat/sat_base.h \
    $(GEN_DIR)/sat/sat_parameters.pb.h \
    $(SRC_DIR)/sat/sat_solver.h \
    $(SRC_DIR)/base/adjustable_priority_queue.h

$(SRC_DIR)/sat/symmetry.h: \
    $(SRC_DIR)/sat/sat_base.h \
    $(SRC_DIR)/util/stats.h \
    $(SRC_DIR)/algorithms/sparse_permutation.h

$(SRC_DIR)/sat/table.h: \
    $(SRC_DIR)/sat/integer.h \
    $(SRC_DIR)/sat/model.h

$(SRC_DIR)/sat/timetable_edgefinding.h: \
    $(SRC_DIR)/sat/integer.h \
    $(SRC_DIR)/sat/intervals.h \
    $(SRC_DIR)/sat/model.h \
    $(SRC_DIR)/sat/sat_base.h

$(SRC_DIR)/sat/timetable.h: \
    $(SRC_DIR)/sat/integer.h \
    $(SRC_DIR)/sat/intervals.h \
    $(SRC_DIR)/sat/model.h \
    $(SRC_DIR)/sat/sat_base.h

$(SRC_DIR)/sat/util.h: \
    $(GEN_DIR)/sat/sat_parameters.pb.h \
    $(SRC_DIR)/base/random.h

$(OBJ_DIR)/sat/boolean_problem.$O: \
    $(SRC_DIR)/sat/boolean_problem.cc \
    $(SRC_DIR)/sat/boolean_problem.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/algorithms/find_graph_symmetries.h \
    $(SRC_DIR)/graph/graph.h \
    $(SRC_DIR)/graph/io.h \
    $(SRC_DIR)/graph/util.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/boolean_problem.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sboolean_problem.$O

$(OBJ_DIR)/sat/clause.$O: \
    $(SRC_DIR)/sat/clause.cc \
    $(SRC_DIR)/sat/clause.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/base/sysinfo.h \
    $(SRC_DIR)/util/time_limit.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/clause.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sclause.$O

$(OBJ_DIR)/sat/cp_constraints.$O: \
    $(SRC_DIR)/sat/cp_constraints.cc \
    $(SRC_DIR)/sat/cp_constraints.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/util/sort.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/cp_constraints.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Scp_constraints.$O

$(OBJ_DIR)/sat/cumulative.$O: \
    $(SRC_DIR)/sat/cumulative.cc \
    $(SRC_DIR)/sat/cumulative.h \
    $(SRC_DIR)/sat/disjunctive.h \
    $(SRC_DIR)/sat/overload_checker.h \
    $(SRC_DIR)/sat/sat_solver.h \
    $(SRC_DIR)/sat/timetable_edgefinding.h \
    $(SRC_DIR)/sat/timetable.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/cumulative.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Scumulative.$O

$(OBJ_DIR)/sat/disjunctive.$O: \
    $(SRC_DIR)/sat/disjunctive.cc \
    $(SRC_DIR)/sat/disjunctive.h \
    $(SRC_DIR)/sat/sat_solver.h \
    $(SRC_DIR)/util/sort.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/disjunctive.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sdisjunctive.$O

$(OBJ_DIR)/sat/drat.$O: \
    $(SRC_DIR)/sat/drat.cc \
    $(SRC_DIR)/sat/drat.h \
    $(SRC_DIR)/base/commandlineflags.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/drat.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sdrat.$O

$(OBJ_DIR)/sat/encoding.$O: \
    $(SRC_DIR)/sat/encoding.cc \
    $(SRC_DIR)/sat/encoding.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/encoding.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sencoding.$O

$(OBJ_DIR)/sat/flow_costs.$O: \
    $(SRC_DIR)/sat/flow_costs.cc \
    $(SRC_DIR)/sat/flow_costs.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/flow_costs.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sflow_costs.$O

$(OBJ_DIR)/sat/integer.$O: \
    $(SRC_DIR)/sat/integer.cc \
    $(SRC_DIR)/sat/integer.h \
    $(SRC_DIR)/base/stl_util.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/integer.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sinteger.$O

$(OBJ_DIR)/sat/integer_expr.$O: \
    $(SRC_DIR)/sat/integer_expr.cc \
    $(SRC_DIR)/sat/integer_expr.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/integer_expr.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sinteger_expr.$O

$(OBJ_DIR)/sat/intervals.$O: \
    $(SRC_DIR)/sat/intervals.cc \
    $(SRC_DIR)/sat/intervals.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/intervals.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sintervals.$O

$(OBJ_DIR)/sat/lp_utils.$O: \
    $(SRC_DIR)/sat/lp_utils.cc \
    $(SRC_DIR)/sat/boolean_problem.h \
    $(SRC_DIR)/sat/lp_utils.h \
    $(SRC_DIR)/util/fp_utils.h \
    $(SRC_DIR)/lp_data/lp_print_utils.h \
    $(SRC_DIR)/glop/lp_solver.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/lp_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Slp_utils.$O

$(OBJ_DIR)/sat/no_cycle.$O: \
    $(SRC_DIR)/sat/no_cycle.cc \
    $(SRC_DIR)/sat/no_cycle.h \
    $(SRC_DIR)/base/stl_util.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/no_cycle.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sno_cycle.$O

$(OBJ_DIR)/sat/optimization.$O: \
    $(SRC_DIR)/sat/optimization.cc \
    $(SRC_DIR)/sat/encoding.h \
    $(SRC_DIR)/sat/optimization.h \
    $(SRC_DIR)/sat/util.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/optimization.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Soptimization.$O

$(OBJ_DIR)/sat/overload_checker.$O: \
    $(SRC_DIR)/sat/overload_checker.cc \
    $(SRC_DIR)/sat/overload_checker.h \
    $(SRC_DIR)/sat/sat_solver.h \
    $(SRC_DIR)/util/sort.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/overload_checker.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Soverload_checker.$O

$(OBJ_DIR)/sat/pb_constraint.$O: \
    $(SRC_DIR)/sat/pb_constraint.cc \
    $(SRC_DIR)/sat/pb_constraint.h \
    $(SRC_DIR)/base/thorough_hash.h \
    $(SRC_DIR)/util/saturated_arithmetic.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/pb_constraint.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Spb_constraint.$O

$(OBJ_DIR)/sat/precedences.$O: \
    $(SRC_DIR)/sat/precedences.cc \
    $(SRC_DIR)/sat/precedences.h \
    $(SRC_DIR)/base/cleanup.h \
    $(SRC_DIR)/base/stl_util.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/precedences.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sprecedences.$O

$(OBJ_DIR)/sat/sat_solver.$O: \
    $(SRC_DIR)/sat/sat_solver.cc \
    $(SRC_DIR)/sat/sat_solver.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/split.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/base/sysinfo.h \
    $(SRC_DIR)/util/saturated_arithmetic.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/sat_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Ssat_solver.$O

$(OBJ_DIR)/sat/simplification.$O: \
    $(SRC_DIR)/sat/simplification.cc \
    $(SRC_DIR)/sat/simplification.h \
    $(SRC_DIR)/sat/util.h \
    $(SRC_DIR)/base/random.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/base/strongly_connected_components.h \
    $(SRC_DIR)/base/timer.h \
    $(SRC_DIR)/algorithms/dynamic_partition.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/simplification.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Ssimplification.$O

$(OBJ_DIR)/sat/symmetry.$O: \
    $(SRC_DIR)/sat/symmetry.cc \
    $(SRC_DIR)/sat/symmetry.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/symmetry.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Ssymmetry.$O

$(OBJ_DIR)/sat/table.$O: \
    $(SRC_DIR)/sat/table.cc \
    $(SRC_DIR)/sat/table.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/stl_util.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/table.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Stable.$O

$(OBJ_DIR)/sat/timetable.$O: \
    $(SRC_DIR)/sat/timetable.cc \
    $(SRC_DIR)/sat/overload_checker.h \
    $(SRC_DIR)/sat/precedences.h \
    $(SRC_DIR)/sat/sat_solver.h \
    $(SRC_DIR)/sat/timetable.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/timetable.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Stimetable.$O

$(OBJ_DIR)/sat/timetable_edgefinding.$O: \
    $(SRC_DIR)/sat/timetable_edgefinding.cc \
    $(SRC_DIR)/sat/timetable_edgefinding.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/util/sort.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/timetable_edgefinding.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Stimetable_edgefinding.$O

$(OBJ_DIR)/sat/util.$O: \
    $(SRC_DIR)/sat/util.cc \
    $(SRC_DIR)/sat/util.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/util.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sutil.$O

$(GEN_DIR)/sat/boolean_problem.pb.cc: $(SRC_DIR)/sat/boolean_problem.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/sat/boolean_problem.proto

$(GEN_DIR)/sat/boolean_problem.pb.h: $(GEN_DIR)/sat/boolean_problem.pb.cc

$(OBJ_DIR)/sat/boolean_problem.pb.$O: $(GEN_DIR)/sat/boolean_problem.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/sat/boolean_problem.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sboolean_problem.pb.$O

$(GEN_DIR)/sat/sat_parameters.pb.cc: $(SRC_DIR)/sat/sat_parameters.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/sat/sat_parameters.proto

$(GEN_DIR)/sat/sat_parameters.pb.h: $(GEN_DIR)/sat/sat_parameters.pb.cc

$(OBJ_DIR)/sat/sat_parameters.pb.$O: $(GEN_DIR)/sat/sat_parameters.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/sat/sat_parameters.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Ssat_parameters.pb.$O

BOP_DEPS = \
    $(SRC_DIR)/bop/bop_base.h \
    $(SRC_DIR)/bop/bop_lns.h \
    $(GEN_DIR)/bop/bop_parameters.pb.h \
    $(SRC_DIR)/bop/bop_solution.h \
    $(SRC_DIR)/bop/bop_types.h \
    $(SRC_DIR)/bop/bop_util.h \
    $(SRC_DIR)/base/adjustable_priority_queue.h \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/casts.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/status.h \
    $(SRC_DIR)/base/stringpiece.h \
    $(SRC_DIR)/base/thorough_hash.h \
    $(SRC_DIR)/base/time_support.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/running_stat.h \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/lp_data/matrix_scaler.h \
    $(SRC_DIR)/lp_data/permutation.h \
    $(SRC_DIR)/lp_data/sparse_column.h \
    $(SRC_DIR)/lp_data/sparse.h \
    $(SRC_DIR)/lp_data/sparse_vector.h \
    $(SRC_DIR)/glop/basis_representation.h \
    $(SRC_DIR)/glop/dual_edge_norms.h \
    $(SRC_DIR)/glop/entering_variable.h \
    $(SRC_DIR)/glop/lu_factorization.h \
    $(SRC_DIR)/glop/markowitz.h \
    $(GEN_DIR)/glop/parameters.pb.h \
    $(SRC_DIR)/glop/preprocessor.h \
    $(SRC_DIR)/glop/primal_edge_norms.h \
    $(SRC_DIR)/glop/rank_one_update.h \
    $(SRC_DIR)/glop/reduced_costs.h \
    $(SRC_DIR)/glop/revised_simplex.h \
    $(SRC_DIR)/glop/status.h \
    $(SRC_DIR)/glop/update_row.h \
    $(SRC_DIR)/glop/variables_info.h \
    $(SRC_DIR)/glop/variable_values.h \
    $(SRC_DIR)/sat/boolean_problem.h \
    $(GEN_DIR)/sat/boolean_problem.pb.h \
    $(SRC_DIR)/sat/clause.h \
    $(SRC_DIR)/sat/cp_constraints.h \
    $(SRC_DIR)/sat/drat.h \
    $(SRC_DIR)/sat/integer_expr.h \
    $(SRC_DIR)/sat/integer.h \
    $(SRC_DIR)/sat/intervals.h \
    $(SRC_DIR)/sat/model.h \
    $(SRC_DIR)/sat/pb_constraint.h \
    $(SRC_DIR)/sat/precedences.h \
    $(SRC_DIR)/sat/sat_base.h \
    $(GEN_DIR)/sat/sat_parameters.pb.h \
    $(SRC_DIR)/sat/sat_solver.h \
    $(SRC_DIR)/sat/simplification.h

BOP_LIB_OBJS = \
    $(OBJ_DIR)/bop/bop_base.$O \
    $(OBJ_DIR)/bop/bop_fs.$O \
    $(OBJ_DIR)/bop/bop_lns.$O \
    $(OBJ_DIR)/bop/bop_ls.$O \
    $(OBJ_DIR)/bop/bop_portfolio.$O \
    $(OBJ_DIR)/bop/bop_solution.$O \
    $(OBJ_DIR)/bop/bop_solver.$O \
    $(OBJ_DIR)/bop/bop_util.$O \
    $(OBJ_DIR)/bop/complete_optimizer.$O \
    $(OBJ_DIR)/bop/integral_solver.$O \
    $(OBJ_DIR)/bop/bop_parameters.pb.$O

$(SRC_DIR)/bop/bop_base.h: \
    $(GEN_DIR)/bop/bop_parameters.pb.h \
    $(SRC_DIR)/bop/bop_solution.h \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/util/stats.h \
    $(SRC_DIR)/util/time_limit.h \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(GEN_DIR)/sat/boolean_problem.pb.h \
    $(SRC_DIR)/sat/clause.h \
    $(SRC_DIR)/sat/sat_base.h

$(SRC_DIR)/bop/bop_fs.h: \
    $(SRC_DIR)/bop/bop_base.h \
    $(GEN_DIR)/bop/bop_parameters.pb.h \
    $(SRC_DIR)/bop/bop_solution.h \
    $(SRC_DIR)/bop/bop_types.h \
    $(SRC_DIR)/bop/bop_util.h \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/int_type_indexed_vector.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/util/time_limit.h \
    $(SRC_DIR)/glop/lp_solver.h \
    $(GEN_DIR)/sat/boolean_problem.pb.h \
    $(SRC_DIR)/sat/sat_solver.h

$(SRC_DIR)/bop/bop_lns.h: \
    $(SRC_DIR)/bop/bop_base.h \
    $(GEN_DIR)/bop/bop_parameters.pb.h \
    $(SRC_DIR)/bop/bop_solution.h \
    $(SRC_DIR)/bop/bop_types.h \
    $(SRC_DIR)/bop/bop_util.h \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/int_type_indexed_vector.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/util/stats.h \
    $(SRC_DIR)/util/time_limit.h \
    $(SRC_DIR)/glop/lp_solver.h \
    $(GEN_DIR)/sat/boolean_problem.pb.h \
    $(SRC_DIR)/sat/sat_solver.h

$(SRC_DIR)/bop/bop_ls.h: \
    $(SRC_DIR)/bop/bop_base.h \
    $(SRC_DIR)/bop/bop_solution.h \
    $(SRC_DIR)/bop/bop_types.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/random.h \
    $(GEN_DIR)/sat/boolean_problem.pb.h \
    $(SRC_DIR)/sat/sat_solver.h

$(SRC_DIR)/bop/bop_portfolio.h: \
    $(SRC_DIR)/bop/bop_base.h \
    $(SRC_DIR)/bop/bop_lns.h \
    $(GEN_DIR)/bop/bop_parameters.pb.h \
    $(SRC_DIR)/bop/bop_solution.h \
    $(SRC_DIR)/bop/bop_types.h \
    $(SRC_DIR)/util/stats.h \
    $(SRC_DIR)/util/time_limit.h \
    $(SRC_DIR)/glop/lp_solver.h \
    $(GEN_DIR)/sat/boolean_problem.pb.h \
    $(SRC_DIR)/sat/sat_solver.h

$(SRC_DIR)/bop/bop_solution.h: \
    $(SRC_DIR)/bop/bop_types.h \
    $(SRC_DIR)/sat/boolean_problem.h \
    $(GEN_DIR)/sat/boolean_problem.pb.h

$(SRC_DIR)/bop/bop_solver.h: \
    $(SRC_DIR)/bop/bop_base.h \
    $(GEN_DIR)/bop/bop_parameters.pb.h \
    $(SRC_DIR)/bop/bop_solution.h \
    $(SRC_DIR)/bop/bop_types.h \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/int_type_indexed_vector.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/util/stats.h \
    $(SRC_DIR)/util/time_limit.h \
    $(SRC_DIR)/glop/lp_solver.h \
    $(GEN_DIR)/sat/boolean_problem.pb.h \
    $(SRC_DIR)/sat/sat_solver.h

$(SRC_DIR)/bop/bop_types.h: \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/int_type_indexed_vector.h

$(SRC_DIR)/bop/bop_util.h: \
    $(SRC_DIR)/bop/bop_base.h \
    $(SRC_DIR)/bop/bop_solution.h \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/sat/sat_solver.h

$(SRC_DIR)/bop/complete_optimizer.h: \
    $(SRC_DIR)/bop/bop_base.h \
    $(SRC_DIR)/bop/bop_solution.h \
    $(SRC_DIR)/bop/bop_types.h \
    $(GEN_DIR)/sat/boolean_problem.pb.h \
    $(SRC_DIR)/sat/encoding.h \
    $(SRC_DIR)/sat/sat_solver.h

$(SRC_DIR)/bop/integral_solver.h: \
    $(GEN_DIR)/bop/bop_parameters.pb.h \
    $(SRC_DIR)/bop/bop_types.h \
    $(SRC_DIR)/base/port.h \
    $(SRC_DIR)/util/time_limit.h \
    $(SRC_DIR)/lp_data/lp_data.h

$(OBJ_DIR)/bop/bop_base.$O: \
    $(SRC_DIR)/bop/bop_base.cc \
    $(SRC_DIR)/bop/bop_base.h \
    $(SRC_DIR)/sat/boolean_problem.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/bop/bop_base.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_base.$O

$(OBJ_DIR)/bop/bop_fs.$O: \
    $(SRC_DIR)/bop/bop_fs.cc \
    $(SRC_DIR)/bop/bop_fs.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/lp_data/lp_print_utils.h \
    $(SRC_DIR)/glop/lp_solver.h \
    $(SRC_DIR)/sat/boolean_problem.h \
    $(SRC_DIR)/sat/lp_utils.h \
    $(SRC_DIR)/sat/sat_solver.h \
    $(SRC_DIR)/sat/symmetry.h \
    $(SRC_DIR)/sat/util.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/bop/bop_fs.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_fs.$O

$(OBJ_DIR)/bop/bop_lns.$O: \
    $(SRC_DIR)/bop/bop_lns.cc \
    $(SRC_DIR)/bop/bop_lns.h \
    $(SRC_DIR)/base/cleanup.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/lp_data/lp_print_utils.h \
    $(SRC_DIR)/glop/lp_solver.h \
    $(SRC_DIR)/sat/boolean_problem.h \
    $(SRC_DIR)/sat/lp_utils.h \
    $(SRC_DIR)/sat/sat_solver.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/bop/bop_lns.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_lns.$O

$(OBJ_DIR)/bop/bop_ls.$O: \
    $(SRC_DIR)/bop/bop_ls.cc \
    $(SRC_DIR)/bop/bop_ls.h \
    $(SRC_DIR)/bop/bop_util.h \
    $(SRC_DIR)/sat/boolean_problem.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/bop/bop_ls.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_ls.$O

$(OBJ_DIR)/bop/bop_portfolio.$O: \
    $(SRC_DIR)/bop/bop_portfolio.cc \
    $(SRC_DIR)/bop/bop_fs.h \
    $(SRC_DIR)/bop/bop_lns.h \
    $(SRC_DIR)/bop/bop_ls.h \
    $(SRC_DIR)/bop/bop_portfolio.h \
    $(SRC_DIR)/bop/bop_util.h \
    $(SRC_DIR)/bop/complete_optimizer.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/sat/boolean_problem.h \
    $(SRC_DIR)/sat/symmetry.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/bop/bop_portfolio.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_portfolio.$O

$(OBJ_DIR)/bop/bop_solution.$O: \
    $(SRC_DIR)/bop/bop_solution.cc \
    $(SRC_DIR)/bop/bop_solution.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/bop/bop_solution.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_solution.$O

$(OBJ_DIR)/bop/bop_solver.$O: \
    $(SRC_DIR)/bop/bop_solver.cc \
    $(SRC_DIR)/bop/bop_fs.h \
    $(SRC_DIR)/bop/bop_lns.h \
    $(SRC_DIR)/bop/bop_ls.h \
    $(SRC_DIR)/bop/bop_portfolio.h \
    $(SRC_DIR)/bop/bop_solver.h \
    $(SRC_DIR)/bop/bop_util.h \
    $(SRC_DIR)/bop/complete_optimizer.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/lp_data/lp_print_utils.h \
    $(SRC_DIR)/glop/lp_solver.h \
    $(SRC_DIR)/sat/boolean_problem.h \
    $(SRC_DIR)/sat/lp_utils.h \
    $(SRC_DIR)/sat/sat_solver.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/bop/bop_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_solver.$O

$(OBJ_DIR)/bop/bop_util.$O: \
    $(SRC_DIR)/bop/bop_util.cc \
    $(SRC_DIR)/bop/bop_base.h \
    $(SRC_DIR)/bop/bop_solution.h \
    $(SRC_DIR)/bop/bop_util.h \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/sat/boolean_problem.h \
    $(SRC_DIR)/sat/sat_solver.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/bop/bop_util.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_util.$O

$(OBJ_DIR)/bop/complete_optimizer.$O: \
    $(SRC_DIR)/bop/complete_optimizer.cc \
    $(SRC_DIR)/bop/bop_util.h \
    $(SRC_DIR)/bop/complete_optimizer.h \
    $(SRC_DIR)/sat/boolean_problem.h \
    $(SRC_DIR)/sat/optimization.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/bop/complete_optimizer.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Scomplete_optimizer.$O

$(OBJ_DIR)/bop/integral_solver.$O: \
    $(SRC_DIR)/bop/integral_solver.cc \
    $(SRC_DIR)/bop/bop_solver.h \
    $(SRC_DIR)/bop/integral_solver.h \
    $(SRC_DIR)/lp_data/lp_decomposer.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/bop/integral_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sintegral_solver.$O

$(GEN_DIR)/bop/bop_parameters.pb.cc: $(SRC_DIR)/bop/bop_parameters.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/bop/bop_parameters.proto

$(GEN_DIR)/bop/bop_parameters.pb.h: $(GEN_DIR)/bop/bop_parameters.pb.cc

$(OBJ_DIR)/bop/bop_parameters.pb.$O: $(GEN_DIR)/bop/bop_parameters.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/bop/bop_parameters.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_parameters.pb.$O

LP_DEPS = \
    $(SRC_DIR)/linear_solver/linear_solver.h \
    $(GEN_DIR)/linear_solver/linear_solver.pb.h \
    $(SRC_DIR)/base/adjustable_priority_queue.h \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/casts.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/status.h \
    $(SRC_DIR)/base/stringpiece.h \
    $(SRC_DIR)/base/thorough_hash.h \
    $(SRC_DIR)/base/time_support.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/running_stat.h \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/lp_data/matrix_scaler.h \
    $(SRC_DIR)/lp_data/permutation.h \
    $(SRC_DIR)/lp_data/sparse_column.h \
    $(SRC_DIR)/lp_data/sparse.h \
    $(SRC_DIR)/lp_data/sparse_vector.h \
    $(SRC_DIR)/glop/basis_representation.h \
    $(SRC_DIR)/glop/dual_edge_norms.h \
    $(SRC_DIR)/glop/entering_variable.h \
    $(SRC_DIR)/glop/lu_factorization.h \
    $(SRC_DIR)/glop/markowitz.h \
    $(GEN_DIR)/glop/parameters.pb.h \
    $(SRC_DIR)/glop/preprocessor.h \
    $(SRC_DIR)/glop/primal_edge_norms.h \
    $(SRC_DIR)/glop/rank_one_update.h \
    $(SRC_DIR)/glop/reduced_costs.h \
    $(SRC_DIR)/glop/revised_simplex.h \
    $(SRC_DIR)/glop/status.h \
    $(SRC_DIR)/glop/update_row.h \
    $(SRC_DIR)/glop/variables_info.h \
    $(SRC_DIR)/glop/variable_values.h \
    $(SRC_DIR)/bop/bop_base.h \
    $(SRC_DIR)/bop/bop_lns.h \
    $(GEN_DIR)/bop/bop_parameters.pb.h \
    $(SRC_DIR)/bop/bop_solution.h \
    $(SRC_DIR)/bop/bop_types.h \
    $(SRC_DIR)/bop/bop_util.h

LP_LIB_OBJS = \
    $(OBJ_DIR)/linear_solver/bop_interface.$O \
    $(OBJ_DIR)/linear_solver/cbc_interface.$O \
    $(OBJ_DIR)/linear_solver/clp_interface.$O \
    $(OBJ_DIR)/linear_solver/cplex_interface.$O \
    $(OBJ_DIR)/linear_solver/glop_interface.$O \
    $(OBJ_DIR)/linear_solver/glop_utils.$O \
    $(OBJ_DIR)/linear_solver/glpk_interface.$O \
    $(OBJ_DIR)/linear_solver/gurobi_interface.$O \
    $(OBJ_DIR)/linear_solver/linear_solver.$O \
    $(OBJ_DIR)/linear_solver/model_exporter.$O \
    $(OBJ_DIR)/linear_solver/model_validator.$O \
    $(OBJ_DIR)/linear_solver/scip_interface.$O \
    $(OBJ_DIR)/linear_solver/sulum_interface.$O \
    $(OBJ_DIR)/linear_solver/linear_solver.pb.$O

$(SRC_DIR)/linear_solver/glop_utils.h: \
    $(SRC_DIR)/linear_solver/linear_solver.h \
    $(SRC_DIR)/lp_data/lp_types.h

$(SRC_DIR)/linear_solver/linear_solver_ext.h: \
    $(SRC_DIR)/linear_solver/linear_solver.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/sparsetable.h \
    $(SRC_DIR)/base/strutil.h \
    $(SRC_DIR)/base/timer.h

$(SRC_DIR)/linear_solver/linear_solver.h: \
    $(GEN_DIR)/linear_solver/linear_solver.pb.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/timer.h \
    $(GEN_DIR)/glop/parameters.pb.h

$(SRC_DIR)/linear_solver/model_exporter.h: \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/macros.h

$(SRC_DIR)/linear_solver/model_validator.h: \
    $(GEN_DIR)/linear_solver/linear_solver.pb.h

$(OBJ_DIR)/linear_solver/bop_interface.$O: \
    $(SRC_DIR)/linear_solver/bop_interface.cc \
    $(SRC_DIR)/linear_solver/linear_solver.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(GEN_DIR)/bop/bop_parameters.pb.h \
    $(SRC_DIR)/bop/integral_solver.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/bop_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Sbop_interface.$O

$(OBJ_DIR)/linear_solver/cbc_interface.$O: \
    $(SRC_DIR)/linear_solver/cbc_interface.cc \
    $(SRC_DIR)/linear_solver/linear_solver.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/base/timer.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/cbc_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Scbc_interface.$O

$(OBJ_DIR)/linear_solver/clp_interface.$O: \
    $(SRC_DIR)/linear_solver/clp_interface.cc \
    $(SRC_DIR)/linear_solver/linear_solver.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/base/strutil.h \
    $(SRC_DIR)/base/timer.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/clp_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Sclp_interface.$O

$(OBJ_DIR)/linear_solver/cplex_interface.$O: \
    $(SRC_DIR)/linear_solver/cplex_interface.cc \
    $(SRC_DIR)/linear_solver/linear_solver.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/base/timer.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/cplex_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Scplex_interface.$O

$(OBJ_DIR)/linear_solver/glop_interface.$O: \
    $(SRC_DIR)/linear_solver/glop_interface.cc \
    $(SRC_DIR)/linear_solver/glop_utils.h \
    $(SRC_DIR)/linear_solver/linear_solver.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/time_limit.h \
    $(SRC_DIR)/lp_data/lp_data.h \
    $(SRC_DIR)/lp_data/lp_types.h \
    $(SRC_DIR)/glop/lp_solver.h \
    $(GEN_DIR)/glop/parameters.pb.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/glop_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Sglop_interface.$O

$(OBJ_DIR)/linear_solver/glop_utils.$O: \
    $(SRC_DIR)/linear_solver/glop_utils.cc \
    $(SRC_DIR)/linear_solver/glop_utils.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/glop_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Sglop_utils.$O

$(OBJ_DIR)/linear_solver/glpk_interface.$O: \
    $(SRC_DIR)/linear_solver/glpk_interface.cc \
    $(SRC_DIR)/linear_solver/linear_solver.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/base/timer.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/glpk_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Sglpk_interface.$O

$(OBJ_DIR)/linear_solver/gurobi_interface.$O: \
    $(SRC_DIR)/linear_solver/gurobi_interface.cc \
    $(SRC_DIR)/linear_solver/linear_solver.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/base/timer.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/gurobi_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Sgurobi_interface.$O

$(OBJ_DIR)/linear_solver/linear_solver.$O: \
    $(SRC_DIR)/linear_solver/linear_solver.cc \
    $(SRC_DIR)/linear_solver/linear_solver.h \
    $(GEN_DIR)/linear_solver/linear_solver.pb.h \
    $(SRC_DIR)/linear_solver/model_exporter.h \
    $(SRC_DIR)/linear_solver/model_validator.h \
    $(SRC_DIR)/base/accurate_sum.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/numbers.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/base/timer.h \
    $(SRC_DIR)/util/fp_utils.h \
    $(SRC_DIR)/util/proto_tools.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/linear_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Slinear_solver.$O

$(OBJ_DIR)/linear_solver/model_exporter.$O: \
    $(SRC_DIR)/linear_solver/model_exporter.cc \
    $(GEN_DIR)/linear_solver/linear_solver.pb.h \
    $(SRC_DIR)/linear_solver/model_exporter.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/base/strutil.h \
    $(SRC_DIR)/util/fp_utils.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/model_exporter.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Smodel_exporter.$O

$(OBJ_DIR)/linear_solver/model_validator.$O: \
    $(SRC_DIR)/linear_solver/model_validator.cc \
    $(SRC_DIR)/linear_solver/model_validator.h \
    $(SRC_DIR)/base/accurate_sum.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/util/fp_utils.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/model_validator.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Smodel_validator.$O

$(OBJ_DIR)/linear_solver/scip_interface.$O: \
    $(SRC_DIR)/linear_solver/scip_interface.cc \
    $(SRC_DIR)/linear_solver/linear_solver.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/base/timer.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/scip_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Sscip_interface.$O

$(OBJ_DIR)/linear_solver/sulum_interface.$O: \
    $(SRC_DIR)/linear_solver/sulum_interface.cc \
    $(SRC_DIR)/linear_solver/linear_solver.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/base/timer.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/sulum_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Ssulum_interface.$O

$(GEN_DIR)/linear_solver/linear_solver.pb.cc: $(SRC_DIR)/linear_solver/linear_solver.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/linear_solver/linear_solver.proto

$(GEN_DIR)/linear_solver/linear_solver.pb.h: $(GEN_DIR)/linear_solver/linear_solver.pb.cc

$(OBJ_DIR)/linear_solver/linear_solver.pb.$O: $(GEN_DIR)/linear_solver/linear_solver.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/linear_solver/linear_solver.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Slinear_solver.pb.$O

CP_DEPS = \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(GEN_DIR)/constraint_solver/model.pb.h \
    $(GEN_DIR)/constraint_solver/routing_parameters.pb.h \
    $(GEN_DIR)/constraint_solver/solver_parameters.pb.h \
    $(SRC_DIR)/base/adjustable_priority_queue.h \
    $(SRC_DIR)/base/basictypes.h \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/casts.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/status.h \
    $(SRC_DIR)/base/stringpiece.h \
    $(SRC_DIR)/base/thorough_hash.h \
    $(SRC_DIR)/base/time_support.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/running_stat.h \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/graph/christofides.h \
    $(SRC_DIR)/graph/connectivity.h \
    $(SRC_DIR)/graph/ebert_graph.h \
    $(SRC_DIR)/graph/eulerian_path.h \
    $(GEN_DIR)/graph/flow_problem.pb.h \
    $(SRC_DIR)/graph/graph.h \
    $(SRC_DIR)/graph/minimum_spanning_tree.h \
    $(SRC_DIR)/linear_solver/linear_solver.h \
    $(GEN_DIR)/linear_solver/linear_solver.pb.h \
    $(SRC_DIR)/sat/boolean_problem.h \
    $(GEN_DIR)/sat/boolean_problem.pb.h \
    $(SRC_DIR)/sat/clause.h \
    $(SRC_DIR)/sat/cp_constraints.h \
    $(SRC_DIR)/sat/drat.h \
    $(SRC_DIR)/sat/integer_expr.h \
    $(SRC_DIR)/sat/integer.h \
    $(SRC_DIR)/sat/intervals.h \
    $(SRC_DIR)/sat/model.h \
    $(SRC_DIR)/sat/pb_constraint.h \
    $(SRC_DIR)/sat/precedences.h \
    $(SRC_DIR)/sat/sat_base.h \
    $(GEN_DIR)/sat/sat_parameters.pb.h \
    $(SRC_DIR)/sat/sat_solver.h \
    $(SRC_DIR)/sat/simplification.h

CP_LIB_OBJS = \
    $(OBJ_DIR)/constraint_solver/ac4_mdd_reset_table.$O \
    $(OBJ_DIR)/constraint_solver/ac4r_table.$O \
    $(OBJ_DIR)/constraint_solver/alldiff_cst.$O \
    $(OBJ_DIR)/constraint_solver/assignment.$O \
    $(OBJ_DIR)/constraint_solver/collect_variables.$O \
    $(OBJ_DIR)/constraint_solver/constraints.$O \
    $(OBJ_DIR)/constraint_solver/constraint_solver.$O \
    $(OBJ_DIR)/constraint_solver/count_cst.$O \
    $(OBJ_DIR)/constraint_solver/default_search.$O \
    $(OBJ_DIR)/constraint_solver/demon_profiler.$O \
    $(OBJ_DIR)/constraint_solver/deviation.$O \
    $(OBJ_DIR)/constraint_solver/diffn.$O \
    $(OBJ_DIR)/constraint_solver/element.$O \
    $(OBJ_DIR)/constraint_solver/expr_array.$O \
    $(OBJ_DIR)/constraint_solver/expr_cst.$O \
    $(OBJ_DIR)/constraint_solver/expressions.$O \
    $(OBJ_DIR)/constraint_solver/gcc.$O \
    $(OBJ_DIR)/constraint_solver/graph_constraints.$O \
    $(OBJ_DIR)/constraint_solver/hybrid.$O \
    $(OBJ_DIR)/constraint_solver/interval.$O \
    $(OBJ_DIR)/constraint_solver/io.$O \
    $(OBJ_DIR)/constraint_solver/local_search.$O \
    $(OBJ_DIR)/constraint_solver/model_cache.$O \
    $(OBJ_DIR)/constraint_solver/nogoods.$O \
    $(OBJ_DIR)/constraint_solver/pack.$O \
    $(OBJ_DIR)/constraint_solver/range_cst.$O \
    $(OBJ_DIR)/constraint_solver/resource.$O \
    $(OBJ_DIR)/constraint_solver/routing.$O \
    $(OBJ_DIR)/constraint_solver/routing_flags.$O \
    $(OBJ_DIR)/constraint_solver/routing_search.$O \
    $(OBJ_DIR)/constraint_solver/sat_constraint.$O \
    $(OBJ_DIR)/constraint_solver/sched_constraints.$O \
    $(OBJ_DIR)/constraint_solver/sched_expr.$O \
    $(OBJ_DIR)/constraint_solver/sched_search.$O \
    $(OBJ_DIR)/constraint_solver/search.$O \
    $(OBJ_DIR)/constraint_solver/softgcc.$O \
    $(OBJ_DIR)/constraint_solver/table.$O \
    $(OBJ_DIR)/constraint_solver/timetabling.$O \
    $(OBJ_DIR)/constraint_solver/trace.$O \
    $(OBJ_DIR)/constraint_solver/tree_monitor.$O \
    $(OBJ_DIR)/constraint_solver/utilities.$O \
    $(OBJ_DIR)/constraint_solver/visitor.$O \
    $(OBJ_DIR)/constraint_solver/assignment.pb.$O \
    $(OBJ_DIR)/constraint_solver/demon_profiler.pb.$O \
    $(OBJ_DIR)/constraint_solver/model.pb.$O \
    $(OBJ_DIR)/constraint_solver/routing_enums.pb.$O \
    $(OBJ_DIR)/constraint_solver/routing_parameters.pb.$O \
    $(OBJ_DIR)/constraint_solver/search_limit.pb.$O \
    $(OBJ_DIR)/constraint_solver/solver_parameters.pb.$O

$(SRC_DIR)/constraint_solver/constraint_solver.h: \
    $(GEN_DIR)/constraint_solver/solver_parameters.pb.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/random.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/base/sysinfo.h \
    $(SRC_DIR)/base/timer.h \
    $(SRC_DIR)/util/sorted_interval_list.h \
    $(SRC_DIR)/util/tuple_set.h

$(SRC_DIR)/constraint_solver/constraint_solveri.h: \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(GEN_DIR)/constraint_solver/model.pb.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/sysinfo.h \
    $(SRC_DIR)/base/timer.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/tuple_set.h \
    $(SRC_DIR)/util/vector_map.h

$(SRC_DIR)/constraint_solver/hybrid.h: \
    $(SRC_DIR)/constraint_solver/constraint_solver.h

$(SRC_DIR)/constraint_solver/routing_flags.h: \
    $(GEN_DIR)/constraint_solver/routing_parameters.pb.h \
    $(SRC_DIR)/base/commandlineflags.h

$(SRC_DIR)/constraint_solver/routing.h: \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(GEN_DIR)/constraint_solver/routing_parameters.pb.h \
    $(SRC_DIR)/base/adjustable_priority_queue.h \
    $(SRC_DIR)/base/adjustable_priority_queue-inl.h \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/int_type_indexed_vector.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/util/range_query_function.h \
    $(SRC_DIR)/util/sorted_interval_list.h \
    $(SRC_DIR)/graph/graph.h

$(SRC_DIR)/constraint_solver/sat_constraint.h: \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/util/tuple_set.h \
    $(SRC_DIR)/sat/sat_solver.h

$(OBJ_DIR)/constraint_solver/ac4_mdd_reset_table.$O: \
    $(SRC_DIR)/constraint_solver/ac4_mdd_reset_table.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/int_type_indexed_vector.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/util/vector_map.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/ac4_mdd_reset_table.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sac4_mdd_reset_table.$O

$(OBJ_DIR)/constraint_solver/ac4r_table.$O: \
    $(SRC_DIR)/constraint_solver/ac4r_table.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/int_type_indexed_vector.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/util/vector_map.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/ac4r_table.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sac4r_table.$O

$(OBJ_DIR)/constraint_solver/alldiff_cst.$O: \
    $(SRC_DIR)/constraint_solver/alldiff_cst.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/util/string_array.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/alldiff_cst.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Salldiff_cst.$O

$(OBJ_DIR)/constraint_solver/assignment.$O: \
    $(SRC_DIR)/constraint_solver/assignment.cc \
    $(GEN_DIR)/constraint_solver/assignment.pb.h \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/recordio.h \
    $(SRC_DIR)/base/stringprintf.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/assignment.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sassignment.$O

$(OBJ_DIR)/constraint_solver/collect_variables.$O: \
    $(SRC_DIR)/constraint_solver/collect_variables.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/stl_util.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/collect_variables.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Scollect_variables.$O

$(OBJ_DIR)/constraint_solver/constraints.$O: \
    $(SRC_DIR)/constraint_solver/constraints.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/util/string_array.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/constraints.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sconstraints.$O

$(OBJ_DIR)/constraint_solver/constraint_solver.$O: \
    $(SRC_DIR)/constraint_solver/constraint_solver.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(GEN_DIR)/constraint_solver/model.pb.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/random.h \
    $(SRC_DIR)/base/recordio.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/base/stringpiece.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/tuple_set.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/constraint_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sconstraint_solver.$O

$(OBJ_DIR)/constraint_solver/count_cst.$O: \
    $(SRC_DIR)/constraint_solver/count_cst.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/string_array.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/count_cst.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Scount_cst.$O

$(OBJ_DIR)/constraint_solver/default_search.$O: \
    $(SRC_DIR)/constraint_solver/default_search.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/random.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/util/cached_log.h \
    $(SRC_DIR)/util/string_array.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/default_search.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sdefault_search.$O

$(OBJ_DIR)/constraint_solver/demon_profiler.$O: \
    $(SRC_DIR)/constraint_solver/demon_profiler.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(GEN_DIR)/constraint_solver/demon_profiler.pb.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/status.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/base/stringprintf.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/demon_profiler.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sdemon_profiler.$O

$(OBJ_DIR)/constraint_solver/deviation.$O: \
    $(SRC_DIR)/constraint_solver/deviation.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/mathutil.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/string_array.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/deviation.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sdeviation.$O

$(OBJ_DIR)/constraint_solver/diffn.$O: \
    $(SRC_DIR)/constraint_solver/diffn.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/int_type_indexed_vector.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/string_array.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/diffn.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sdiffn.$O

$(OBJ_DIR)/constraint_solver/element.$O: \
    $(SRC_DIR)/constraint_solver/element.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/iterators.h \
    $(SRC_DIR)/util/range_minimum_query.h \
    $(SRC_DIR)/util/string_array.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/element.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Selement.$O

$(OBJ_DIR)/constraint_solver/expr_array.$O: \
    $(SRC_DIR)/constraint_solver/expr_array.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/mathutil.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/util/string_array.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/expr_array.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sexpr_array.$O

$(OBJ_DIR)/constraint_solver/expr_cst.$O: \
    $(SRC_DIR)/constraint_solver/expr_cst.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/util/sorted_interval_list.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/expr_cst.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sexpr_cst.$O

$(OBJ_DIR)/constraint_solver/expressions.$O: \
    $(SRC_DIR)/constraint_solver/expressions.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/mathutil.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/util/string_array.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/expressions.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sexpressions.$O

$(OBJ_DIR)/constraint_solver/gcc.$O: \
    $(SRC_DIR)/constraint_solver/gcc.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/int_type.h \
    $(SRC_DIR)/base/int_type_indexed_vector.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/vector_map.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/gcc.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sgcc.$O

$(OBJ_DIR)/constraint_solver/graph_constraints.$O: \
    $(SRC_DIR)/constraint_solver/graph_constraints.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/util/string_array.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/graph_constraints.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sgraph_constraints.$O

$(OBJ_DIR)/constraint_solver/hybrid.$O: \
    $(SRC_DIR)/constraint_solver/hybrid.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/constraint_solver/hybrid.h \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/util/string_array.h \
    $(SRC_DIR)/linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/hybrid.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Shybrid.$O

$(OBJ_DIR)/constraint_solver/interval.$O: \
    $(SRC_DIR)/constraint_solver/interval.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/saturated_arithmetic.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/interval.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sinterval.$O

$(OBJ_DIR)/constraint_solver/io.$O: \
    $(SRC_DIR)/constraint_solver/io.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(GEN_DIR)/constraint_solver/model.pb.h \
    $(GEN_DIR)/constraint_solver/search_limit.pb.h \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/util/tuple_set.h \
    $(SRC_DIR)/util/vector_map.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/io.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sio.$O

$(OBJ_DIR)/constraint_solver/local_search.$O: \
    $(SRC_DIR)/constraint_solver/local_search.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/random.h \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/graph/hamiltonian_path.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/local_search.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Slocal_search.$O

$(OBJ_DIR)/constraint_solver/model_cache.$O: \
    $(SRC_DIR)/constraint_solver/model_cache.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stl_util.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/model_cache.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Smodel_cache.$O

$(OBJ_DIR)/constraint_solver/nogoods.$O: \
    $(SRC_DIR)/constraint_solver/nogoods.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/string_array.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/nogoods.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Snogoods.$O

$(OBJ_DIR)/constraint_solver/pack.$O: \
    $(SRC_DIR)/constraint_solver/pack.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringprintf.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/pack.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Spack.$O

$(OBJ_DIR)/constraint_solver/range_cst.$O: \
    $(SRC_DIR)/constraint_solver/range_cst.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/logging.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/range_cst.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Srange_cst.$O

$(OBJ_DIR)/constraint_solver/resource.$O: \
    $(SRC_DIR)/constraint_solver/resource.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/mathutil.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/monoid_operation_tree.h \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/util/string_array.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/resource.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sresource.$O

$(OBJ_DIR)/constraint_solver/routing.$O: \
    $(SRC_DIR)/constraint_solver/routing.cc \
    $(GEN_DIR)/constraint_solver/model.pb.h \
    $(SRC_DIR)/constraint_solver/routing.h \
    $(SRC_DIR)/base/callback.h \
    $(SRC_DIR)/base/casts.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/base/thorough_hash.h \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/graph/connectivity.h \
    $(SRC_DIR)/graph/linear_assignment.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/routing.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Srouting.$O

$(OBJ_DIR)/constraint_solver/routing_flags.$O: \
    $(SRC_DIR)/constraint_solver/routing_flags.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/routing_flags.h \
    $(SRC_DIR)/base/map_util.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/routing_flags.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Srouting_flags.$O

$(OBJ_DIR)/constraint_solver/routing_search.$O: \
    $(SRC_DIR)/constraint_solver/routing_search.cc \
    $(SRC_DIR)/constraint_solver/routing.h \
    $(SRC_DIR)/base/small_map.h \
    $(SRC_DIR)/base/small_ordered_set.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/saturated_arithmetic.h \
    $(SRC_DIR)/graph/christofides.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/routing_search.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Srouting_search.$O

$(OBJ_DIR)/constraint_solver/sat_constraint.$O: \
    $(SRC_DIR)/constraint_solver/sat_constraint.cc \
    $(SRC_DIR)/constraint_solver/sat_constraint.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/sat_constraint.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssat_constraint.$O

$(OBJ_DIR)/constraint_solver/sched_constraints.$O: \
    $(SRC_DIR)/constraint_solver/sched_constraints.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/string_array.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/sched_constraints.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssched_constraints.$O

$(OBJ_DIR)/constraint_solver/sched_expr.$O: \
    $(SRC_DIR)/constraint_solver/sched_expr.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/stringprintf.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/sched_expr.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssched_expr.$O

$(OBJ_DIR)/constraint_solver/sched_search.$O: \
    $(SRC_DIR)/constraint_solver/sched_search.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/string_array.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/sched_search.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssched_search.$O

$(OBJ_DIR)/constraint_solver/search.$O: \
    $(SRC_DIR)/constraint_solver/search.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(GEN_DIR)/constraint_solver/search_limit.pb.h \
    $(SRC_DIR)/base/bitmap.h \
    $(SRC_DIR)/base/casts.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/random.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/base/timer.h \
    $(SRC_DIR)/util/string_array.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/search.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssearch.$O

$(OBJ_DIR)/constraint_solver/softgcc.$O: \
    $(SRC_DIR)/constraint_solver/softgcc.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/string_array.h \
    $(SRC_DIR)/util/vector_map.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/softgcc.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssoftgcc.$O

$(OBJ_DIR)/constraint_solver/table.$O: \
    $(SRC_DIR)/constraint_solver/table.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/constraint_solver/sat_constraint.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/bitset.h \
    $(SRC_DIR)/util/string_array.h \
    $(SRC_DIR)/util/tuple_set.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/table.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Stable.$O

$(OBJ_DIR)/constraint_solver/timetabling.$O: \
    $(SRC_DIR)/constraint_solver/timetabling.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/stringprintf.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/timetabling.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Stimetabling.$O

$(OBJ_DIR)/constraint_solver/trace.$O: \
    $(SRC_DIR)/constraint_solver/trace.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/commandlineflags.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/stringprintf.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/trace.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Strace.$O

$(OBJ_DIR)/constraint_solver/tree_monitor.$O: \
    $(SRC_DIR)/constraint_solver/tree_monitor.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/base/file.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/stl_util.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/xml_helper.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/tree_monitor.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Stree_monitor.$O

$(OBJ_DIR)/constraint_solver/utilities.$O: \
    $(SRC_DIR)/constraint_solver/utilities.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/join.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/stringprintf.h \
    $(SRC_DIR)/util/bitset.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/utilities.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sutilities.$O

$(OBJ_DIR)/constraint_solver/visitor.$O: \
    $(SRC_DIR)/constraint_solver/visitor.cc \
    $(SRC_DIR)/constraint_solver/constraint_solver.h \
    $(SRC_DIR)/constraint_solver/constraint_solveri.h \
    $(SRC_DIR)/base/hash.h \
    $(SRC_DIR)/base/integral_types.h \
    $(SRC_DIR)/base/logging.h \
    $(SRC_DIR)/base/macros.h \
    $(SRC_DIR)/base/map_util.h \
    $(SRC_DIR)/base/stl_util.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/visitor.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Svisitor.$O

$(GEN_DIR)/constraint_solver/assignment.pb.cc: $(SRC_DIR)/constraint_solver/assignment.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/assignment.proto

$(GEN_DIR)/constraint_solver/assignment.pb.h: $(GEN_DIR)/constraint_solver/assignment.pb.cc

$(OBJ_DIR)/constraint_solver/assignment.pb.$O: $(GEN_DIR)/constraint_solver/assignment.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/assignment.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sassignment.pb.$O

$(GEN_DIR)/constraint_solver/demon_profiler.pb.cc: $(SRC_DIR)/constraint_solver/demon_profiler.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/demon_profiler.proto

$(GEN_DIR)/constraint_solver/demon_profiler.pb.h: $(GEN_DIR)/constraint_solver/demon_profiler.pb.cc

$(OBJ_DIR)/constraint_solver/demon_profiler.pb.$O: $(GEN_DIR)/constraint_solver/demon_profiler.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/demon_profiler.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sdemon_profiler.pb.$O

$(GEN_DIR)/constraint_solver/model.pb.cc: $(SRC_DIR)/constraint_solver/model.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/model.proto

$(GEN_DIR)/constraint_solver/model.pb.h: $(GEN_DIR)/constraint_solver/model.pb.cc \
    $(GEN_DIR)/constraint_solver/search_limit.pb.h

$(OBJ_DIR)/constraint_solver/model.pb.$O: $(GEN_DIR)/constraint_solver/model.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/model.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Smodel.pb.$O

$(GEN_DIR)/constraint_solver/routing_enums.pb.cc: $(SRC_DIR)/constraint_solver/routing_enums.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/routing_enums.proto

$(GEN_DIR)/constraint_solver/routing_enums.pb.h: $(GEN_DIR)/constraint_solver/routing_enums.pb.cc

$(OBJ_DIR)/constraint_solver/routing_enums.pb.$O: $(GEN_DIR)/constraint_solver/routing_enums.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/routing_enums.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Srouting_enums.pb.$O

$(GEN_DIR)/constraint_solver/routing_parameters.pb.cc: $(SRC_DIR)/constraint_solver/routing_parameters.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/routing_parameters.proto

$(GEN_DIR)/constraint_solver/routing_parameters.pb.h: $(GEN_DIR)/constraint_solver/routing_parameters.pb.cc \
    $(GEN_DIR)/constraint_solver/routing_enums.pb.h \
    $(GEN_DIR)/constraint_solver/solver_parameters.pb.h

$(OBJ_DIR)/constraint_solver/routing_parameters.pb.$O: $(GEN_DIR)/constraint_solver/routing_parameters.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/routing_parameters.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Srouting_parameters.pb.$O

$(GEN_DIR)/constraint_solver/search_limit.pb.cc: $(SRC_DIR)/constraint_solver/search_limit.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/search_limit.proto

$(GEN_DIR)/constraint_solver/search_limit.pb.h: $(GEN_DIR)/constraint_solver/search_limit.pb.cc

$(OBJ_DIR)/constraint_solver/search_limit.pb.$O: $(GEN_DIR)/constraint_solver/search_limit.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/search_limit.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssearch_limit.pb.$O

$(GEN_DIR)/constraint_solver/solver_parameters.pb.cc: $(SRC_DIR)/constraint_solver/solver_parameters.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/solver_parameters.proto

$(GEN_DIR)/constraint_solver/solver_parameters.pb.h: $(GEN_DIR)/constraint_solver/solver_parameters.pb.cc

$(OBJ_DIR)/constraint_solver/solver_parameters.pb.$O: $(GEN_DIR)/constraint_solver/solver_parameters.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/solver_parameters.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssolver_parameters.pb.$O

