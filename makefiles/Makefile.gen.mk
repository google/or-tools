BASE_DEPS = \
 $(SRC_DIR)/ortools/base/accurate_sum.h \
 $(SRC_DIR)/ortools/base/adjustable_priority_queue.h \
 $(SRC_DIR)/ortools/base/adjustable_priority_queue-inl.h \
 $(SRC_DIR)/ortools/base/base_export.h \
 $(SRC_DIR)/ortools/base/basictypes.h \
 $(SRC_DIR)/ortools/base/bitmap.h \
 $(SRC_DIR)/ortools/base/callback.h \
 $(SRC_DIR)/ortools/base/canonical_errors.h \
 $(SRC_DIR)/ortools/base/casts.h \
 $(SRC_DIR)/ortools/base/cleanup.h \
 $(SRC_DIR)/ortools/base/commandlineflags.h \
 $(SRC_DIR)/ortools/base/encodingutils.h \
 $(SRC_DIR)/ortools/base/file.h \
 $(SRC_DIR)/ortools/base/filelineiter.h \
 $(SRC_DIR)/ortools/base/hash.h \
 $(SRC_DIR)/ortools/base/inlined_vector.h \
 $(SRC_DIR)/ortools/base/integral_types.h \
 $(SRC_DIR)/ortools/base/int_type.h \
 $(SRC_DIR)/ortools/base/int_type_indexed_vector.h \
 $(SRC_DIR)/ortools/base/iterator_adaptors.h \
 $(SRC_DIR)/ortools/base/jniutil.h \
 $(SRC_DIR)/ortools/base/join.h \
 $(SRC_DIR)/ortools/base/logging.h \
 $(SRC_DIR)/ortools/base/macros.h \
 $(SRC_DIR)/ortools/base/map_util.h \
 $(SRC_DIR)/ortools/base/match.h \
 $(SRC_DIR)/ortools/base/mathutil.h \
 $(SRC_DIR)/ortools/base/memory.h \
 $(SRC_DIR)/ortools/base/murmur.h \
 $(SRC_DIR)/ortools/base/mutex.h \
 $(SRC_DIR)/ortools/base/notification.h \
 $(SRC_DIR)/ortools/base/numbers.h \
 $(SRC_DIR)/ortools/base/optional.h \
 $(SRC_DIR)/ortools/base/port.h \
 $(SRC_DIR)/ortools/base/protoutil.h \
 $(SRC_DIR)/ortools/base/ptr_util.h \
 $(SRC_DIR)/ortools/base/python-swig.h \
 $(SRC_DIR)/ortools/base/random.h \
 $(SRC_DIR)/ortools/base/recordio.h \
 $(SRC_DIR)/ortools/base/small_map.h \
 $(SRC_DIR)/ortools/base/small_ordered_set.h \
 $(SRC_DIR)/ortools/base/span.h \
 $(SRC_DIR)/ortools/base/sparsetable.h \
 $(SRC_DIR)/ortools/base/split.h \
 $(SRC_DIR)/ortools/base/status.h \
 $(SRC_DIR)/ortools/base/statusor.h \
 $(SRC_DIR)/ortools/base/stl_util.h \
 $(SRC_DIR)/ortools/base/stringpiece_utils.h \
 $(SRC_DIR)/ortools/base/stringprintf.h \
 $(SRC_DIR)/ortools/base/string_view.h \
 $(SRC_DIR)/ortools/base/strtoint.h \
 $(SRC_DIR)/ortools/base/strutil.h \
 $(SRC_DIR)/ortools/base/synchronization.h \
 $(SRC_DIR)/ortools/base/sysinfo.h \
 $(SRC_DIR)/ortools/base/thorough_hash.h \
 $(SRC_DIR)/ortools/base/threadpool.h \
 $(SRC_DIR)/ortools/base/timer.h \
 $(SRC_DIR)/ortools/base/time_support.h \
 $(SRC_DIR)/ortools/base/typeid.h

BASE_LIB_OBJS = \
 $(OBJ_DIR)/base/bitmap.$O \
 $(OBJ_DIR)/base/callback.$O \
 $(OBJ_DIR)/base/file.$O \
 $(OBJ_DIR)/base/join.$O \
 $(OBJ_DIR)/base/mutex.$O \
 $(OBJ_DIR)/base/notification.$O \
 $(OBJ_DIR)/base/numbers.$O \
 $(OBJ_DIR)/base/random.$O \
 $(OBJ_DIR)/base/recordio.$O \
 $(OBJ_DIR)/base/split.$O \
 $(OBJ_DIR)/base/string_view.$O \
 $(OBJ_DIR)/base/stringprintf.$O \
 $(OBJ_DIR)/base/sysinfo.$O \
 $(OBJ_DIR)/base/threadpool.$O \
 $(OBJ_DIR)/base/time_support.$O \
 $(OBJ_DIR)/base/timer.$O

objs/base/bitmap.o: ortools/base/bitmap.cc ortools/base/bitmap.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h | $(OBJ_DIR)/base
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbase$Sbitmap.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Sbitmap.$O

objs/base/callback.o: ortools/base/callback.cc ortools/base/callback.h \
 ortools/base/logging.h ortools/base/integral_types.h \
 ortools/base/macros.h | $(OBJ_DIR)/base
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbase$Scallback.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Scallback.$O

objs/base/file.o: ortools/base/file.cc ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/string_view.h \
 ortools/base/file.h ortools/base/status.h | $(OBJ_DIR)/base
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbase$Sfile.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Sfile.$O

objs/base/join.o: ortools/base/join.cc ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/string_view.h \
 ortools/base/stringprintf.h | $(OBJ_DIR)/base
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbase$Sjoin.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Sjoin.$O

objs/base/mutex.o: ortools/base/mutex.cc ortools/base/mutex.h \
 ortools/base/macros.h | $(OBJ_DIR)/base
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbase$Smutex.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Smutex.$O

objs/base/notification.o: ortools/base/notification.cc \
 ortools/base/notification.h | $(OBJ_DIR)/base
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbase$Snotification.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Snotification.$O

objs/base/numbers.o: ortools/base/numbers.cc ortools/base/numbers.h \
 ortools/base/integral_types.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/logging.h ortools/base/macros.h \
 ortools/base/string_view.h | $(OBJ_DIR)/base
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbase$Snumbers.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Snumbers.$O

objs/base/random.o: ortools/base/random.cc ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/random.h | $(OBJ_DIR)/base
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbase$Srandom.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Srandom.$O

objs/base/recordio.o: ortools/base/recordio.cc ortools/base/recordio.h \
 ortools/base/file.h ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/status.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/string_view.h | $(OBJ_DIR)/base
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbase$Srecordio.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Srecordio.$O

objs/base/split.o: ortools/base/split.cc ortools/base/split.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/string_view.h | $(OBJ_DIR)/base
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbase$Ssplit.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Ssplit.$O

objs/base/string_view.o: ortools/base/string_view.cc \
 ortools/base/string_view.h | $(OBJ_DIR)/base
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbase$Sstring_view.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Sstring_view.$O

objs/base/stringprintf.o: ortools/base/stringprintf.cc \
 ortools/base/stringprintf.h | $(OBJ_DIR)/base
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbase$Sstringprintf.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Sstringprintf.$O

objs/base/sysinfo.o: ortools/base/sysinfo.cc ortools/base/sysinfo.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h | $(OBJ_DIR)/base
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbase$Ssysinfo.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Ssysinfo.$O

objs/base/threadpool.o: ortools/base/threadpool.cc \
 ortools/base/threadpool.h | $(OBJ_DIR)/base
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbase$Sthreadpool.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Sthreadpool.$O

objs/base/time_support.o: ortools/base/time_support.cc \
 ortools/base/time_support.h ortools/base/integral_types.h | $(OBJ_DIR)/base
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbase$Stime_support.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Stime_support.$O

objs/base/timer.o: ortools/base/timer.cc ortools/base/timer.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/time_support.h | $(OBJ_DIR)/base
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbase$Stimer.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Stimer.$O

PORT_DEPS = \
 $(SRC_DIR)/ortools/port/file.h \
 $(SRC_DIR)/ortools/port/proto_utils.h \
 $(SRC_DIR)/ortools/port/sysinfo.h \
 $(SRC_DIR)/ortools/port/utf8.h

PORT_LIB_OBJS = \
 $(OBJ_DIR)/port/file_nonport.$O \
 $(OBJ_DIR)/port/sysinfo_nonport.$O

objs/port/file_nonport.o: ortools/port/file_nonport.cc \
 ortools/port/file.h ortools/base/status.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/string_view.h \
 ortools/base/file.h | $(OBJ_DIR)/port
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sport$Sfile_nonport.cc $(OBJ_OUT)$(OBJ_DIR)$Sport$Sfile_nonport.$O

objs/port/sysinfo_nonport.o: ortools/port/sysinfo_nonport.cc \
 ortools/port/sysinfo.h ortools/base/integral_types.h \
 ortools/base/sysinfo.h ortools/base/basictypes.h ortools/base/logging.h \
 ortools/base/macros.h | $(OBJ_DIR)/port
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sport$Ssysinfo_nonport.cc $(OBJ_OUT)$(OBJ_DIR)$Sport$Ssysinfo_nonport.$O

UTIL_DEPS = \
 $(SRC_DIR)/ortools/util/affine_relation.h \
 $(SRC_DIR)/ortools/util/bitset.h \
 $(SRC_DIR)/ortools/util/cached_log.h \
 $(SRC_DIR)/ortools/util/file_util.h \
 $(SRC_DIR)/ortools/util/fp_utils.h \
 $(SRC_DIR)/ortools/util/functions_swig_helpers.h \
 $(SRC_DIR)/ortools/util/functions_swig_test_helpers.h \
 $(SRC_DIR)/ortools/util/graph_export.h \
 $(SRC_DIR)/ortools/util/integer_pq.h \
 $(SRC_DIR)/ortools/util/monoid_operation_tree.h \
 $(SRC_DIR)/ortools/util/permutation.h \
 $(SRC_DIR)/ortools/util/piecewise_linear_function.h \
 $(SRC_DIR)/ortools/util/proto_tools.h \
 $(SRC_DIR)/ortools/util/random_engine.h \
 $(SRC_DIR)/ortools/util/range_minimum_query.h \
 $(SRC_DIR)/ortools/util/range_query_function.h \
 $(SRC_DIR)/ortools/util/rational_approximation.h \
 $(SRC_DIR)/ortools/util/return_macros.h \
 $(SRC_DIR)/ortools/util/rev.h \
 $(SRC_DIR)/ortools/util/running_stat.h \
 $(SRC_DIR)/ortools/util/saturated_arithmetic.h \
 $(SRC_DIR)/ortools/util/sigint.h \
 $(SRC_DIR)/ortools/util/sorted_interval_list.h \
 $(SRC_DIR)/ortools/util/sort.h \
 $(SRC_DIR)/ortools/util/stats.h \
 $(SRC_DIR)/ortools/util/string_array.h \
 $(SRC_DIR)/ortools/util/time_limit.h \
 $(SRC_DIR)/ortools/util/tuple_set.h \
 $(SRC_DIR)/ortools/util/vector_map.h \
 $(SRC_DIR)/ortools/util/vector_or_function.h \
 $(SRC_DIR)/ortools/util/xml_helper.h \
 $(SRC_DIR)/ortools/util/zvector.h \
 $(GEN_DIR)/ortools/util/optional_boolean.pb.h

UTIL_LIB_OBJS = \
 $(OBJ_DIR)/util/bitset.$O \
 $(OBJ_DIR)/util/cached_log.$O \
 $(OBJ_DIR)/util/file_util.$O \
 $(OBJ_DIR)/util/fp_utils.$O \
 $(OBJ_DIR)/util/graph_export.$O \
 $(OBJ_DIR)/util/piecewise_linear_function.$O \
 $(OBJ_DIR)/util/proto_tools.$O \
 $(OBJ_DIR)/util/range_query_function.$O \
 $(OBJ_DIR)/util/rational_approximation.$O \
 $(OBJ_DIR)/util/sigint.$O \
 $(OBJ_DIR)/util/sorted_interval_list.$O \
 $(OBJ_DIR)/util/stats.$O \
 $(OBJ_DIR)/util/time_limit.$O \
 $(OBJ_DIR)/util/xml_helper.$O \
 $(OBJ_DIR)/util/optional_boolean.pb.$O

objs/util/bitset.o: ortools/util/bitset.cc ortools/util/bitset.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h \
 ortools/base/commandlineflags.h | $(OBJ_DIR)/util
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sutil$Sbitset.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Sbitset.$O

objs/util/cached_log.o: ortools/util/cached_log.cc \
 ortools/util/cached_log.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h | $(OBJ_DIR)/util
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sutil$Scached_log.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Scached_log.$O

objs/util/file_util.o: ortools/util/file_util.cc ortools/util/file_util.h \
 ortools/base/file.h ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/status.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/string_view.h \
 ortools/base/recordio.h | $(OBJ_DIR)/util
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sutil$Sfile_util.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Sfile_util.$O

objs/util/fp_utils.o: ortools/util/fp_utils.cc ortools/util/fp_utils.h \
 ortools/base/logging.h ortools/base/integral_types.h \
 ortools/base/macros.h ortools/util/bitset.h ortools/base/basictypes.h | $(OBJ_DIR)/util
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sutil$Sfp_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Sfp_utils.$O

objs/util/graph_export.o: ortools/util/graph_export.cc \
 ortools/util/graph_export.h ortools/base/file.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/status.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/string_view.h \
 ortools/base/stringprintf.h | $(OBJ_DIR)/util
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sutil$Sgraph_export.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Sgraph_export.$O

objs/util/piecewise_linear_function.o: \
 ortools/util/piecewise_linear_function.cc \
 ortools/util/piecewise_linear_function.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/util/saturated_arithmetic.h \
 ortools/base/casts.h ortools/util/bitset.h ortools/base/stringprintf.h | $(OBJ_DIR)/util
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sutil$Spiecewise_linear_function.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Spiecewise_linear_function.$O

objs/util/proto_tools.o: ortools/util/proto_tools.cc \
 ortools/util/proto_tools.h ortools/base/join.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/string_view.h | $(OBJ_DIR)/util
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sutil$Sproto_tools.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Sproto_tools.$O

objs/util/range_query_function.o: ortools/util/range_query_function.cc \
 ortools/util/range_query_function.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h \
 ortools/util/range_minimum_query.h ortools/util/bitset.h \
 ortools/base/basictypes.h | $(OBJ_DIR)/util
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sutil$Srange_query_function.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Srange_query_function.$O

objs/util/rational_approximation.o: \
 ortools/util/rational_approximation.cc \
 ortools/util/rational_approximation.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h | $(OBJ_DIR)/util
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sutil$Srational_approximation.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Srational_approximation.$O

objs/util/sigint.o: ortools/util/sigint.cc ortools/util/sigint.h \
 ortools/base/logging.h ortools/base/integral_types.h \
 ortools/base/macros.h | $(OBJ_DIR)/util
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sutil$Ssigint.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Ssigint.$O

objs/util/sorted_interval_list.o: ortools/util/sorted_interval_list.cc \
 ortools/util/sorted_interval_list.h ortools/base/integral_types.h \
 ortools/base/span.h ortools/base/inlined_vector.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/stringprintf.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/base/basictypes.h | $(OBJ_DIR)/util
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sutil$Ssorted_interval_list.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Ssorted_interval_list.$O

objs/util/stats.o: ortools/util/stats.cc ortools/util/stats.h \
 ortools/base/timer.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/time_support.h \
 ortools/base/stringprintf.h ortools/base/stl_util.h \
 ortools/port/sysinfo.h ortools/port/utf8.h ortools/base/encodingutils.h | $(OBJ_DIR)/util
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sutil$Sstats.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Sstats.$O

objs/util/time_limit.o: ortools/util/time_limit.cc \
 ortools/util/time_limit.h ortools/base/commandlineflags.h \
 ortools/base/logging.h ortools/base/integral_types.h \
 ortools/base/macros.h ortools/base/memory.h ortools/base/port.h \
 ortools/base/time_support.h ortools/base/timer.h \
 ortools/base/basictypes.h ortools/util/running_stat.h \
 ortools/base/join.h ortools/base/string_view.h | $(OBJ_DIR)/util
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sutil$Stime_limit.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Stime_limit.$O

objs/util/xml_helper.o: ortools/util/xml_helper.cc \
 ortools/util/xml_helper.h ortools/base/macros.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/string_view.h \
 ortools/base/stringprintf.h ortools/base/strutil.h | $(OBJ_DIR)/util
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sutil$Sxml_helper.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Sxml_helper.$O

ortools/util/optional_boolean.proto: ;

$(GEN_DIR)/ortools/util/optional_boolean.pb.cc: \
 $(SRC_DIR)/ortools/util/optional_boolean.proto | $(GEN_DIR)/ortools/util
	$(PROTOC) --proto_path=$(INC_DIR) $(PROTOBUF_PROTOC_INC) --cpp_out=$(GEN_PATH) $(SRC_DIR)/ortools/util/optional_boolean.proto

$(GEN_DIR)/ortools/util/optional_boolean.pb.h: \
 $(GEN_DIR)/ortools/util/optional_boolean.pb.cc
	$(TOUCH) $(GEN_PATH)$Sortools$Sutil$Soptional_boolean.pb.h

$(OBJ_DIR)/util/optional_boolean.pb.$O: \
 $(GEN_DIR)/ortools/util/optional_boolean.pb.cc | $(OBJ_DIR)/util
	$(CCC) $(CFLAGS) -c $(GEN_PATH)$Sortools$Sutil$Soptional_boolean.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Soptional_boolean.pb.$O

DATA_DEPS = \
 $(SRC_DIR)/ortools/data/jobshop_scheduling_parser.h \
 $(SRC_DIR)/ortools/data/rcpsp_parser.h \
 $(SRC_DIR)/ortools/data/set_covering_data.h \
 $(SRC_DIR)/ortools/data/set_covering_parser.h \
 $(GEN_DIR)/ortools/data/jobshop_scheduling.pb.h \
 $(GEN_DIR)/ortools/data/rcpsp.pb.h

DATA_LIB_OBJS = \
 $(OBJ_DIR)/data/jobshop_scheduling_parser.$O \
 $(OBJ_DIR)/data/rcpsp_parser.$O \
 $(OBJ_DIR)/data/set_covering_data.$O \
 $(OBJ_DIR)/data/set_covering_parser.$O \
 $(OBJ_DIR)/data/jobshop_scheduling.pb.$O \
 $(OBJ_DIR)/data/rcpsp.pb.$O

objs/data/jobshop_scheduling_parser.o: \
 ortools/data/jobshop_scheduling_parser.cc \
 ortools/data/jobshop_scheduling_parser.h ortools/base/match.h \
 ortools/base/string_view.h \
 ortools/gen/ortools/data/jobshop_scheduling.pb.h \
 ortools/base/filelineiter.h ortools/base/file.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/status.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/stringpiece_utils.h \
 ortools/base/strutil.h ortools/base/split.h ortools/base/stringprintf.h \
 ortools/base/strtoint.h | $(OBJ_DIR)/data
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sdata$Sjobshop_scheduling_parser.cc $(OBJ_OUT)$(OBJ_DIR)$Sdata$Sjobshop_scheduling_parser.$O

objs/data/rcpsp_parser.o: ortools/data/rcpsp_parser.cc \
 ortools/data/rcpsp_parser.h ortools/base/integral_types.h \
 ortools/gen/ortools/data/rcpsp.pb.h ortools/base/filelineiter.h \
 ortools/base/file.h ortools/base/logging.h ortools/base/macros.h \
 ortools/base/status.h ortools/base/join.h ortools/base/basictypes.h \
 ortools/base/string_view.h ortools/base/stringpiece_utils.h \
 ortools/base/strutil.h ortools/base/numbers.h ortools/base/split.h \
 ortools/base/strtoint.h | $(OBJ_DIR)/data
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sdata$Srcpsp_parser.cc $(OBJ_OUT)$(OBJ_DIR)$Sdata$Srcpsp_parser.$O

objs/data/set_covering_data.o: ortools/data/set_covering_data.cc \
 ortools/data/set_covering_data.h ortools/base/integral_types.h | $(OBJ_DIR)/data
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sdata$Sset_covering_data.cc $(OBJ_OUT)$(OBJ_DIR)$Sdata$Sset_covering_data.$O

objs/data/set_covering_parser.o: ortools/data/set_covering_parser.cc \
 ortools/data/set_covering_parser.h ortools/base/integral_types.h \
 ortools/data/set_covering_data.h ortools/base/filelineiter.h \
 ortools/base/file.h ortools/base/logging.h ortools/base/macros.h \
 ortools/base/status.h ortools/base/join.h ortools/base/basictypes.h \
 ortools/base/string_view.h ortools/base/stringpiece_utils.h \
 ortools/base/strutil.h ortools/base/numbers.h ortools/base/split.h \
 ortools/base/strtoint.h | $(OBJ_DIR)/data
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sdata$Sset_covering_parser.cc $(OBJ_OUT)$(OBJ_DIR)$Sdata$Sset_covering_parser.$O

ortools/data/jobshop_scheduling.proto: ;

$(GEN_DIR)/ortools/data/jobshop_scheduling.pb.cc: \
 $(SRC_DIR)/ortools/data/jobshop_scheduling.proto | $(GEN_DIR)/ortools/data
	$(PROTOC) --proto_path=$(INC_DIR) $(PROTOBUF_PROTOC_INC) --cpp_out=$(GEN_PATH) $(SRC_DIR)/ortools/data/jobshop_scheduling.proto

$(GEN_DIR)/ortools/data/jobshop_scheduling.pb.h: \
 $(GEN_DIR)/ortools/data/jobshop_scheduling.pb.cc
	$(TOUCH) $(GEN_PATH)$Sortools$Sdata$Sjobshop_scheduling.pb.h

$(OBJ_DIR)/data/jobshop_scheduling.pb.$O: \
 $(GEN_DIR)/ortools/data/jobshop_scheduling.pb.cc | $(OBJ_DIR)/data
	$(CCC) $(CFLAGS) -c $(GEN_PATH)$Sortools$Sdata$Sjobshop_scheduling.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sdata$Sjobshop_scheduling.pb.$O

ortools/data/rcpsp.proto: ;

$(GEN_DIR)/ortools/data/rcpsp.pb.cc: \
 $(SRC_DIR)/ortools/data/rcpsp.proto | $(GEN_DIR)/ortools/data
	$(PROTOC) --proto_path=$(INC_DIR) $(PROTOBUF_PROTOC_INC) --cpp_out=$(GEN_PATH) $(SRC_DIR)/ortools/data/rcpsp.proto

$(GEN_DIR)/ortools/data/rcpsp.pb.h: \
 $(GEN_DIR)/ortools/data/rcpsp.pb.cc
	$(TOUCH) $(GEN_PATH)$Sortools$Sdata$Srcpsp.pb.h

$(OBJ_DIR)/data/rcpsp.pb.$O: \
 $(GEN_DIR)/ortools/data/rcpsp.pb.cc | $(OBJ_DIR)/data
	$(CCC) $(CFLAGS) -c $(GEN_PATH)$Sortools$Sdata$Srcpsp.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sdata$Srcpsp.pb.$O

LP_DATA_DEPS = \
 $(SRC_DIR)/ortools/lp_data/lp_data.h \
 $(SRC_DIR)/ortools/lp_data/lp_data_utils.h \
 $(SRC_DIR)/ortools/lp_data/lp_decomposer.h \
 $(SRC_DIR)/ortools/lp_data/lp_print_utils.h \
 $(SRC_DIR)/ortools/lp_data/lp_types.h \
 $(SRC_DIR)/ortools/lp_data/lp_utils.h \
 $(SRC_DIR)/ortools/lp_data/matrix_scaler.h \
 $(SRC_DIR)/ortools/lp_data/matrix_utils.h \
 $(SRC_DIR)/ortools/lp_data/model_reader.h \
 $(SRC_DIR)/ortools/lp_data/mps_reader.h \
 $(SRC_DIR)/ortools/lp_data/permutation.h \
 $(SRC_DIR)/ortools/lp_data/proto_utils.h \
 $(SRC_DIR)/ortools/lp_data/sparse_column.h \
 $(SRC_DIR)/ortools/lp_data/sparse.h \
 $(SRC_DIR)/ortools/lp_data/sparse_row.h \
 $(SRC_DIR)/ortools/lp_data/sparse_vector.h

LP_DATA_LIB_OBJS = \
 $(OBJ_DIR)/lp_data/lp_data.$O \
 $(OBJ_DIR)/lp_data/lp_data_utils.$O \
 $(OBJ_DIR)/lp_data/lp_decomposer.$O \
 $(OBJ_DIR)/lp_data/lp_print_utils.$O \
 $(OBJ_DIR)/lp_data/lp_types.$O \
 $(OBJ_DIR)/lp_data/lp_utils.$O \
 $(OBJ_DIR)/lp_data/matrix_scaler.$O \
 $(OBJ_DIR)/lp_data/matrix_utils.$O \
 $(OBJ_DIR)/lp_data/model_reader.$O \
 $(OBJ_DIR)/lp_data/mps_reader.$O \
 $(OBJ_DIR)/lp_data/proto_utils.$O \
 $(OBJ_DIR)/lp_data/sparse.$O \
 $(OBJ_DIR)/lp_data/sparse_column.$O

objs/lp_data/lp_data.o: ortools/lp_data/lp_data.cc \
 ortools/lp_data/lp_data.h ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/lp_data/lp_types.h \
 ortools/util/bitset.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/base/stringprintf.h \
 ortools/graph/iterators.h ortools/util/fp_utils.h ortools/base/join.h \
 ortools/base/string_view.h ortools/lp_data/lp_print_utils.h \
 ortools/lp_data/lp_utils.h ortools/base/accurate_sum.h \
 ortools/lp_data/matrix_utils.h | $(OBJ_DIR)/lp_data
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slp_data$Slp_data.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Slp_data.$O

objs/lp_data/lp_data_utils.o: ortools/lp_data/lp_data_utils.cc \
 ortools/lp_data/lp_data_utils.h ortools/gen/ortools/glop/parameters.pb.h \
 ortools/lp_data/lp_data.h ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/lp_data/lp_types.h \
 ortools/util/bitset.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/base/stringprintf.h \
 ortools/graph/iterators.h ortools/util/fp_utils.h \
 ortools/lp_data/matrix_scaler.h ortools/glop/revised_simplex.h \
 ortools/glop/basis_representation.h ortools/glop/lu_factorization.h \
 ortools/glop/markowitz.h ortools/base/inlined_vector.h \
 ortools/glop/status.h ortools/base/port.h ortools/util/stats.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/glop/rank_one_update.h ortools/lp_data/lp_utils.h \
 ortools/base/accurate_sum.h ortools/glop/dual_edge_norms.h \
 ortools/glop/entering_variable.h ortools/glop/primal_edge_norms.h \
 ortools/glop/update_row.h ortools/glop/variables_info.h \
 ortools/glop/reduced_costs.h ortools/util/random_engine.h \
 ortools/glop/variable_values.h ortools/lp_data/lp_print_utils.h \
 ortools/lp_data/sparse_row.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/util/running_stat.h | $(OBJ_DIR)/lp_data
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slp_data$Slp_data_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Slp_data_utils.$O

objs/lp_data/lp_decomposer.o: ortools/lp_data/lp_decomposer.cc \
 ortools/lp_data/lp_decomposer.h ortools/base/mutex.h \
 ortools/base/macros.h ortools/lp_data/lp_data.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/lp_data/lp_types.h \
 ortools/util/bitset.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/base/stringprintf.h \
 ortools/graph/iterators.h ortools/util/fp_utils.h \
 ortools/algorithms/dynamic_partition.h ortools/lp_data/lp_utils.h \
 ortools/base/accurate_sum.h | $(OBJ_DIR)/lp_data
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slp_data$Slp_decomposer.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Slp_decomposer.$O

objs/lp_data/lp_print_utils.o: ortools/lp_data/lp_print_utils.cc \
 ortools/lp_data/lp_print_utils.h ortools/base/integral_types.h \
 ortools/base/stringprintf.h ortools/lp_data/lp_types.h \
 ortools/base/basictypes.h ortools/base/logging.h ortools/base/macros.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/util/bitset.h ortools/base/join.h ortools/base/string_view.h \
 ortools/util/rational_approximation.h | $(OBJ_DIR)/lp_data
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slp_data$Slp_print_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Slp_print_utils.$O

objs/lp_data/lp_types.o: ortools/lp_data/lp_types.cc \
 ortools/lp_data/lp_types.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/util/bitset.h | $(OBJ_DIR)/lp_data
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slp_data$Slp_types.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Slp_types.$O

objs/lp_data/lp_utils.o: ortools/lp_data/lp_utils.cc \
 ortools/lp_data/lp_utils.h ortools/base/accurate_sum.h \
 ortools/lp_data/lp_types.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/util/bitset.h \
 ortools/lp_data/sparse_column.h ortools/lp_data/sparse_vector.h \
 ortools/base/stringprintf.h ortools/graph/iterators.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h | $(OBJ_DIR)/lp_data
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slp_data$Slp_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Slp_utils.$O

objs/lp_data/matrix_scaler.o: ortools/lp_data/matrix_scaler.cc \
 ortools/lp_data/matrix_scaler.h ortools/base/int_type_indexed_vector.h \
 ortools/base/int_type.h ortools/base/macros.h \
 ortools/base/integral_types.h ortools/gen/ortools/glop/parameters.pb.h \
 ortools/glop/revised_simplex.h ortools/glop/basis_representation.h \
 ortools/base/logging.h ortools/glop/lu_factorization.h \
 ortools/glop/markowitz.h ortools/base/inlined_vector.h \
 ortools/glop/status.h ortools/base/port.h ortools/lp_data/lp_types.h \
 ortools/base/basictypes.h ortools/util/bitset.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/base/stringprintf.h \
 ortools/graph/iterators.h ortools/util/stats.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/glop/rank_one_update.h \
 ortools/lp_data/lp_utils.h ortools/base/accurate_sum.h \
 ortools/glop/dual_edge_norms.h ortools/lp_data/lp_data.h \
 ortools/base/hash.h ortools/util/fp_utils.h \
 ortools/glop/entering_variable.h ortools/glop/primal_edge_norms.h \
 ortools/glop/update_row.h ortools/glop/variables_info.h \
 ortools/glop/reduced_costs.h ortools/util/random_engine.h \
 ortools/glop/variable_values.h ortools/lp_data/lp_print_utils.h \
 ortools/lp_data/sparse_row.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/util/running_stat.h | $(OBJ_DIR)/lp_data
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slp_data$Smatrix_scaler.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Smatrix_scaler.$O

objs/lp_data/matrix_utils.o: ortools/lp_data/matrix_utils.cc \
 ortools/lp_data/matrix_utils.h ortools/lp_data/lp_types.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/util/bitset.h \
 ortools/lp_data/sparse.h ortools/lp_data/permutation.h \
 ortools/base/random.h ortools/util/return_macros.h \
 ortools/lp_data/sparse_column.h ortools/lp_data/sparse_vector.h \
 ortools/base/stringprintf.h ortools/graph/iterators.h \
 ortools/base/hash.h | $(OBJ_DIR)/lp_data
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slp_data$Smatrix_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Smatrix_utils.$O

objs/lp_data/model_reader.o: ortools/lp_data/model_reader.cc \
 ortools/lp_data/model_reader.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h ortools/lp_data/lp_data.h \
 ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/lp_data/lp_types.h \
 ortools/util/bitset.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/base/stringprintf.h \
 ortools/graph/iterators.h ortools/util/fp_utils.h ortools/base/file.h \
 ortools/base/status.h ortools/base/join.h ortools/base/string_view.h \
 ortools/lp_data/mps_reader.h ortools/base/commandlineflags.h \
 ortools/base/map_util.h ortools/lp_data/proto_utils.h \
 ortools/util/file_util.h ortools/base/recordio.h | $(OBJ_DIR)/lp_data
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slp_data$Smodel_reader.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Smodel_reader.$O

objs/lp_data/mps_reader.o: ortools/lp_data/mps_reader.cc \
 ortools/lp_data/mps_reader.h ortools/base/commandlineflags.h \
 ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/base/map_util.h \
 ortools/base/stringprintf.h ortools/lp_data/lp_data.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/lp_data/lp_types.h \
 ortools/util/bitset.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/graph/iterators.h \
 ortools/util/fp_utils.h ortools/base/callback.h ortools/base/file.h \
 ortools/base/status.h ortools/base/join.h ortools/base/string_view.h \
 ortools/base/filelineiter.h ortools/base/stringpiece_utils.h \
 ortools/base/strutil.h ortools/base/match.h ortools/base/numbers.h \
 ortools/base/split.h ortools/lp_data/lp_print_utils.h | $(OBJ_DIR)/lp_data
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slp_data$Smps_reader.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Smps_reader.$O

objs/lp_data/proto_utils.o: ortools/lp_data/proto_utils.cc \
 ortools/lp_data/proto_utils.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h ortools/lp_data/lp_data.h \
 ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/lp_data/lp_types.h \
 ortools/util/bitset.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/base/stringprintf.h \
 ortools/graph/iterators.h ortools/util/fp_utils.h | $(OBJ_DIR)/lp_data
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slp_data$Sproto_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Sproto_utils.$O

objs/lp_data/sparse.o: ortools/lp_data/sparse.cc ortools/lp_data/sparse.h \
 ortools/base/integral_types.h ortools/lp_data/lp_types.h \
 ortools/base/basictypes.h ortools/base/logging.h ortools/base/macros.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/util/bitset.h ortools/lp_data/permutation.h \
 ortools/base/random.h ortools/util/return_macros.h \
 ortools/lp_data/sparse_column.h ortools/lp_data/sparse_vector.h \
 ortools/base/stringprintf.h ortools/graph/iterators.h \
 ortools/base/join.h ortools/base/string_view.h | $(OBJ_DIR)/lp_data
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slp_data$Ssparse.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Ssparse.$O

objs/lp_data/sparse_column.o: ortools/lp_data/sparse_column.cc \
 ortools/lp_data/sparse_column.h ortools/lp_data/sparse_vector.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/stringprintf.h \
 ortools/graph/iterators.h ortools/lp_data/lp_types.h \
 ortools/base/basictypes.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/util/bitset.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h | $(OBJ_DIR)/lp_data
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slp_data$Ssparse_column.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Ssparse_column.$O

GLOP_DEPS = \
 $(SRC_DIR)/ortools/glop/basis_representation.h \
 $(SRC_DIR)/ortools/glop/dual_edge_norms.h \
 $(SRC_DIR)/ortools/glop/entering_variable.h \
 $(SRC_DIR)/ortools/glop/initial_basis.h \
 $(SRC_DIR)/ortools/glop/lp_solver.h \
 $(SRC_DIR)/ortools/glop/lu_factorization.h \
 $(SRC_DIR)/ortools/glop/markowitz.h \
 $(SRC_DIR)/ortools/glop/preprocessor.h \
 $(SRC_DIR)/ortools/glop/primal_edge_norms.h \
 $(SRC_DIR)/ortools/glop/rank_one_update.h \
 $(SRC_DIR)/ortools/glop/reduced_costs.h \
 $(SRC_DIR)/ortools/glop/revised_simplex.h \
 $(SRC_DIR)/ortools/glop/status.h \
 $(SRC_DIR)/ortools/glop/update_row.h \
 $(SRC_DIR)/ortools/glop/variables_info.h \
 $(SRC_DIR)/ortools/glop/variable_values.h \
 $(GEN_DIR)/ortools/glop/parameters.pb.h

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
 $(OBJ_DIR)/glop/variable_values.$O \
 $(OBJ_DIR)/glop/variables_info.$O \
 $(OBJ_DIR)/glop/parameters.pb.$O

objs/glop/basis_representation.o: ortools/glop/basis_representation.cc \
 ortools/glop/basis_representation.h ortools/base/logging.h \
 ortools/base/integral_types.h ortools/base/macros.h \
 ortools/glop/lu_factorization.h ortools/glop/markowitz.h \
 ortools/base/inlined_vector.h ortools/gen/ortools/glop/parameters.pb.h \
 ortools/glop/status.h ortools/base/port.h ortools/lp_data/lp_types.h \
 ortools/base/basictypes.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/util/bitset.h \
 ortools/lp_data/sparse.h ortools/lp_data/permutation.h \
 ortools/base/random.h ortools/util/return_macros.h \
 ortools/lp_data/sparse_column.h ortools/lp_data/sparse_vector.h \
 ortools/base/stringprintf.h ortools/graph/iterators.h \
 ortools/util/stats.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/glop/rank_one_update.h ortools/lp_data/lp_utils.h \
 ortools/base/accurate_sum.h ortools/base/stl_util.h | $(OBJ_DIR)/glop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sglop$Sbasis_representation.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sbasis_representation.$O

objs/glop/dual_edge_norms.o: ortools/glop/dual_edge_norms.cc \
 ortools/glop/dual_edge_norms.h ortools/glop/basis_representation.h \
 ortools/base/logging.h ortools/base/integral_types.h \
 ortools/base/macros.h ortools/glop/lu_factorization.h \
 ortools/glop/markowitz.h ortools/base/inlined_vector.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/glop/status.h \
 ortools/base/port.h ortools/lp_data/lp_types.h ortools/base/basictypes.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/util/bitset.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/base/stringprintf.h \
 ortools/graph/iterators.h ortools/util/stats.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/glop/rank_one_update.h \
 ortools/lp_data/lp_utils.h ortools/base/accurate_sum.h \
 ortools/lp_data/lp_data.h ortools/base/hash.h ortools/util/fp_utils.h | $(OBJ_DIR)/glop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sglop$Sdual_edge_norms.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sdual_edge_norms.$O

objs/glop/entering_variable.o: ortools/glop/entering_variable.cc \
 ortools/glop/entering_variable.h ortools/glop/basis_representation.h \
 ortools/base/logging.h ortools/base/integral_types.h \
 ortools/base/macros.h ortools/glop/lu_factorization.h \
 ortools/glop/markowitz.h ortools/base/inlined_vector.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/glop/status.h \
 ortools/base/port.h ortools/lp_data/lp_types.h ortools/base/basictypes.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/util/bitset.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/base/stringprintf.h \
 ortools/graph/iterators.h ortools/util/stats.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/glop/rank_one_update.h \
 ortools/lp_data/lp_utils.h ortools/base/accurate_sum.h \
 ortools/glop/primal_edge_norms.h ortools/glop/update_row.h \
 ortools/glop/variables_info.h ortools/lp_data/lp_data.h \
 ortools/base/hash.h ortools/util/fp_utils.h ortools/glop/reduced_costs.h \
 ortools/util/random_engine.h ortools/port/proto_utils.h \
 ortools/base/join.h ortools/base/string_view.h | $(OBJ_DIR)/glop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sglop$Sentering_variable.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sentering_variable.$O

objs/glop/initial_basis.o: ortools/glop/initial_basis.cc \
 ortools/glop/initial_basis.h ortools/lp_data/lp_data.h \
 ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/lp_data/lp_types.h \
 ortools/util/bitset.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/base/stringprintf.h \
 ortools/graph/iterators.h ortools/util/fp_utils.h \
 ortools/glop/markowitz.h ortools/base/inlined_vector.h \
 ortools/glop/status.h ortools/base/port.h ortools/util/stats.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/lp_data/lp_utils.h ortools/base/accurate_sum.h | $(OBJ_DIR)/glop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sglop$Sinitial_basis.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sinitial_basis.$O

objs/glop/lp_solver.o: ortools/glop/lp_solver.cc ortools/glop/lp_solver.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/glop/preprocessor.h \
 ortools/glop/revised_simplex.h ortools/base/integral_types.h \
 ortools/base/macros.h ortools/glop/basis_representation.h \
 ortools/base/logging.h ortools/glop/lu_factorization.h \
 ortools/glop/markowitz.h ortools/base/inlined_vector.h \
 ortools/glop/status.h ortools/base/port.h ortools/lp_data/lp_types.h \
 ortools/base/basictypes.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/util/bitset.h \
 ortools/lp_data/sparse.h ortools/lp_data/permutation.h \
 ortools/base/random.h ortools/util/return_macros.h \
 ortools/lp_data/sparse_column.h ortools/lp_data/sparse_vector.h \
 ortools/base/stringprintf.h ortools/graph/iterators.h \
 ortools/util/stats.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/glop/rank_one_update.h ortools/lp_data/lp_utils.h \
 ortools/base/accurate_sum.h ortools/glop/dual_edge_norms.h \
 ortools/lp_data/lp_data.h ortools/base/hash.h ortools/util/fp_utils.h \
 ortools/glop/entering_variable.h ortools/glop/primal_edge_norms.h \
 ortools/glop/update_row.h ortools/glop/variables_info.h \
 ortools/glop/reduced_costs.h ortools/util/random_engine.h \
 ortools/glop/variable_values.h ortools/lp_data/lp_print_utils.h \
 ortools/lp_data/sparse_row.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/util/running_stat.h ortools/lp_data/matrix_scaler.h \
 ortools/base/join.h ortools/base/string_view.h ortools/base/strutil.h \
 ortools/lp_data/proto_utils.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h ortools/util/file_util.h \
 ortools/base/file.h ortools/base/status.h ortools/base/recordio.h | $(OBJ_DIR)/glop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sglop$Slp_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Slp_solver.$O

objs/glop/lu_factorization.o: ortools/glop/lu_factorization.cc \
 ortools/glop/lu_factorization.h ortools/glop/markowitz.h \
 ortools/base/inlined_vector.h ortools/base/logging.h \
 ortools/base/integral_types.h ortools/base/macros.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/glop/status.h \
 ortools/base/port.h ortools/lp_data/lp_types.h ortools/base/basictypes.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/util/bitset.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/base/stringprintf.h \
 ortools/graph/iterators.h ortools/util/stats.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/lp_data/lp_utils.h \
 ortools/base/accurate_sum.h | $(OBJ_DIR)/glop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sglop$Slu_factorization.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Slu_factorization.$O

objs/glop/markowitz.o: ortools/glop/markowitz.cc ortools/glop/markowitz.h \
 ortools/base/inlined_vector.h ortools/base/logging.h \
 ortools/base/integral_types.h ortools/base/macros.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/glop/status.h \
 ortools/base/port.h ortools/lp_data/lp_types.h ortools/base/basictypes.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/util/bitset.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/base/stringprintf.h \
 ortools/graph/iterators.h ortools/util/stats.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/lp_data/lp_utils.h \
 ortools/base/accurate_sum.h | $(OBJ_DIR)/glop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sglop$Smarkowitz.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Smarkowitz.$O

objs/glop/preprocessor.o: ortools/glop/preprocessor.cc \
 ortools/glop/preprocessor.h ortools/gen/ortools/glop/parameters.pb.h \
 ortools/glop/revised_simplex.h ortools/base/integral_types.h \
 ortools/base/macros.h ortools/glop/basis_representation.h \
 ortools/base/logging.h ortools/glop/lu_factorization.h \
 ortools/glop/markowitz.h ortools/base/inlined_vector.h \
 ortools/glop/status.h ortools/base/port.h ortools/lp_data/lp_types.h \
 ortools/base/basictypes.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/util/bitset.h \
 ortools/lp_data/sparse.h ortools/lp_data/permutation.h \
 ortools/base/random.h ortools/util/return_macros.h \
 ortools/lp_data/sparse_column.h ortools/lp_data/sparse_vector.h \
 ortools/base/stringprintf.h ortools/graph/iterators.h \
 ortools/util/stats.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/glop/rank_one_update.h ortools/lp_data/lp_utils.h \
 ortools/base/accurate_sum.h ortools/glop/dual_edge_norms.h \
 ortools/lp_data/lp_data.h ortools/base/hash.h ortools/util/fp_utils.h \
 ortools/glop/entering_variable.h ortools/glop/primal_edge_norms.h \
 ortools/glop/update_row.h ortools/glop/variables_info.h \
 ortools/glop/reduced_costs.h ortools/util/random_engine.h \
 ortools/glop/variable_values.h ortools/lp_data/lp_print_utils.h \
 ortools/lp_data/sparse_row.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/util/running_stat.h ortools/lp_data/matrix_scaler.h \
 ortools/lp_data/lp_data_utils.h ortools/lp_data/matrix_utils.h | $(OBJ_DIR)/glop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sglop$Spreprocessor.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Spreprocessor.$O

objs/glop/primal_edge_norms.o: ortools/glop/primal_edge_norms.cc \
 ortools/glop/primal_edge_norms.h ortools/glop/basis_representation.h \
 ortools/base/logging.h ortools/base/integral_types.h \
 ortools/base/macros.h ortools/glop/lu_factorization.h \
 ortools/glop/markowitz.h ortools/base/inlined_vector.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/glop/status.h \
 ortools/base/port.h ortools/lp_data/lp_types.h ortools/base/basictypes.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/util/bitset.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/base/stringprintf.h \
 ortools/graph/iterators.h ortools/util/stats.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/glop/rank_one_update.h \
 ortools/lp_data/lp_utils.h ortools/base/accurate_sum.h \
 ortools/glop/update_row.h ortools/glop/variables_info.h \
 ortools/lp_data/lp_data.h ortools/base/hash.h ortools/util/fp_utils.h | $(OBJ_DIR)/glop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sglop$Sprimal_edge_norms.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sprimal_edge_norms.$O

objs/glop/reduced_costs.o: ortools/glop/reduced_costs.cc \
 ortools/glop/reduced_costs.h ortools/glop/basis_representation.h \
 ortools/base/logging.h ortools/base/integral_types.h \
 ortools/base/macros.h ortools/glop/lu_factorization.h \
 ortools/glop/markowitz.h ortools/base/inlined_vector.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/glop/status.h \
 ortools/base/port.h ortools/lp_data/lp_types.h ortools/base/basictypes.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/util/bitset.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/base/stringprintf.h \
 ortools/graph/iterators.h ortools/util/stats.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/glop/rank_one_update.h \
 ortools/lp_data/lp_utils.h ortools/base/accurate_sum.h \
 ortools/glop/primal_edge_norms.h ortools/glop/update_row.h \
 ortools/glop/variables_info.h ortools/lp_data/lp_data.h \
 ortools/base/hash.h ortools/util/fp_utils.h ortools/util/random_engine.h | $(OBJ_DIR)/glop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sglop$Sreduced_costs.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sreduced_costs.$O

objs/glop/revised_simplex.o: ortools/glop/revised_simplex.cc \
 ortools/glop/revised_simplex.h ortools/base/integral_types.h \
 ortools/base/macros.h ortools/glop/basis_representation.h \
 ortools/base/logging.h ortools/glop/lu_factorization.h \
 ortools/glop/markowitz.h ortools/base/inlined_vector.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/glop/status.h \
 ortools/base/port.h ortools/lp_data/lp_types.h ortools/base/basictypes.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/util/bitset.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/base/stringprintf.h \
 ortools/graph/iterators.h ortools/util/stats.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/glop/rank_one_update.h \
 ortools/lp_data/lp_utils.h ortools/base/accurate_sum.h \
 ortools/glop/dual_edge_norms.h ortools/lp_data/lp_data.h \
 ortools/base/hash.h ortools/util/fp_utils.h \
 ortools/glop/entering_variable.h ortools/glop/primal_edge_norms.h \
 ortools/glop/update_row.h ortools/glop/variables_info.h \
 ortools/glop/reduced_costs.h ortools/util/random_engine.h \
 ortools/glop/variable_values.h ortools/lp_data/lp_print_utils.h \
 ortools/lp_data/sparse_row.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/util/running_stat.h ortools/base/join.h \
 ortools/base/string_view.h ortools/glop/initial_basis.h \
 ortools/lp_data/matrix_utils.h | $(OBJ_DIR)/glop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sglop$Srevised_simplex.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Srevised_simplex.$O

objs/glop/status.o: ortools/glop/status.cc ortools/glop/status.h \
 ortools/base/port.h ortools/base/logging.h ortools/base/integral_types.h \
 ortools/base/macros.h | $(OBJ_DIR)/glop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sglop$Sstatus.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sstatus.$O

objs/glop/update_row.o: ortools/glop/update_row.cc \
 ortools/glop/update_row.h ortools/glop/basis_representation.h \
 ortools/base/logging.h ortools/base/integral_types.h \
 ortools/base/macros.h ortools/glop/lu_factorization.h \
 ortools/glop/markowitz.h ortools/base/inlined_vector.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/glop/status.h \
 ortools/base/port.h ortools/lp_data/lp_types.h ortools/base/basictypes.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/util/bitset.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/base/stringprintf.h \
 ortools/graph/iterators.h ortools/util/stats.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/glop/rank_one_update.h \
 ortools/lp_data/lp_utils.h ortools/base/accurate_sum.h \
 ortools/glop/variables_info.h | $(OBJ_DIR)/glop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sglop$Supdate_row.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Supdate_row.$O

objs/glop/variable_values.o: ortools/glop/variable_values.cc \
 ortools/glop/variable_values.h ortools/glop/basis_representation.h \
 ortools/base/logging.h ortools/base/integral_types.h \
 ortools/base/macros.h ortools/glop/lu_factorization.h \
 ortools/glop/markowitz.h ortools/base/inlined_vector.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/glop/status.h \
 ortools/base/port.h ortools/lp_data/lp_types.h ortools/base/basictypes.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/util/bitset.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/base/stringprintf.h \
 ortools/graph/iterators.h ortools/util/stats.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/glop/rank_one_update.h \
 ortools/lp_data/lp_utils.h ortools/base/accurate_sum.h \
 ortools/glop/variables_info.h | $(OBJ_DIR)/glop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sglop$Svariable_values.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Svariable_values.$O

objs/glop/variables_info.o: ortools/glop/variables_info.cc \
 ortools/glop/variables_info.h ortools/lp_data/lp_types.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/util/bitset.h \
 ortools/lp_data/sparse.h ortools/lp_data/permutation.h \
 ortools/base/random.h ortools/util/return_macros.h \
 ortools/lp_data/sparse_column.h ortools/lp_data/sparse_vector.h \
 ortools/base/stringprintf.h ortools/graph/iterators.h | $(OBJ_DIR)/glop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sglop$Svariables_info.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Svariables_info.$O

ortools/glop/parameters.proto: ;

$(GEN_DIR)/ortools/glop/parameters.pb.cc: \
 $(SRC_DIR)/ortools/glop/parameters.proto | $(GEN_DIR)/ortools/glop
	$(PROTOC) --proto_path=$(INC_DIR) $(PROTOBUF_PROTOC_INC) --cpp_out=$(GEN_PATH) $(SRC_DIR)/ortools/glop/parameters.proto

$(GEN_DIR)/ortools/glop/parameters.pb.h: \
 $(GEN_DIR)/ortools/glop/parameters.pb.cc
	$(TOUCH) $(GEN_PATH)$Sortools$Sglop$Sparameters.pb.h

$(OBJ_DIR)/glop/parameters.pb.$O: \
 $(GEN_DIR)/ortools/glop/parameters.pb.cc | $(OBJ_DIR)/glop
	$(CCC) $(CFLAGS) -c $(GEN_PATH)$Sortools$Sglop$Sparameters.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sparameters.pb.$O

GRAPH_DEPS = \
 $(SRC_DIR)/ortools/graph/assignment.h \
 $(SRC_DIR)/ortools/graph/christofides.h \
 $(SRC_DIR)/ortools/graph/cliques.h \
 $(SRC_DIR)/ortools/graph/connected_components.h \
 $(SRC_DIR)/ortools/graph/connectivity.h \
 $(SRC_DIR)/ortools/graph/ebert_graph.h \
 $(SRC_DIR)/ortools/graph/eulerian_path.h \
 $(SRC_DIR)/ortools/graph/graph.h \
 $(SRC_DIR)/ortools/graph/graphs.h \
 $(SRC_DIR)/ortools/graph/hamiltonian_path.h \
 $(SRC_DIR)/ortools/graph/io.h \
 $(SRC_DIR)/ortools/graph/iterators.h \
 $(SRC_DIR)/ortools/graph/linear_assignment.h \
 $(SRC_DIR)/ortools/graph/max_flow.h \
 $(SRC_DIR)/ortools/graph/min_cost_flow.h \
 $(SRC_DIR)/ortools/graph/minimum_spanning_tree.h \
 $(SRC_DIR)/ortools/graph/one_tree_lower_bound.h \
 $(SRC_DIR)/ortools/graph/shortestpaths.h \
 $(SRC_DIR)/ortools/graph/strongly_connected_components.h \
 $(SRC_DIR)/ortools/graph/util.h \
 $(GEN_DIR)/ortools/graph/flow_problem.pb.h

GRAPH_LIB_OBJS = \
 $(OBJ_DIR)/graph/assignment.$O \
 $(OBJ_DIR)/graph/astar.$O \
 $(OBJ_DIR)/graph/bellman_ford.$O \
 $(OBJ_DIR)/graph/cliques.$O \
 $(OBJ_DIR)/graph/connected_components.$O \
 $(OBJ_DIR)/graph/dijkstra.$O \
 $(OBJ_DIR)/graph/linear_assignment.$O \
 $(OBJ_DIR)/graph/max_flow.$O \
 $(OBJ_DIR)/graph/min_cost_flow.$O \
 $(OBJ_DIR)/graph/shortestpaths.$O \
 $(OBJ_DIR)/graph/util.$O \
 $(OBJ_DIR)/graph/flow_problem.pb.$O

objs/graph/assignment.o: ortools/graph/assignment.cc \
 ortools/graph/assignment.h ortools/graph/ebert_graph.h \
 ortools/base/integral_types.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/logging.h ortools/base/macros.h \
 ortools/base/string_view.h ortools/base/stringprintf.h \
 ortools/util/permutation.h ortools/util/zvector.h \
 ortools/base/commandlineflags.h ortools/graph/linear_assignment.h | $(OBJ_DIR)/graph
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sgraph$Sassignment.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Sassignment.$O

objs/graph/astar.o: ortools/graph/astar.cc \
 ortools/base/adjustable_priority_queue.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/callback.h | $(OBJ_DIR)/graph
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sgraph$Sastar.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Sastar.$O

objs/graph/bellman_ford.o: ortools/graph/bellman_ford.cc \
 ortools/base/integral_types.h | $(OBJ_DIR)/graph
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sgraph$Sbellman_ford.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Sbellman_ford.$O

objs/graph/cliques.o: ortools/graph/cliques.cc ortools/graph/cliques.h \
 ortools/base/int_type.h ortools/base/macros.h \
 ortools/base/int_type_indexed_vector.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/string_view.h \
 ortools/util/time_limit.h ortools/base/commandlineflags.h \
 ortools/base/memory.h ortools/base/port.h ortools/base/time_support.h \
 ortools/base/timer.h ortools/util/running_stat.h ortools/base/hash.h | $(OBJ_DIR)/graph
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sgraph$Scliques.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Scliques.$O

objs/graph/connected_components.o: ortools/graph/connected_components.cc \
 ortools/graph/connected_components.h ortools/base/logging.h \
 ortools/base/integral_types.h ortools/base/macros.h \
 ortools/base/map_util.h ortools/base/ptr_util.h | $(OBJ_DIR)/graph
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sgraph$Sconnected_components.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Sconnected_components.$O

objs/graph/dijkstra.o: ortools/graph/dijkstra.cc \
 ortools/base/adjustable_priority_queue.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h | $(OBJ_DIR)/graph
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sgraph$Sdijkstra.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Sdijkstra.$O

objs/graph/linear_assignment.o: ortools/graph/linear_assignment.cc \
 ortools/graph/linear_assignment.h ortools/base/commandlineflags.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/stringprintf.h \
 ortools/graph/ebert_graph.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/string_view.h \
 ortools/util/permutation.h ortools/util/zvector.h | $(OBJ_DIR)/graph
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sgraph$Slinear_assignment.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Slinear_assignment.$O

objs/graph/max_flow.o: ortools/graph/max_flow.cc ortools/graph/max_flow.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/graph/ebert_graph.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/string_view.h \
 ortools/base/stringprintf.h ortools/util/permutation.h \
 ortools/util/zvector.h ortools/gen/ortools/graph/flow_problem.pb.h \
 ortools/graph/graph.h ortools/base/port.h ortools/graph/iterators.h \
 ortools/util/stats.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/base/memory.h ortools/graph/graphs.h | $(OBJ_DIR)/graph
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sgraph$Smax_flow.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Smax_flow.$O

objs/graph/min_cost_flow.o: ortools/graph/min_cost_flow.cc \
 ortools/graph/min_cost_flow.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/graph/ebert_graph.h \
 ortools/base/join.h ortools/base/basictypes.h ortools/base/string_view.h \
 ortools/base/stringprintf.h ortools/util/permutation.h \
 ortools/util/zvector.h ortools/graph/graph.h ortools/base/port.h \
 ortools/graph/iterators.h ortools/util/stats.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/base/commandlineflags.h \
 ortools/base/mathutil.h ortools/base/casts.h ortools/graph/graphs.h \
 ortools/graph/max_flow.h ortools/gen/ortools/graph/flow_problem.pb.h | $(OBJ_DIR)/graph
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sgraph$Smin_cost_flow.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Smin_cost_flow.$O

objs/graph/shortestpaths.o: ortools/graph/shortestpaths.cc \
 ortools/graph/shortestpaths.h ortools/base/integral_types.h \
 ortools/base/macros.h ortools/base/commandlineflags.h \
 ortools/base/logging.h | $(OBJ_DIR)/graph
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sgraph$Sshortestpaths.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Sshortestpaths.$O

objs/graph/util.o: ortools/graph/util.cc ortools/graph/util.h \
 ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/map_util.h \
 ortools/graph/connected_components.h ortools/base/ptr_util.h \
 ortools/graph/graph.h ortools/base/port.h ortools/graph/iterators.h | $(OBJ_DIR)/graph
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sgraph$Sutil.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Sutil.$O

ortools/graph/flow_problem.proto: ;

$(GEN_DIR)/ortools/graph/flow_problem.pb.cc: \
 $(SRC_DIR)/ortools/graph/flow_problem.proto | $(GEN_DIR)/ortools/graph
	$(PROTOC) --proto_path=$(INC_DIR) $(PROTOBUF_PROTOC_INC) --cpp_out=$(GEN_PATH) $(SRC_DIR)/ortools/graph/flow_problem.proto

$(GEN_DIR)/ortools/graph/flow_problem.pb.h: \
 $(GEN_DIR)/ortools/graph/flow_problem.pb.cc
	$(TOUCH) $(GEN_PATH)$Sortools$Sgraph$Sflow_problem.pb.h

$(OBJ_DIR)/graph/flow_problem.pb.$O: \
 $(GEN_DIR)/ortools/graph/flow_problem.pb.cc | $(OBJ_DIR)/graph
	$(CCC) $(CFLAGS) -c $(GEN_PATH)$Sortools$Sgraph$Sflow_problem.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Sflow_problem.pb.$O

ALGORITHMS_DEPS = \
 $(SRC_DIR)/ortools/algorithms/dense_doubly_linked_list.h \
 $(SRC_DIR)/ortools/algorithms/dynamic_partition.h \
 $(SRC_DIR)/ortools/algorithms/dynamic_permutation.h \
 $(SRC_DIR)/ortools/algorithms/find_graph_symmetries.h \
 $(SRC_DIR)/ortools/algorithms/hungarian.h \
 $(SRC_DIR)/ortools/algorithms/knapsack_solver.h \
 $(SRC_DIR)/ortools/algorithms/sparse_permutation.h

ALGORITHMS_LIB_OBJS = \
 $(OBJ_DIR)/algorithms/dynamic_partition.$O \
 $(OBJ_DIR)/algorithms/dynamic_permutation.$O \
 $(OBJ_DIR)/algorithms/find_graph_symmetries.$O \
 $(OBJ_DIR)/algorithms/hungarian.$O \
 $(OBJ_DIR)/algorithms/knapsack_solver.$O \
 $(OBJ_DIR)/algorithms/sparse_permutation.$O

objs/algorithms/dynamic_partition.o: \
 ortools/algorithms/dynamic_partition.cc \
 ortools/algorithms/dynamic_partition.h ortools/base/logging.h \
 ortools/base/integral_types.h ortools/base/macros.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/string_view.h \
 ortools/base/murmur.h ortools/base/thorough_hash.h \
 ortools/base/stringprintf.h | $(OBJ_DIR)/algorithms
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Salgorithms$Sdynamic_partition.cc $(OBJ_OUT)$(OBJ_DIR)$Salgorithms$Sdynamic_partition.$O

objs/algorithms/dynamic_permutation.o: \
 ortools/algorithms/dynamic_permutation.cc \
 ortools/algorithms/dynamic_permutation.h ortools/base/logging.h \
 ortools/base/integral_types.h ortools/base/macros.h \
 ortools/algorithms/sparse_permutation.h | $(OBJ_DIR)/algorithms
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Salgorithms$Sdynamic_permutation.cc $(OBJ_OUT)$(OBJ_DIR)$Salgorithms$Sdynamic_permutation.$O

objs/algorithms/find_graph_symmetries.o: \
 ortools/algorithms/find_graph_symmetries.cc \
 ortools/algorithms/find_graph_symmetries.h \
 ortools/algorithms/dynamic_partition.h ortools/base/logging.h \
 ortools/base/integral_types.h ortools/base/macros.h \
 ortools/algorithms/dynamic_permutation.h ortools/base/status.h \
 ortools/base/join.h ortools/base/basictypes.h ortools/base/string_view.h \
 ortools/base/time_support.h ortools/graph/graph.h ortools/base/port.h \
 ortools/graph/iterators.h ortools/util/stats.h ortools/base/timer.h \
 ortools/util/time_limit.h ortools/base/commandlineflags.h \
 ortools/base/memory.h ortools/util/running_stat.h \
 ortools/algorithms/dense_doubly_linked_list.h \
 ortools/algorithms/sparse_permutation.h ortools/base/canonical_errors.h \
 ortools/base/stringprintf.h ortools/graph/util.h ortools/base/hash.h \
 ortools/base/map_util.h ortools/graph/connected_components.h \
 ortools/base/ptr_util.h | $(OBJ_DIR)/algorithms
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Salgorithms$Sfind_graph_symmetries.cc $(OBJ_OUT)$(OBJ_DIR)$Salgorithms$Sfind_graph_symmetries.$O

objs/algorithms/hungarian.o: ortools/algorithms/hungarian.cc \
 ortools/algorithms/hungarian.h | $(OBJ_DIR)/algorithms
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Salgorithms$Shungarian.cc $(OBJ_OUT)$(OBJ_DIR)$Salgorithms$Shungarian.$O

objs/algorithms/knapsack_solver.o: ortools/algorithms/knapsack_solver.cc \
 ortools/algorithms/knapsack_solver.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/memory.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/port.h \
 ortools/base/time_support.h ortools/base/timer.h \
 ortools/util/running_stat.h ortools/base/stl_util.h \
 ortools/linear_solver/linear_solver.h ortools/base/optional.h \
 ortools/base/status.h ortools/base/join.h ortools/base/string_view.h \
 ortools/base/stringprintf.h ortools/base/strutil.h \
 ortools/gen/ortools/glop/parameters.pb.h \
 ortools/linear_solver/linear_expr.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h \
 ortools/port/proto_utils.h ortools/util/bitset.h | $(OBJ_DIR)/algorithms
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Salgorithms$Sknapsack_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Salgorithms$Sknapsack_solver.$O

objs/algorithms/sparse_permutation.o: \
 ortools/algorithms/sparse_permutation.cc \
 ortools/algorithms/sparse_permutation.h ortools/base/logging.h \
 ortools/base/integral_types.h ortools/base/macros.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/string_view.h | $(OBJ_DIR)/algorithms
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Salgorithms$Ssparse_permutation.cc $(OBJ_OUT)$(OBJ_DIR)$Salgorithms$Ssparse_permutation.$O

SAT_DEPS = \
 $(SRC_DIR)/ortools/sat/all_different.h \
 $(SRC_DIR)/ortools/sat/boolean_problem.h \
 $(SRC_DIR)/ortools/sat/circuit.h \
 $(SRC_DIR)/ortools/sat/clause.h \
 $(SRC_DIR)/ortools/sat/cp_constraints.h \
 $(SRC_DIR)/ortools/sat/cp_model_checker.h \
 $(SRC_DIR)/ortools/sat/cp_model_expand.h \
 $(SRC_DIR)/ortools/sat/cp_model_lns.h \
 $(SRC_DIR)/ortools/sat/cp_model_objective.h \
 $(SRC_DIR)/ortools/sat/cp_model_presolve.h \
 $(SRC_DIR)/ortools/sat/cp_model_search.h \
 $(SRC_DIR)/ortools/sat/cp_model_solver.h \
 $(SRC_DIR)/ortools/sat/cp_model_symmetries.h \
 $(SRC_DIR)/ortools/sat/cp_model_utils.h \
 $(SRC_DIR)/ortools/sat/cumulative.h \
 $(SRC_DIR)/ortools/sat/disjunctive.h \
 $(SRC_DIR)/ortools/sat/drat_checker.h \
 $(SRC_DIR)/ortools/sat/drat_proof_handler.h \
 $(SRC_DIR)/ortools/sat/drat_writer.h \
 $(SRC_DIR)/ortools/sat/encoding.h \
 $(SRC_DIR)/ortools/sat/integer_expr.h \
 $(SRC_DIR)/ortools/sat/integer.h \
 $(SRC_DIR)/ortools/sat/integer_search.h \
 $(SRC_DIR)/ortools/sat/intervals.h \
 $(SRC_DIR)/ortools/sat/linear_programming_constraint.h \
 $(SRC_DIR)/ortools/sat/linear_relaxation.h \
 $(SRC_DIR)/ortools/sat/lns.h \
 $(SRC_DIR)/ortools/sat/lp_utils.h \
 $(SRC_DIR)/ortools/sat/model.h \
 $(SRC_DIR)/ortools/sat/optimization.h \
 $(SRC_DIR)/ortools/sat/overload_checker.h \
 $(SRC_DIR)/ortools/sat/pb_constraint.h \
 $(SRC_DIR)/ortools/sat/precedences.h \
 $(SRC_DIR)/ortools/sat/restart.h \
 $(SRC_DIR)/ortools/sat/sat_base.h \
 $(SRC_DIR)/ortools/sat/sat_decision.h \
 $(SRC_DIR)/ortools/sat/sat_solver.h \
 $(SRC_DIR)/ortools/sat/simplification.h \
 $(SRC_DIR)/ortools/sat/swig_helper.h \
 $(SRC_DIR)/ortools/sat/symmetry.h \
 $(SRC_DIR)/ortools/sat/table.h \
 $(SRC_DIR)/ortools/sat/theta_tree.h \
 $(SRC_DIR)/ortools/sat/timetable_edgefinding.h \
 $(SRC_DIR)/ortools/sat/timetable.h \
 $(SRC_DIR)/ortools/sat/util.h \
 $(GEN_DIR)/ortools/sat/boolean_problem.pb.h \
 $(GEN_DIR)/ortools/sat/cp_model.pb.h \
 $(GEN_DIR)/ortools/sat/sat_parameters.pb.h

SAT_LIB_OBJS = \
 $(OBJ_DIR)/sat/all_different.$O \
 $(OBJ_DIR)/sat/boolean_problem.$O \
 $(OBJ_DIR)/sat/circuit.$O \
 $(OBJ_DIR)/sat/clause.$O \
 $(OBJ_DIR)/sat/cp_constraints.$O \
 $(OBJ_DIR)/sat/cp_model_checker.$O \
 $(OBJ_DIR)/sat/cp_model_expand.$O \
 $(OBJ_DIR)/sat/cp_model_lns.$O \
 $(OBJ_DIR)/sat/cp_model_objective.$O \
 $(OBJ_DIR)/sat/cp_model_presolve.$O \
 $(OBJ_DIR)/sat/cp_model_search.$O \
 $(OBJ_DIR)/sat/cp_model_solver.$O \
 $(OBJ_DIR)/sat/cp_model_symmetries.$O \
 $(OBJ_DIR)/sat/cp_model_utils.$O \
 $(OBJ_DIR)/sat/cumulative.$O \
 $(OBJ_DIR)/sat/disjunctive.$O \
 $(OBJ_DIR)/sat/drat_checker.$O \
 $(OBJ_DIR)/sat/drat_proof_handler.$O \
 $(OBJ_DIR)/sat/drat_writer.$O \
 $(OBJ_DIR)/sat/encoding.$O \
 $(OBJ_DIR)/sat/integer.$O \
 $(OBJ_DIR)/sat/integer_expr.$O \
 $(OBJ_DIR)/sat/integer_search.$O \
 $(OBJ_DIR)/sat/intervals.$O \
 $(OBJ_DIR)/sat/linear_programming_constraint.$O \
 $(OBJ_DIR)/sat/linear_relaxation.$O \
 $(OBJ_DIR)/sat/lp_utils.$O \
 $(OBJ_DIR)/sat/optimization.$O \
 $(OBJ_DIR)/sat/overload_checker.$O \
 $(OBJ_DIR)/sat/pb_constraint.$O \
 $(OBJ_DIR)/sat/precedences.$O \
 $(OBJ_DIR)/sat/restart.$O \
 $(OBJ_DIR)/sat/sat_decision.$O \
 $(OBJ_DIR)/sat/sat_solver.$O \
 $(OBJ_DIR)/sat/simplification.$O \
 $(OBJ_DIR)/sat/symmetry.$O \
 $(OBJ_DIR)/sat/table.$O \
 $(OBJ_DIR)/sat/theta_tree.$O \
 $(OBJ_DIR)/sat/timetable.$O \
 $(OBJ_DIR)/sat/timetable_edgefinding.$O \
 $(OBJ_DIR)/sat/util.$O \
 $(OBJ_DIR)/sat/boolean_problem.pb.$O \
 $(OBJ_DIR)/sat/cp_model.pb.$O \
 $(OBJ_DIR)/sat/sat_parameters.pb.$O

objs/sat/all_different.o: ortools/sat/all_different.cc \
 ortools/sat/all_different.h ortools/base/integral_types.h \
 ortools/base/macros.h ortools/sat/integer.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/logging.h \
 ortools/base/inlined_vector.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/base/join.h \
 ortools/base/string_view.h ortools/base/map_util.h ortools/base/port.h \
 ortools/base/span.h ortools/graph/iterators.h ortools/sat/model.h \
 ortools/base/typeid.h ortools/sat/sat_base.h ortools/base/stringprintf.h \
 ortools/util/bitset.h ortools/sat/sat_solver.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h \
 ortools/util/stats.h ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h \
 ortools/graph/strongly_connected_components.h ortools/util/sort.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Sall_different.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sall_different.$O

objs/sat/boolean_problem.o: ortools/sat/boolean_problem.cc \
 ortools/sat/boolean_problem.h ortools/algorithms/sparse_permutation.h \
 ortools/base/logging.h ortools/base/integral_types.h \
 ortools/base/macros.h ortools/base/int_type_indexed_vector.h \
 ortools/base/int_type.h ortools/base/status.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/string_view.h \
 ortools/gen/ortools/sat/boolean_problem.pb.h \
 ortools/gen/ortools/sat/cp_model.pb.h ortools/sat/pb_constraint.h \
 ortools/base/hash.h ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/sat/sat_base.h ortools/base/port.h ortools/base/stringprintf.h \
 ortools/sat/model.h ortools/base/map_util.h ortools/base/typeid.h \
 ortools/util/bitset.h ortools/gen/ortools/sat/sat_parameters.pb.h \
 ortools/util/stats.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/sat/sat_solver.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h \
 ortools/util/random_engine.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/sat/simplification.h ortools/base/adjustable_priority_queue.h \
 ortools/graph/io.h ortools/base/filelineiter.h \
 ortools/base/stringpiece_utils.h ortools/base/strutil.h \
 ortools/base/numbers.h ortools/base/split.h ortools/base/statusor.h \
 ortools/graph/graph.h ortools/graph/iterators.h \
 ortools/algorithms/find_graph_symmetries.h \
 ortools/algorithms/dynamic_partition.h \
 ortools/algorithms/dynamic_permutation.h ortools/graph/util.h \
 ortools/graph/connected_components.h ortools/base/ptr_util.h \
 ortools/port/proto_utils.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Sboolean_problem.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sboolean_problem.$O

objs/sat/circuit.o: ortools/sat/circuit.cc ortools/sat/circuit.h \
 ortools/base/int_type.h ortools/base/macros.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/sat/integer.h ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/inlined_vector.h ortools/base/int_type_indexed_vector.h \
 ortools/base/join.h ortools/base/string_view.h ortools/base/map_util.h \
 ortools/base/port.h ortools/base/span.h ortools/graph/iterators.h \
 ortools/sat/model.h ortools/base/typeid.h ortools/sat/sat_base.h \
 ortools/base/stringprintf.h ortools/util/bitset.h \
 ortools/sat/sat_solver.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h \
 ortools/util/stats.h ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Scircuit.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Scircuit.$O

objs/sat/clause.o: ortools/sat/clause.cc ortools/sat/clause.h \
 ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/inlined_vector.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/base/span.h ortools/sat/drat_proof_handler.h \
 ortools/sat/drat_checker.h ortools/sat/sat_base.h ortools/base/port.h \
 ortools/base/stringprintf.h ortools/sat/model.h ortools/base/map_util.h \
 ortools/base/typeid.h ortools/util/bitset.h ortools/sat/drat_writer.h \
 ortools/base/file.h ortools/base/status.h ortools/base/join.h \
 ortools/base/string_view.h ortools/gen/ortools/sat/sat_parameters.pb.h \
 ortools/util/random_engine.h ortools/util/stats.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/base/stl_util.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Sclause.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sclause.$O

objs/sat/cp_constraints.o: ortools/sat/cp_constraints.cc \
 ortools/sat/cp_constraints.h ortools/base/int_type.h \
 ortools/base/macros.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/sat/integer.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/inlined_vector.h \
 ortools/base/int_type_indexed_vector.h ortools/base/join.h \
 ortools/base/string_view.h ortools/base/map_util.h ortools/base/port.h \
 ortools/base/span.h ortools/graph/iterators.h ortools/sat/model.h \
 ortools/base/typeid.h ortools/sat/sat_base.h ortools/base/stringprintf.h \
 ortools/util/bitset.h ortools/sat/sat_solver.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h \
 ortools/util/stats.h ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h ortools/util/sort.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Scp_constraints.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Scp_constraints.$O

objs/sat/cp_model_checker.o: ortools/sat/cp_model_checker.cc \
 ortools/sat/cp_model_checker.h ortools/base/integral_types.h \
 ortools/gen/ortools/sat/cp_model.pb.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/logging.h ortools/base/macros.h \
 ortools/base/join.h ortools/base/string_view.h ortools/base/map_util.h \
 ortools/port/proto_utils.h ortools/base/port.h \
 ortools/sat/cp_model_utils.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Scp_model_checker.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Scp_model_checker.$O

objs/sat/cp_model_expand.o: ortools/sat/cp_model_expand.cc \
 ortools/sat/cp_model_expand.h ortools/gen/ortools/sat/cp_model.pb.h \
 ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/map_util.h \
 ortools/sat/cp_model_utils.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Scp_model_expand.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Scp_model_expand.$O

objs/sat/cp_model_lns.o: ortools/sat/cp_model_lns.cc \
 ortools/sat/cp_model_lns.h ortools/base/integral_types.h \
 ortools/gen/ortools/sat/cp_model.pb.h ortools/sat/cp_model_utils.h \
 ortools/base/logging.h ortools/base/macros.h \
 ortools/util/sorted_interval_list.h ortools/base/span.h \
 ortools/base/inlined_vector.h ortools/util/random_engine.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Scp_model_lns.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Scp_model_lns.$O

objs/sat/cp_model_objective.o: ortools/sat/cp_model_objective.cc \
 ortools/sat/cp_model_objective.h ortools/gen/ortools/sat/cp_model.pb.h \
 ortools/sat/cp_model_utils.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h \
 ortools/util/sorted_interval_list.h ortools/base/span.h \
 ortools/base/inlined_vector.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Scp_model_objective.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Scp_model_objective.$O

objs/sat/cp_model_presolve.o: ortools/sat/cp_model_presolve.cc \
 ortools/sat/cp_model_presolve.h ortools/gen/ortools/sat/cp_model.pb.h \
 ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/join.h ortools/base/string_view.h \
 ortools/base/map_util.h ortools/base/port.h ortools/base/stl_util.h \
 ortools/sat/cp_model_checker.h ortools/sat/cp_model_objective.h \
 ortools/sat/cp_model_utils.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h ortools/sat/sat_base.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/base/stringprintf.h ortools/sat/model.h ortools/base/typeid.h \
 ortools/util/bitset.h ortools/gen/ortools/sat/sat_parameters.pb.h \
 ortools/sat/simplification.h ortools/base/adjustable_priority_queue.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/sat/sat_solver.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/util/random_engine.h ortools/util/stats.h \
 ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/util/affine_relation.h ortools/base/iterator_adaptors.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Scp_model_presolve.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Scp_model_presolve.$O

objs/sat/cp_model_search.o: ortools/sat/cp_model_search.cc \
 ortools/sat/cp_model_search.h ortools/base/integral_types.h \
 ortools/gen/ortools/sat/cp_model.pb.h ortools/sat/integer.h \
 ortools/base/hash.h ortools/base/basictypes.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/inlined_vector.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/base/join.h ortools/base/string_view.h ortools/base/map_util.h \
 ortools/base/port.h ortools/base/span.h ortools/graph/iterators.h \
 ortools/sat/model.h ortools/base/typeid.h ortools/sat/sat_base.h \
 ortools/base/stringprintf.h ortools/util/bitset.h \
 ortools/sat/sat_solver.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h \
 ortools/util/stats.h ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h ortools/sat/integer_search.h \
 ortools/sat/cp_model_utils.h ortools/sat/util.h ortools/base/random.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Scp_model_search.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Scp_model_search.$O

objs/sat/cp_model_solver.o: ortools/sat/cp_model_solver.cc \
 ortools/sat/cp_model_solver.h ortools/base/integral_types.h \
 ortools/gen/ortools/sat/cp_model.pb.h ortools/sat/model.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/map_util.h \
 ortools/base/typeid.h ortools/gen/ortools/sat/sat_parameters.pb.h \
 ortools/base/commandlineflags.h ortools/base/mutex.h \
 ortools/base/stringprintf.h ortools/base/timer.h \
 ortools/base/basictypes.h ortools/base/time_support.h \
 ortools/base/notification.h ortools/base/cleanup.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/base/iterator_adaptors.h ortools/base/join.h \
 ortools/base/string_view.h ortools/base/memory.h ortools/base/stl_util.h \
 ortools/graph/connectivity.h ortools/port/proto_utils.h \
 ortools/base/port.h ortools/sat/all_different.h ortools/sat/integer.h \
 ortools/base/hash.h ortools/base/inlined_vector.h ortools/base/span.h \
 ortools/graph/iterators.h ortools/sat/sat_base.h ortools/util/bitset.h \
 ortools/sat/sat_solver.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/util/random_engine.h ortools/util/stats.h \
 ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h ortools/sat/circuit.h \
 ortools/sat/cp_constraints.h ortools/sat/cp_model_checker.h \
 ortools/sat/cp_model_expand.h ortools/sat/cp_model_lns.h \
 ortools/sat/cp_model_presolve.h ortools/sat/cp_model_search.h \
 ortools/sat/integer_search.h ortools/sat/cp_model_utils.h \
 ortools/sat/cumulative.h ortools/sat/intervals.h \
 ortools/sat/integer_expr.h ortools/sat/precedences.h \
 ortools/sat/disjunctive.h ortools/sat/theta_tree.h \
 ortools/sat/linear_programming_constraint.h \
 ortools/glop/revised_simplex.h ortools/glop/basis_representation.h \
 ortools/glop/lu_factorization.h ortools/glop/markowitz.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/glop/status.h \
 ortools/lp_data/lp_types.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/glop/rank_one_update.h \
 ortools/lp_data/lp_utils.h ortools/base/accurate_sum.h \
 ortools/glop/dual_edge_norms.h ortools/lp_data/lp_data.h \
 ortools/util/fp_utils.h ortools/glop/entering_variable.h \
 ortools/glop/primal_edge_norms.h ortools/glop/update_row.h \
 ortools/glop/variables_info.h ortools/glop/reduced_costs.h \
 ortools/glop/variable_values.h ortools/lp_data/lp_print_utils.h \
 ortools/lp_data/sparse_row.h ortools/lp_data/matrix_scaler.h \
 ortools/sat/linear_relaxation.h ortools/sat/lns.h \
 ortools/base/threadpool.h ortools/sat/optimization.h \
 ortools/gen/ortools/sat/boolean_problem.pb.h \
 ortools/sat/simplification.h ortools/base/adjustable_priority_queue.h \
 ortools/sat/table.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Scp_model_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Scp_model_solver.$O

objs/sat/cp_model_symmetries.o: ortools/sat/cp_model_symmetries.cc \
 ortools/sat/cp_model_symmetries.h \
 ortools/algorithms/sparse_permutation.h ortools/base/logging.h \
 ortools/base/integral_types.h ortools/base/macros.h \
 ortools/gen/ortools/sat/cp_model.pb.h \
 ortools/algorithms/find_graph_symmetries.h \
 ortools/algorithms/dynamic_partition.h \
 ortools/algorithms/dynamic_permutation.h ortools/base/status.h \
 ortools/base/join.h ortools/base/basictypes.h ortools/base/string_view.h \
 ortools/base/time_support.h ortools/graph/graph.h ortools/base/port.h \
 ortools/graph/iterators.h ortools/util/stats.h ortools/base/timer.h \
 ortools/util/time_limit.h ortools/base/commandlineflags.h \
 ortools/base/memory.h ortools/util/running_stat.h ortools/base/hash.h \
 ortools/base/map_util.h ortools/sat/cp_model_utils.h \
 ortools/util/sorted_interval_list.h ortools/base/span.h \
 ortools/base/inlined_vector.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Scp_model_symmetries.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Scp_model_symmetries.$O

objs/sat/cp_model_utils.o: ortools/sat/cp_model_utils.cc \
 ortools/sat/cp_model_utils.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h \
 ortools/gen/ortools/sat/cp_model.pb.h \
 ortools/util/sorted_interval_list.h ortools/base/span.h \
 ortools/base/inlined_vector.h ortools/base/stl_util.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Scp_model_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Scp_model_utils.$O

objs/sat/cumulative.o: ortools/sat/cumulative.cc ortools/sat/cumulative.h \
 ortools/sat/integer.h ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/inlined_vector.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/base/join.h ortools/base/string_view.h ortools/base/map_util.h \
 ortools/base/port.h ortools/base/span.h ortools/graph/iterators.h \
 ortools/sat/model.h ortools/base/typeid.h ortools/sat/sat_base.h \
 ortools/base/stringprintf.h ortools/util/bitset.h \
 ortools/sat/sat_solver.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h \
 ortools/util/stats.h ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h ortools/sat/intervals.h \
 ortools/sat/cp_constraints.h ortools/sat/integer_expr.h \
 ortools/sat/precedences.h ortools/sat/disjunctive.h \
 ortools/sat/theta_tree.h ortools/sat/overload_checker.h \
 ortools/sat/timetable.h ortools/sat/timetable_edgefinding.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Scumulative.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Scumulative.$O

objs/sat/disjunctive.o: ortools/sat/disjunctive.cc \
 ortools/sat/disjunctive.h ortools/base/int_type.h ortools/base/macros.h \
 ortools/sat/integer.h ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/inlined_vector.h ortools/base/int_type_indexed_vector.h \
 ortools/base/join.h ortools/base/string_view.h ortools/base/map_util.h \
 ortools/base/port.h ortools/base/span.h ortools/graph/iterators.h \
 ortools/sat/model.h ortools/base/typeid.h ortools/sat/sat_base.h \
 ortools/base/stringprintf.h ortools/util/bitset.h \
 ortools/sat/sat_solver.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h \
 ortools/util/stats.h ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h ortools/sat/intervals.h \
 ortools/sat/cp_constraints.h ortools/sat/integer_expr.h \
 ortools/sat/precedences.h ortools/sat/theta_tree.h \
 ortools/base/iterator_adaptors.h ortools/sat/all_different.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Sdisjunctive.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sdisjunctive.$O

objs/sat/drat_checker.o: ortools/sat/drat_checker.cc \
 ortools/sat/drat_checker.h ortools/base/int_type.h ortools/base/macros.h \
 ortools/base/int_type_indexed_vector.h ortools/base/span.h \
 ortools/base/inlined_vector.h ortools/base/logging.h \
 ortools/base/integral_types.h ortools/sat/sat_base.h ortools/base/port.h \
 ortools/base/stringprintf.h ortools/sat/model.h ortools/base/map_util.h \
 ortools/base/typeid.h ortools/util/bitset.h ortools/base/basictypes.h \
 ortools/base/hash.h ortools/base/numbers.h ortools/base/join.h \
 ortools/base/string_view.h ortools/base/split.h ortools/base/stl_util.h \
 ortools/base/time_support.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/base/timer.h ortools/util/running_stat.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Sdrat_checker.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sdrat_checker.$O

objs/sat/drat_proof_handler.o: ortools/sat/drat_proof_handler.cc \
 ortools/sat/drat_proof_handler.h ortools/base/int_type_indexed_vector.h \
 ortools/base/int_type.h ortools/base/macros.h ortools/base/span.h \
 ortools/base/inlined_vector.h ortools/base/logging.h \
 ortools/base/integral_types.h ortools/sat/drat_checker.h \
 ortools/sat/sat_base.h ortools/base/port.h ortools/base/stringprintf.h \
 ortools/sat/model.h ortools/base/map_util.h ortools/base/typeid.h \
 ortools/util/bitset.h ortools/base/basictypes.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/base/join.h ortools/base/string_view.h ortools/base/memory.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Sdrat_proof_handler.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sdrat_proof_handler.$O

objs/sat/drat_writer.o: ortools/sat/drat_writer.cc \
 ortools/sat/drat_writer.h ortools/base/file.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/status.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/string_view.h ortools/base/span.h \
 ortools/base/inlined_vector.h ortools/sat/sat_base.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/base/port.h ortools/base/stringprintf.h ortools/sat/model.h \
 ortools/base/map_util.h ortools/base/typeid.h ortools/util/bitset.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Sdrat_writer.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sdrat_writer.$O

objs/sat/encoding.o: ortools/sat/encoding.cc ortools/sat/encoding.h \
 ortools/base/int_type.h ortools/base/macros.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/gen/ortools/sat/boolean_problem.pb.h ortools/sat/pb_constraint.h \
 ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/int_type_indexed_vector.h ortools/base/span.h \
 ortools/base/inlined_vector.h ortools/sat/sat_base.h ortools/base/port.h \
 ortools/base/stringprintf.h ortools/sat/model.h ortools/base/map_util.h \
 ortools/base/typeid.h ortools/util/bitset.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/stats.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/sat/sat_solver.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/util/random_engine.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Sencoding.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sencoding.$O

objs/sat/integer.o: ortools/sat/integer.cc ortools/sat/integer.h \
 ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/inlined_vector.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/base/join.h ortools/base/string_view.h ortools/base/map_util.h \
 ortools/base/port.h ortools/base/span.h ortools/graph/iterators.h \
 ortools/sat/model.h ortools/base/typeid.h ortools/sat/sat_base.h \
 ortools/base/stringprintf.h ortools/util/bitset.h \
 ortools/sat/sat_solver.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h \
 ortools/util/stats.h ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h ortools/base/iterator_adaptors.h \
 ortools/base/stl_util.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Sinteger.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sinteger.$O

objs/sat/integer_expr.o: ortools/sat/integer_expr.cc \
 ortools/sat/integer_expr.h ortools/base/int_type.h ortools/base/macros.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/sat/integer.h ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/inlined_vector.h ortools/base/int_type_indexed_vector.h \
 ortools/base/join.h ortools/base/string_view.h ortools/base/map_util.h \
 ortools/base/port.h ortools/base/span.h ortools/graph/iterators.h \
 ortools/sat/model.h ortools/base/typeid.h ortools/sat/sat_base.h \
 ortools/base/stringprintf.h ortools/util/bitset.h \
 ortools/sat/sat_solver.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h \
 ortools/util/stats.h ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h ortools/sat/precedences.h \
 ortools/base/stl_util.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Sinteger_expr.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sinteger_expr.$O

objs/sat/integer_search.o: ortools/sat/integer_search.cc \
 ortools/sat/integer_search.h ortools/sat/integer.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h \
 ortools/base/inlined_vector.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/base/join.h \
 ortools/base/string_view.h ortools/base/map_util.h ortools/base/port.h \
 ortools/base/span.h ortools/graph/iterators.h ortools/sat/model.h \
 ortools/base/typeid.h ortools/sat/sat_base.h ortools/base/stringprintf.h \
 ortools/util/bitset.h ortools/sat/sat_solver.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h \
 ortools/util/stats.h ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h \
 ortools/sat/linear_programming_constraint.h \
 ortools/glop/revised_simplex.h ortools/glop/basis_representation.h \
 ortools/glop/lu_factorization.h ortools/glop/markowitz.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/glop/status.h \
 ortools/lp_data/lp_types.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/glop/rank_one_update.h \
 ortools/lp_data/lp_utils.h ortools/base/accurate_sum.h \
 ortools/glop/dual_edge_norms.h ortools/lp_data/lp_data.h \
 ortools/util/fp_utils.h ortools/glop/entering_variable.h \
 ortools/glop/primal_edge_norms.h ortools/glop/update_row.h \
 ortools/glop/variables_info.h ortools/glop/reduced_costs.h \
 ortools/glop/variable_values.h ortools/lp_data/lp_print_utils.h \
 ortools/lp_data/sparse_row.h ortools/lp_data/matrix_scaler.h \
 ortools/sat/util.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Sinteger_search.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sinteger_search.$O

objs/sat/intervals.o: ortools/sat/intervals.cc ortools/sat/intervals.h \
 ortools/base/int_type.h ortools/base/macros.h \
 ortools/base/int_type_indexed_vector.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/sat/cp_constraints.h \
 ortools/sat/integer.h ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/inlined_vector.h ortools/base/join.h \
 ortools/base/string_view.h ortools/base/map_util.h ortools/base/port.h \
 ortools/base/span.h ortools/graph/iterators.h ortools/sat/model.h \
 ortools/base/typeid.h ortools/sat/sat_base.h ortools/base/stringprintf.h \
 ortools/util/bitset.h ortools/sat/sat_solver.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h \
 ortools/util/stats.h ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h ortools/sat/integer_expr.h \
 ortools/sat/precedences.h ortools/util/sort.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Sintervals.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sintervals.$O

objs/sat/linear_programming_constraint.o: \
 ortools/sat/linear_programming_constraint.cc \
 ortools/sat/linear_programming_constraint.h ortools/base/int_type.h \
 ortools/base/macros.h ortools/glop/revised_simplex.h \
 ortools/base/integral_types.h ortools/glop/basis_representation.h \
 ortools/base/logging.h ortools/glop/lu_factorization.h \
 ortools/glop/markowitz.h ortools/base/inlined_vector.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/glop/status.h \
 ortools/base/port.h ortools/lp_data/lp_types.h ortools/base/basictypes.h \
 ortools/base/int_type_indexed_vector.h ortools/util/bitset.h \
 ortools/lp_data/sparse.h ortools/lp_data/permutation.h \
 ortools/base/random.h ortools/util/return_macros.h \
 ortools/lp_data/sparse_column.h ortools/lp_data/sparse_vector.h \
 ortools/base/stringprintf.h ortools/graph/iterators.h \
 ortools/util/stats.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/glop/rank_one_update.h ortools/lp_data/lp_utils.h \
 ortools/base/accurate_sum.h ortools/glop/dual_edge_norms.h \
 ortools/lp_data/lp_data.h ortools/base/hash.h ortools/util/fp_utils.h \
 ortools/glop/entering_variable.h ortools/glop/primal_edge_norms.h \
 ortools/glop/update_row.h ortools/glop/variables_info.h \
 ortools/glop/reduced_costs.h ortools/util/random_engine.h \
 ortools/glop/variable_values.h ortools/lp_data/lp_print_utils.h \
 ortools/lp_data/sparse_row.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/util/running_stat.h ortools/lp_data/matrix_scaler.h \
 ortools/sat/integer.h ortools/base/join.h ortools/base/string_view.h \
 ortools/base/map_util.h ortools/base/span.h ortools/sat/model.h \
 ortools/base/typeid.h ortools/sat/sat_base.h ortools/sat/sat_solver.h \
 ortools/sat/clause.h ortools/sat/drat_proof_handler.h \
 ortools/sat/drat_checker.h ortools/sat/drat_writer.h ortools/base/file.h \
 ortools/base/status.h ortools/gen/ortools/sat/sat_parameters.pb.h \
 ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/sat/sat_decision.h ortools/util/integer_pq.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h \
 ortools/graph/strongly_connected_components.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Slinear_programming_constraint.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Slinear_programming_constraint.$O

objs/sat/linear_relaxation.o: ortools/sat/linear_relaxation.cc \
 ortools/sat/linear_relaxation.h ortools/sat/integer.h \
 ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/inlined_vector.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/base/join.h ortools/base/string_view.h ortools/base/map_util.h \
 ortools/base/port.h ortools/base/span.h ortools/graph/iterators.h \
 ortools/sat/model.h ortools/base/typeid.h ortools/sat/sat_base.h \
 ortools/base/stringprintf.h ortools/util/bitset.h \
 ortools/sat/sat_solver.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h \
 ortools/util/stats.h ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h \
 ortools/sat/linear_programming_constraint.h \
 ortools/glop/revised_simplex.h ortools/glop/basis_representation.h \
 ortools/glop/lu_factorization.h ortools/glop/markowitz.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/glop/status.h \
 ortools/lp_data/lp_types.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/glop/rank_one_update.h \
 ortools/lp_data/lp_utils.h ortools/base/accurate_sum.h \
 ortools/glop/dual_edge_norms.h ortools/lp_data/lp_data.h \
 ortools/util/fp_utils.h ortools/glop/entering_variable.h \
 ortools/glop/primal_edge_norms.h ortools/glop/update_row.h \
 ortools/glop/variables_info.h ortools/glop/reduced_costs.h \
 ortools/glop/variable_values.h ortools/lp_data/lp_print_utils.h \
 ortools/lp_data/sparse_row.h ortools/lp_data/matrix_scaler.h \
 ortools/base/iterator_adaptors.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Slinear_relaxation.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Slinear_relaxation.$O

objs/sat/lp_utils.o: ortools/sat/lp_utils.cc ortools/sat/lp_utils.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h ortools/lp_data/lp_data.h \
 ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/lp_data/lp_types.h \
 ortools/util/bitset.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/base/stringprintf.h \
 ortools/graph/iterators.h ortools/util/fp_utils.h \
 ortools/gen/ortools/sat/boolean_problem.pb.h \
 ortools/gen/ortools/sat/cp_model.pb.h ortools/sat/sat_solver.h \
 ortools/base/span.h ortools/base/inlined_vector.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/sat_base.h ortools/base/port.h ortools/sat/model.h \
 ortools/base/map_util.h ortools/base/typeid.h ortools/sat/drat_writer.h \
 ortools/base/file.h ortools/base/status.h ortools/base/join.h \
 ortools/base/string_view.h ortools/gen/ortools/sat/sat_parameters.pb.h \
 ortools/util/random_engine.h ortools/util/stats.h \
 ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/glop/lp_solver.h ortools/glop/preprocessor.h \
 ortools/glop/revised_simplex.h ortools/glop/basis_representation.h \
 ortools/glop/lu_factorization.h ortools/glop/markowitz.h \
 ortools/glop/status.h ortools/glop/rank_one_update.h \
 ortools/lp_data/lp_utils.h ortools/base/accurate_sum.h \
 ortools/glop/dual_edge_norms.h ortools/glop/entering_variable.h \
 ortools/glop/primal_edge_norms.h ortools/glop/update_row.h \
 ortools/glop/variables_info.h ortools/glop/reduced_costs.h \
 ortools/glop/variable_values.h ortools/lp_data/lp_print_utils.h \
 ortools/lp_data/sparse_row.h ortools/lp_data/matrix_scaler.h \
 ortools/sat/boolean_problem.h ortools/algorithms/sparse_permutation.h \
 ortools/sat/simplification.h ortools/base/adjustable_priority_queue.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Slp_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Slp_utils.$O

objs/sat/optimization.o: ortools/sat/optimization.cc \
 ortools/sat/optimization.h ortools/gen/ortools/sat/boolean_problem.pb.h \
 ortools/sat/integer.h ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/inlined_vector.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/base/join.h ortools/base/string_view.h ortools/base/map_util.h \
 ortools/base/port.h ortools/base/span.h ortools/graph/iterators.h \
 ortools/sat/model.h ortools/base/typeid.h ortools/sat/sat_base.h \
 ortools/base/stringprintf.h ortools/util/bitset.h \
 ortools/sat/sat_solver.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h \
 ortools/util/stats.h ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h ortools/sat/integer_search.h \
 ortools/linear_solver/linear_solver.h ortools/base/optional.h \
 ortools/base/strutil.h ortools/gen/ortools/glop/parameters.pb.h \
 ortools/linear_solver/linear_expr.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h \
 ortools/port/proto_utils.h ortools/base/random.h \
 ortools/sat/boolean_problem.h ortools/algorithms/sparse_permutation.h \
 ortools/gen/ortools/sat/cp_model.pb.h ortools/sat/simplification.h \
 ortools/base/adjustable_priority_queue.h ortools/sat/encoding.h \
 ortools/sat/integer_expr.h ortools/sat/precedences.h ortools/sat/util.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Soptimization.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Soptimization.$O

objs/sat/overload_checker.o: ortools/sat/overload_checker.cc \
 ortools/sat/overload_checker.h ortools/base/int_type.h \
 ortools/base/macros.h ortools/sat/integer.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/inlined_vector.h \
 ortools/base/int_type_indexed_vector.h ortools/base/join.h \
 ortools/base/string_view.h ortools/base/map_util.h ortools/base/port.h \
 ortools/base/span.h ortools/graph/iterators.h ortools/sat/model.h \
 ortools/base/typeid.h ortools/sat/sat_base.h ortools/base/stringprintf.h \
 ortools/util/bitset.h ortools/sat/sat_solver.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h \
 ortools/util/stats.h ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h ortools/sat/intervals.h \
 ortools/sat/cp_constraints.h ortools/sat/integer_expr.h \
 ortools/sat/precedences.h ortools/util/sort.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Soverload_checker.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Soverload_checker.$O

objs/sat/pb_constraint.o: ortools/sat/pb_constraint.cc \
 ortools/sat/pb_constraint.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/base/span.h \
 ortools/base/inlined_vector.h ortools/sat/sat_base.h ortools/base/port.h \
 ortools/base/stringprintf.h ortools/sat/model.h ortools/base/map_util.h \
 ortools/base/typeid.h ortools/util/bitset.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/stats.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/base/thorough_hash.h ortools/util/saturated_arithmetic.h \
 ortools/base/casts.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Spb_constraint.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Spb_constraint.$O

objs/sat/precedences.o: ortools/sat/precedences.cc \
 ortools/sat/precedences.h ortools/base/inlined_vector.h \
 ortools/base/logging.h ortools/base/integral_types.h \
 ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/sat/integer.h \
 ortools/base/hash.h ortools/base/basictypes.h ortools/base/join.h \
 ortools/base/string_view.h ortools/base/map_util.h ortools/base/port.h \
 ortools/base/span.h ortools/graph/iterators.h ortools/sat/model.h \
 ortools/base/typeid.h ortools/sat/sat_base.h ortools/base/stringprintf.h \
 ortools/util/bitset.h ortools/sat/sat_solver.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h \
 ortools/util/stats.h ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h ortools/base/cleanup.h \
 ortools/base/stl_util.h ortools/sat/cp_constraints.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Sprecedences.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sprecedences.$O

objs/sat/restart.o: ortools/sat/restart.cc ortools/sat/restart.h \
 ortools/sat/model.h ortools/base/logging.h ortools/base/integral_types.h \
 ortools/base/macros.h ortools/base/map_util.h ortools/base/typeid.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/bitset.h \
 ortools/base/basictypes.h ortools/util/running_stat.h \
 ortools/base/stringprintf.h ortools/base/split.h \
 ortools/base/string_view.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Srestart.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Srestart.$O

objs/sat/sat_decision.o: ortools/sat/sat_decision.cc \
 ortools/sat/sat_decision.h ortools/base/integral_types.h \
 ortools/sat/model.h ortools/base/logging.h ortools/base/macros.h \
 ortools/base/map_util.h ortools/base/typeid.h \
 ortools/sat/pb_constraint.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/base/span.h \
 ortools/base/inlined_vector.h ortools/sat/sat_base.h ortools/base/port.h \
 ortools/base/stringprintf.h ortools/util/bitset.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/stats.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/util/integer_pq.h ortools/util/random_engine.h \
 ortools/sat/util.h ortools/base/random.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Ssat_decision.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Ssat_decision.$O

objs/sat/sat_solver.o: ortools/sat/sat_solver.cc ortools/sat/sat_solver.h \
 ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/base/span.h \
 ortools/base/inlined_vector.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/sat_base.h ortools/base/port.h ortools/base/stringprintf.h \
 ortools/sat/model.h ortools/base/map_util.h ortools/base/typeid.h \
 ortools/util/bitset.h ortools/sat/drat_writer.h ortools/base/file.h \
 ortools/base/status.h ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h \
 ortools/util/stats.h ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/base/stl_util.h ortools/base/sysinfo.h \
 ortools/port/proto_utils.h ortools/port/sysinfo.h ortools/sat/util.h \
 ortools/base/random.h ortools/util/saturated_arithmetic.h \
 ortools/base/casts.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Ssat_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Ssat_solver.$O

objs/sat/simplification.o: ortools/sat/simplification.cc \
 ortools/sat/simplification.h ortools/base/adjustable_priority_queue.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/base/span.h \
 ortools/base/inlined_vector.h ortools/sat/drat_proof_handler.h \
 ortools/sat/drat_checker.h ortools/sat/sat_base.h ortools/base/port.h \
 ortools/base/stringprintf.h ortools/sat/model.h ortools/base/map_util.h \
 ortools/base/typeid.h ortools/util/bitset.h ortools/sat/drat_writer.h \
 ortools/base/file.h ortools/base/status.h ortools/base/join.h \
 ortools/base/string_view.h ortools/gen/ortools/sat/sat_parameters.pb.h \
 ortools/sat/sat_solver.h ortools/base/hash.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/util/random_engine.h ortools/util/stats.h \
 ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/algorithms/dynamic_partition.h \
 ortools/base/adjustable_priority_queue-inl.h ortools/base/random.h \
 ortools/base/stl_util.h ortools/graph/strongly_connected_components.h \
 ortools/sat/util.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Ssimplification.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Ssimplification.$O

objs/sat/symmetry.o: ortools/sat/symmetry.cc ortools/sat/symmetry.h \
 ortools/algorithms/sparse_permutation.h ortools/base/logging.h \
 ortools/base/integral_types.h ortools/base/macros.h \
 ortools/base/int_type_indexed_vector.h ortools/base/int_type.h \
 ortools/base/span.h ortools/base/inlined_vector.h ortools/sat/sat_base.h \
 ortools/base/port.h ortools/base/stringprintf.h ortools/sat/model.h \
 ortools/base/map_util.h ortools/base/typeid.h ortools/util/bitset.h \
 ortools/base/basictypes.h ortools/util/stats.h ortools/base/timer.h \
 ortools/base/time_support.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Ssymmetry.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Ssymmetry.$O

objs/sat/table.o: ortools/sat/table.cc ortools/sat/table.h \
 ortools/base/integral_types.h ortools/sat/integer.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/logging.h ortools/base/macros.h \
 ortools/base/inlined_vector.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/base/join.h \
 ortools/base/string_view.h ortools/base/map_util.h ortools/base/port.h \
 ortools/base/span.h ortools/graph/iterators.h ortools/sat/model.h \
 ortools/base/typeid.h ortools/sat/sat_base.h ortools/base/stringprintf.h \
 ortools/util/bitset.h ortools/sat/sat_solver.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h \
 ortools/util/stats.h ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h ortools/base/stl_util.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Stable.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Stable.$O

objs/sat/theta_tree.o: ortools/sat/theta_tree.cc ortools/sat/theta_tree.h \
 ortools/base/logging.h ortools/base/integral_types.h \
 ortools/base/macros.h ortools/sat/integer.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/inlined_vector.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/base/join.h ortools/base/string_view.h ortools/base/map_util.h \
 ortools/base/port.h ortools/base/span.h ortools/graph/iterators.h \
 ortools/sat/model.h ortools/base/typeid.h ortools/sat/sat_base.h \
 ortools/base/stringprintf.h ortools/util/bitset.h \
 ortools/sat/sat_solver.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h \
 ortools/util/stats.h ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Stheta_tree.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Stheta_tree.$O

objs/sat/timetable.o: ortools/sat/timetable.cc ortools/sat/timetable.h \
 ortools/base/macros.h ortools/sat/integer.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/inlined_vector.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/base/join.h ortools/base/string_view.h ortools/base/map_util.h \
 ortools/base/port.h ortools/base/span.h ortools/graph/iterators.h \
 ortools/sat/model.h ortools/base/typeid.h ortools/sat/sat_base.h \
 ortools/base/stringprintf.h ortools/util/bitset.h \
 ortools/sat/sat_solver.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h \
 ortools/util/stats.h ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h ortools/sat/intervals.h \
 ortools/sat/cp_constraints.h ortools/sat/integer_expr.h \
 ortools/sat/precedences.h ortools/util/sort.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Stimetable.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Stimetable.$O

objs/sat/timetable_edgefinding.o: ortools/sat/timetable_edgefinding.cc \
 ortools/sat/timetable_edgefinding.h ortools/base/int_type.h \
 ortools/base/macros.h ortools/sat/integer.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/inlined_vector.h \
 ortools/base/int_type_indexed_vector.h ortools/base/join.h \
 ortools/base/string_view.h ortools/base/map_util.h ortools/base/port.h \
 ortools/base/span.h ortools/graph/iterators.h ortools/sat/model.h \
 ortools/base/typeid.h ortools/sat/sat_base.h ortools/base/stringprintf.h \
 ortools/util/bitset.h ortools/sat/sat_solver.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h ortools/base/status.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h \
 ortools/util/stats.h ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h ortools/sat/intervals.h \
 ortools/sat/cp_constraints.h ortools/sat/integer_expr.h \
 ortools/sat/precedences.h ortools/util/sort.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Stimetable_edgefinding.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Stimetable_edgefinding.$O

objs/sat/util.o: ortools/sat/util.cc ortools/sat/util.h \
 ortools/base/random.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/sat/model.h ortools/base/map_util.h \
 ortools/base/typeid.h ortools/sat/sat_base.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/base/port.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/base/stringprintf.h ortools/util/bitset.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Ssat$Sutil.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sutil.$O

ortools/sat/boolean_problem.proto: ;

$(GEN_DIR)/ortools/sat/boolean_problem.pb.cc: \
 $(SRC_DIR)/ortools/sat/boolean_problem.proto | $(GEN_DIR)/ortools/sat
	$(PROTOC) --proto_path=$(INC_DIR) $(PROTOBUF_PROTOC_INC) --cpp_out=$(GEN_PATH) $(SRC_DIR)/ortools/sat/boolean_problem.proto

$(GEN_DIR)/ortools/sat/boolean_problem.pb.h: \
 $(GEN_DIR)/ortools/sat/boolean_problem.pb.cc
	$(TOUCH) $(GEN_PATH)$Sortools$Ssat$Sboolean_problem.pb.h

$(OBJ_DIR)/sat/boolean_problem.pb.$O: \
 $(GEN_DIR)/ortools/sat/boolean_problem.pb.cc | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(GEN_PATH)$Sortools$Ssat$Sboolean_problem.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sboolean_problem.pb.$O

ortools/sat/cp_model.proto: ;

$(GEN_DIR)/ortools/sat/cp_model.pb.cc: \
 $(SRC_DIR)/ortools/sat/cp_model.proto | $(GEN_DIR)/ortools/sat
	$(PROTOC) --proto_path=$(INC_DIR) $(PROTOBUF_PROTOC_INC) --cpp_out=$(GEN_PATH) $(SRC_DIR)/ortools/sat/cp_model.proto

$(GEN_DIR)/ortools/sat/cp_model.pb.h: \
 $(GEN_DIR)/ortools/sat/cp_model.pb.cc
	$(TOUCH) $(GEN_PATH)$Sortools$Ssat$Scp_model.pb.h

$(OBJ_DIR)/sat/cp_model.pb.$O: \
 $(GEN_DIR)/ortools/sat/cp_model.pb.cc | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(GEN_PATH)$Sortools$Ssat$Scp_model.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Scp_model.pb.$O

ortools/sat/sat_parameters.proto: ;

$(GEN_DIR)/ortools/sat/sat_parameters.pb.cc: \
 $(SRC_DIR)/ortools/sat/sat_parameters.proto | $(GEN_DIR)/ortools/sat
	$(PROTOC) --proto_path=$(INC_DIR) $(PROTOBUF_PROTOC_INC) --cpp_out=$(GEN_PATH) $(SRC_DIR)/ortools/sat/sat_parameters.proto

$(GEN_DIR)/ortools/sat/sat_parameters.pb.h: \
 $(GEN_DIR)/ortools/sat/sat_parameters.pb.cc
	$(TOUCH) $(GEN_PATH)$Sortools$Ssat$Ssat_parameters.pb.h

$(OBJ_DIR)/sat/sat_parameters.pb.$O: \
 $(GEN_DIR)/ortools/sat/sat_parameters.pb.cc | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(GEN_PATH)$Sortools$Ssat$Ssat_parameters.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Ssat_parameters.pb.$O

BOP_DEPS = \
 $(SRC_DIR)/ortools/bop/bop_base.h \
 $(SRC_DIR)/ortools/bop/bop_fs.h \
 $(SRC_DIR)/ortools/bop/bop_lns.h \
 $(SRC_DIR)/ortools/bop/bop_ls.h \
 $(SRC_DIR)/ortools/bop/bop_portfolio.h \
 $(SRC_DIR)/ortools/bop/bop_solution.h \
 $(SRC_DIR)/ortools/bop/bop_solver.h \
 $(SRC_DIR)/ortools/bop/bop_types.h \
 $(SRC_DIR)/ortools/bop/bop_util.h \
 $(SRC_DIR)/ortools/bop/complete_optimizer.h \
 $(SRC_DIR)/ortools/bop/integral_solver.h \
 $(GEN_DIR)/ortools/bop/bop_parameters.pb.h

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

objs/bop/bop_base.o: ortools/bop/bop_base.cc ortools/bop/bop_base.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/mutex.h \
 ortools/gen/ortools/bop/bop_parameters.pb.h ortools/bop/bop_solution.h \
 ortools/bop/bop_types.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/sat/boolean_problem.h \
 ortools/algorithms/sparse_permutation.h ortools/base/status.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/sat/boolean_problem.pb.h \
 ortools/gen/ortools/sat/cp_model.pb.h ortools/sat/pb_constraint.h \
 ortools/base/hash.h ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/sat/sat_base.h ortools/base/port.h ortools/base/stringprintf.h \
 ortools/sat/model.h ortools/base/map_util.h ortools/base/typeid.h \
 ortools/util/bitset.h ortools/gen/ortools/sat/sat_parameters.pb.h \
 ortools/util/stats.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/sat/sat_solver.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h \
 ortools/util/random_engine.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/sat/simplification.h ortools/base/adjustable_priority_queue.h \
 ortools/lp_data/lp_types.h | $(OBJ_DIR)/bop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbop$Sbop_base.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_base.$O

objs/bop/bop_fs.o: ortools/bop/bop_fs.cc ortools/bop/bop_fs.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/base/random.h \
 ortools/bop/bop_base.h ortools/base/mutex.h \
 ortools/gen/ortools/bop/bop_parameters.pb.h ortools/bop/bop_solution.h \
 ortools/bop/bop_types.h ortools/sat/boolean_problem.h \
 ortools/algorithms/sparse_permutation.h ortools/base/status.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/sat/boolean_problem.pb.h \
 ortools/gen/ortools/sat/cp_model.pb.h ortools/sat/pb_constraint.h \
 ortools/base/hash.h ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/sat/sat_base.h ortools/base/port.h ortools/base/stringprintf.h \
 ortools/sat/model.h ortools/base/map_util.h ortools/base/typeid.h \
 ortools/util/bitset.h ortools/gen/ortools/sat/sat_parameters.pb.h \
 ortools/util/stats.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/sat/sat_solver.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h \
 ortools/util/random_engine.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/sat/simplification.h ortools/base/adjustable_priority_queue.h \
 ortools/lp_data/lp_types.h ortools/bop/bop_util.h \
 ortools/glop/lp_solver.h ortools/gen/ortools/glop/parameters.pb.h \
 ortools/glop/preprocessor.h ortools/glop/revised_simplex.h \
 ortools/glop/basis_representation.h ortools/glop/lu_factorization.h \
 ortools/glop/markowitz.h ortools/glop/status.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/util/return_macros.h \
 ortools/lp_data/sparse_column.h ortools/lp_data/sparse_vector.h \
 ortools/graph/iterators.h ortools/glop/rank_one_update.h \
 ortools/lp_data/lp_utils.h ortools/base/accurate_sum.h \
 ortools/glop/dual_edge_norms.h ortools/lp_data/lp_data.h \
 ortools/util/fp_utils.h ortools/glop/entering_variable.h \
 ortools/glop/primal_edge_norms.h ortools/glop/update_row.h \
 ortools/glop/variables_info.h ortools/glop/reduced_costs.h \
 ortools/glop/variable_values.h ortools/lp_data/lp_print_utils.h \
 ortools/lp_data/sparse_row.h ortools/lp_data/matrix_scaler.h \
 ortools/base/stl_util.h ortools/sat/lp_utils.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h ortools/sat/symmetry.h \
 ortools/sat/util.h | $(OBJ_DIR)/bop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbop$Sbop_fs.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_fs.$O

objs/bop/bop_lns.o: ortools/bop/bop_lns.cc ortools/bop/bop_lns.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/base/random.h \
 ortools/bop/bop_base.h ortools/base/mutex.h \
 ortools/gen/ortools/bop/bop_parameters.pb.h ortools/bop/bop_solution.h \
 ortools/bop/bop_types.h ortools/sat/boolean_problem.h \
 ortools/algorithms/sparse_permutation.h ortools/base/status.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/sat/boolean_problem.pb.h \
 ortools/gen/ortools/sat/cp_model.pb.h ortools/sat/pb_constraint.h \
 ortools/base/hash.h ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/sat/sat_base.h ortools/base/port.h ortools/base/stringprintf.h \
 ortools/sat/model.h ortools/base/map_util.h ortools/base/typeid.h \
 ortools/util/bitset.h ortools/gen/ortools/sat/sat_parameters.pb.h \
 ortools/util/stats.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/sat/sat_solver.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h \
 ortools/util/random_engine.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/sat/simplification.h ortools/base/adjustable_priority_queue.h \
 ortools/lp_data/lp_types.h ortools/bop/bop_util.h \
 ortools/glop/lp_solver.h ortools/gen/ortools/glop/parameters.pb.h \
 ortools/glop/preprocessor.h ortools/glop/revised_simplex.h \
 ortools/glop/basis_representation.h ortools/glop/lu_factorization.h \
 ortools/glop/markowitz.h ortools/glop/status.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/util/return_macros.h \
 ortools/lp_data/sparse_column.h ortools/lp_data/sparse_vector.h \
 ortools/graph/iterators.h ortools/glop/rank_one_update.h \
 ortools/lp_data/lp_utils.h ortools/base/accurate_sum.h \
 ortools/glop/dual_edge_norms.h ortools/lp_data/lp_data.h \
 ortools/util/fp_utils.h ortools/glop/entering_variable.h \
 ortools/glop/primal_edge_norms.h ortools/glop/update_row.h \
 ortools/glop/variables_info.h ortools/glop/reduced_costs.h \
 ortools/glop/variable_values.h ortools/lp_data/lp_print_utils.h \
 ortools/lp_data/sparse_row.h ortools/lp_data/matrix_scaler.h \
 ortools/base/cleanup.h ortools/base/stl_util.h ortools/sat/lp_utils.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h | $(OBJ_DIR)/bop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbop$Sbop_lns.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_lns.$O

objs/bop/bop_ls.o: ortools/bop/bop_ls.cc ortools/bop/bop_ls.h \
 ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/random.h ortools/bop/bop_base.h \
 ortools/base/mutex.h ortools/gen/ortools/bop/bop_parameters.pb.h \
 ortools/bop/bop_solution.h ortools/bop/bop_types.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/sat/boolean_problem.h ortools/algorithms/sparse_permutation.h \
 ortools/base/status.h ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/sat/boolean_problem.pb.h \
 ortools/gen/ortools/sat/cp_model.pb.h ortools/sat/pb_constraint.h \
 ortools/base/span.h ortools/base/inlined_vector.h ortools/sat/sat_base.h \
 ortools/base/port.h ortools/base/stringprintf.h ortools/sat/model.h \
 ortools/base/map_util.h ortools/base/typeid.h ortools/util/bitset.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/stats.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/sat/sat_solver.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h \
 ortools/util/random_engine.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/sat/simplification.h ortools/base/adjustable_priority_queue.h \
 ortools/lp_data/lp_types.h ortools/bop/bop_util.h | $(OBJ_DIR)/bop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbop$Sbop_ls.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_ls.$O

objs/bop/bop_portfolio.o: ortools/bop/bop_portfolio.cc \
 ortools/bop/bop_portfolio.h ortools/bop/bop_base.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/mutex.h \
 ortools/gen/ortools/bop/bop_parameters.pb.h ortools/bop/bop_solution.h \
 ortools/bop/bop_types.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/sat/boolean_problem.h \
 ortools/algorithms/sparse_permutation.h ortools/base/status.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/sat/boolean_problem.pb.h \
 ortools/gen/ortools/sat/cp_model.pb.h ortools/sat/pb_constraint.h \
 ortools/base/hash.h ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/sat/sat_base.h ortools/base/port.h ortools/base/stringprintf.h \
 ortools/sat/model.h ortools/base/map_util.h ortools/base/typeid.h \
 ortools/util/bitset.h ortools/gen/ortools/sat/sat_parameters.pb.h \
 ortools/util/stats.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/sat/sat_solver.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h \
 ortools/util/random_engine.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/sat/simplification.h ortools/base/adjustable_priority_queue.h \
 ortools/lp_data/lp_types.h ortools/bop/bop_lns.h ortools/base/random.h \
 ortools/bop/bop_util.h ortools/glop/lp_solver.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/glop/preprocessor.h \
 ortools/glop/revised_simplex.h ortools/glop/basis_representation.h \
 ortools/glop/lu_factorization.h ortools/glop/markowitz.h \
 ortools/glop/status.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/util/return_macros.h \
 ortools/lp_data/sparse_column.h ortools/lp_data/sparse_vector.h \
 ortools/graph/iterators.h ortools/glop/rank_one_update.h \
 ortools/lp_data/lp_utils.h ortools/base/accurate_sum.h \
 ortools/glop/dual_edge_norms.h ortools/lp_data/lp_data.h \
 ortools/util/fp_utils.h ortools/glop/entering_variable.h \
 ortools/glop/primal_edge_norms.h ortools/glop/update_row.h \
 ortools/glop/variables_info.h ortools/glop/reduced_costs.h \
 ortools/glop/variable_values.h ortools/lp_data/lp_print_utils.h \
 ortools/lp_data/sparse_row.h ortools/lp_data/matrix_scaler.h \
 ortools/base/stl_util.h ortools/bop/bop_fs.h ortools/bop/bop_ls.h \
 ortools/bop/complete_optimizer.h ortools/sat/encoding.h \
 ortools/sat/symmetry.h | $(OBJ_DIR)/bop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbop$Sbop_portfolio.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_portfolio.$O

objs/bop/bop_solution.o: ortools/bop/bop_solution.cc \
 ortools/bop/bop_solution.h ortools/bop/bop_types.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/sat/boolean_problem.h \
 ortools/algorithms/sparse_permutation.h ortools/base/status.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/sat/boolean_problem.pb.h \
 ortools/gen/ortools/sat/cp_model.pb.h ortools/sat/pb_constraint.h \
 ortools/base/hash.h ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/sat/sat_base.h ortools/base/port.h ortools/base/stringprintf.h \
 ortools/sat/model.h ortools/base/map_util.h ortools/base/typeid.h \
 ortools/util/bitset.h ortools/gen/ortools/sat/sat_parameters.pb.h \
 ortools/util/stats.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/sat/sat_solver.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h \
 ortools/util/random_engine.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/sat/simplification.h ortools/base/adjustable_priority_queue.h | $(OBJ_DIR)/bop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbop$Sbop_solution.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_solution.$O

objs/bop/bop_solver.o: ortools/bop/bop_solver.cc ortools/bop/bop_solver.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/bop/bop_base.h \
 ortools/base/mutex.h ortools/gen/ortools/bop/bop_parameters.pb.h \
 ortools/bop/bop_solution.h ortools/bop/bop_types.h \
 ortools/sat/boolean_problem.h ortools/algorithms/sparse_permutation.h \
 ortools/base/status.h ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/sat/boolean_problem.pb.h \
 ortools/gen/ortools/sat/cp_model.pb.h ortools/sat/pb_constraint.h \
 ortools/base/hash.h ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/sat/sat_base.h ortools/base/port.h ortools/base/stringprintf.h \
 ortools/sat/model.h ortools/base/map_util.h ortools/base/typeid.h \
 ortools/util/bitset.h ortools/gen/ortools/sat/sat_parameters.pb.h \
 ortools/util/stats.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/sat/sat_solver.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h \
 ortools/util/random_engine.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/sat/simplification.h ortools/base/adjustable_priority_queue.h \
 ortools/lp_data/lp_types.h ortools/glop/lp_solver.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/glop/preprocessor.h \
 ortools/glop/revised_simplex.h ortools/glop/basis_representation.h \
 ortools/glop/lu_factorization.h ortools/glop/markowitz.h \
 ortools/glop/status.h ortools/lp_data/sparse.h \
 ortools/lp_data/permutation.h ortools/base/random.h \
 ortools/util/return_macros.h ortools/lp_data/sparse_column.h \
 ortools/lp_data/sparse_vector.h ortools/graph/iterators.h \
 ortools/glop/rank_one_update.h ortools/lp_data/lp_utils.h \
 ortools/base/accurate_sum.h ortools/glop/dual_edge_norms.h \
 ortools/lp_data/lp_data.h ortools/util/fp_utils.h \
 ortools/glop/entering_variable.h ortools/glop/primal_edge_norms.h \
 ortools/glop/update_row.h ortools/glop/variables_info.h \
 ortools/glop/reduced_costs.h ortools/glop/variable_values.h \
 ortools/lp_data/lp_print_utils.h ortools/lp_data/sparse_row.h \
 ortools/lp_data/matrix_scaler.h ortools/base/stl_util.h \
 ortools/bop/bop_fs.h ortools/bop/bop_util.h ortools/bop/bop_lns.h \
 ortools/bop/bop_ls.h ortools/bop/bop_portfolio.h \
 ortools/bop/complete_optimizer.h ortools/sat/encoding.h \
 ortools/sat/lp_utils.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h | $(OBJ_DIR)/bop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbop$Sbop_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_solver.$O

objs/bop/bop_util.o: ortools/bop/bop_util.cc ortools/bop/bop_util.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/bop/bop_base.h \
 ortools/base/mutex.h ortools/gen/ortools/bop/bop_parameters.pb.h \
 ortools/bop/bop_solution.h ortools/bop/bop_types.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/sat/boolean_problem.h ortools/algorithms/sparse_permutation.h \
 ortools/base/status.h ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/sat/boolean_problem.pb.h \
 ortools/gen/ortools/sat/cp_model.pb.h ortools/sat/pb_constraint.h \
 ortools/base/hash.h ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/sat/sat_base.h ortools/base/port.h ortools/base/stringprintf.h \
 ortools/sat/model.h ortools/base/map_util.h ortools/base/typeid.h \
 ortools/util/bitset.h ortools/gen/ortools/sat/sat_parameters.pb.h \
 ortools/util/stats.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/sat/sat_solver.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h \
 ortools/util/random_engine.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/sat/simplification.h ortools/base/adjustable_priority_queue.h \
 ortools/lp_data/lp_types.h | $(OBJ_DIR)/bop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbop$Sbop_util.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_util.$O

objs/bop/complete_optimizer.o: ortools/bop/complete_optimizer.cc \
 ortools/bop/complete_optimizer.h ortools/bop/bop_base.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/mutex.h \
 ortools/gen/ortools/bop/bop_parameters.pb.h ortools/bop/bop_solution.h \
 ortools/bop/bop_types.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/sat/boolean_problem.h \
 ortools/algorithms/sparse_permutation.h ortools/base/status.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/sat/boolean_problem.pb.h \
 ortools/gen/ortools/sat/cp_model.pb.h ortools/sat/pb_constraint.h \
 ortools/base/hash.h ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/sat/sat_base.h ortools/base/port.h ortools/base/stringprintf.h \
 ortools/sat/model.h ortools/base/map_util.h ortools/base/typeid.h \
 ortools/util/bitset.h ortools/gen/ortools/sat/sat_parameters.pb.h \
 ortools/util/stats.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/sat/sat_solver.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h \
 ortools/util/random_engine.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/sat/simplification.h ortools/base/adjustable_priority_queue.h \
 ortools/lp_data/lp_types.h ortools/sat/encoding.h ortools/bop/bop_util.h \
 ortools/sat/optimization.h ortools/sat/integer.h \
 ortools/graph/iterators.h ortools/util/rev.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/sorted_interval_list.h ortools/sat/integer_search.h | $(OBJ_DIR)/bop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbop$Scomplete_optimizer.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Scomplete_optimizer.$O

objs/bop/integral_solver.o: ortools/bop/integral_solver.cc \
 ortools/bop/integral_solver.h ortools/base/port.h \
 ortools/gen/ortools/bop/bop_parameters.pb.h ortools/bop/bop_types.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/lp_data/lp_data.h \
 ortools/base/hash.h ortools/gen/ortools/glop/parameters.pb.h \
 ortools/lp_data/lp_types.h ortools/util/bitset.h \
 ortools/lp_data/sparse.h ortools/lp_data/permutation.h \
 ortools/base/random.h ortools/util/return_macros.h \
 ortools/lp_data/sparse_column.h ortools/lp_data/sparse_vector.h \
 ortools/base/stringprintf.h ortools/graph/iterators.h \
 ortools/util/fp_utils.h ortools/util/time_limit.h \
 ortools/base/commandlineflags.h ortools/base/memory.h \
 ortools/base/time_support.h ortools/base/timer.h \
 ortools/util/running_stat.h ortools/bop/bop_solver.h \
 ortools/bop/bop_base.h ortools/base/mutex.h ortools/bop/bop_solution.h \
 ortools/sat/boolean_problem.h ortools/algorithms/sparse_permutation.h \
 ortools/base/status.h ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/sat/boolean_problem.pb.h \
 ortools/gen/ortools/sat/cp_model.pb.h ortools/sat/pb_constraint.h \
 ortools/base/span.h ortools/base/inlined_vector.h ortools/sat/sat_base.h \
 ortools/sat/model.h ortools/base/map_util.h ortools/base/typeid.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/stats.h \
 ortools/sat/sat_solver.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/drat_writer.h ortools/base/file.h \
 ortools/util/random_engine.h ortools/sat/restart.h \
 ortools/sat/sat_decision.h ortools/util/integer_pq.h \
 ortools/sat/simplification.h ortools/base/adjustable_priority_queue.h \
 ortools/glop/lp_solver.h ortools/glop/preprocessor.h \
 ortools/glop/revised_simplex.h ortools/glop/basis_representation.h \
 ortools/glop/lu_factorization.h ortools/glop/markowitz.h \
 ortools/glop/status.h ortools/glop/rank_one_update.h \
 ortools/lp_data/lp_utils.h ortools/base/accurate_sum.h \
 ortools/glop/dual_edge_norms.h ortools/glop/entering_variable.h \
 ortools/glop/primal_edge_norms.h ortools/glop/update_row.h \
 ortools/glop/variables_info.h ortools/glop/reduced_costs.h \
 ortools/glop/variable_values.h ortools/lp_data/lp_print_utils.h \
 ortools/lp_data/sparse_row.h ortools/lp_data/matrix_scaler.h \
 ortools/lp_data/lp_decomposer.h | $(OBJ_DIR)/bop
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sbop$Sintegral_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sintegral_solver.$O

ortools/bop/bop_parameters.proto: ;

$(GEN_DIR)/ortools/bop/bop_parameters.pb.cc: \
 $(SRC_DIR)/ortools/bop/bop_parameters.proto | $(GEN_DIR)/ortools/bop
	$(PROTOC) --proto_path=$(INC_DIR) $(PROTOBUF_PROTOC_INC) --cpp_out=$(GEN_PATH) $(SRC_DIR)/ortools/bop/bop_parameters.proto

$(GEN_DIR)/ortools/bop/bop_parameters.pb.h: \
 $(GEN_DIR)/ortools/bop/bop_parameters.pb.cc
	$(TOUCH) $(GEN_PATH)$Sortools$Sbop$Sbop_parameters.pb.h

$(OBJ_DIR)/bop/bop_parameters.pb.$O: \
 $(GEN_DIR)/ortools/bop/bop_parameters.pb.cc | $(OBJ_DIR)/bop
	$(CCC) $(CFLAGS) -c $(GEN_PATH)$Sortools$Sbop$Sbop_parameters.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_parameters.pb.$O

LP_DEPS = \
 $(SRC_DIR)/ortools/linear_solver/glop_utils.h \
 $(SRC_DIR)/ortools/linear_solver/linear_expr.h \
 $(SRC_DIR)/ortools/linear_solver/linear_solver.h \
 $(SRC_DIR)/ortools/linear_solver/model_exporter.h \
 $(SRC_DIR)/ortools/linear_solver/model_validator.h \
 $(GEN_DIR)/ortools/linear_solver/linear_solver.pb.h

LP_LIB_OBJS = \
 $(OBJ_DIR)/linear_solver/bop_interface.$O \
 $(OBJ_DIR)/linear_solver/cbc_interface.$O \
 $(OBJ_DIR)/linear_solver/clp_interface.$O \
 $(OBJ_DIR)/linear_solver/cplex_interface.$O \
 $(OBJ_DIR)/linear_solver/glop_interface.$O \
 $(OBJ_DIR)/linear_solver/glop_utils.$O \
 $(OBJ_DIR)/linear_solver/glpk_interface.$O \
 $(OBJ_DIR)/linear_solver/gurobi_interface.$O \
 $(OBJ_DIR)/linear_solver/linear_expr.$O \
 $(OBJ_DIR)/linear_solver/linear_solver.$O \
 $(OBJ_DIR)/linear_solver/model_exporter.$O \
 $(OBJ_DIR)/linear_solver/model_validator.$O \
 $(OBJ_DIR)/linear_solver/scip_interface.$O \
 $(OBJ_DIR)/linear_solver/linear_solver.pb.$O

objs/linear_solver/bop_interface.o: \
 ortools/linear_solver/bop_interface.cc ortools/base/commandlineflags.h \
 ortools/base/file.h ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/status.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/string_view.h ortools/base/hash.h \
 ortools/base/port.h ortools/base/stringprintf.h \
 ortools/gen/ortools/bop/bop_parameters.pb.h \
 ortools/bop/integral_solver.h ortools/bop/bop_types.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/lp_data/lp_data.h ortools/gen/ortools/glop/parameters.pb.h \
 ortools/lp_data/lp_types.h ortools/util/bitset.h \
 ortools/lp_data/sparse.h ortools/lp_data/permutation.h \
 ortools/base/random.h ortools/util/return_macros.h \
 ortools/lp_data/sparse_column.h ortools/lp_data/sparse_vector.h \
 ortools/graph/iterators.h ortools/util/fp_utils.h \
 ortools/util/time_limit.h ortools/base/memory.h \
 ortools/base/time_support.h ortools/base/timer.h \
 ortools/util/running_stat.h ortools/linear_solver/linear_solver.h \
 ortools/base/optional.h ortools/base/strutil.h \
 ortools/linear_solver/linear_expr.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h \
 ortools/port/proto_utils.h | $(OBJ_DIR)/linear_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slinear_solver$Sbop_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Sbop_interface.$O

objs/linear_solver/cbc_interface.o: \
 ortools/linear_solver/cbc_interface.cc ortools/base/commandlineflags.h \
 ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/port.h ortools/base/stringprintf.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/linear_solver/linear_solver.h ortools/base/optional.h \
 ortools/base/status.h ortools/base/join.h ortools/base/string_view.h \
 ortools/base/strutil.h ortools/gen/ortools/glop/parameters.pb.h \
 ortools/linear_solver/linear_expr.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h \
 ortools/port/proto_utils.h | $(OBJ_DIR)/linear_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slinear_solver$Scbc_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Scbc_interface.$O

objs/linear_solver/clp_interface.o: \
 ortools/linear_solver/clp_interface.cc ortools/base/commandlineflags.h \
 ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/memory.h ortools/base/port.h \
 ortools/base/stringprintf.h ortools/base/strutil.h \
 ortools/base/string_view.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/linear_solver/linear_solver.h \
 ortools/base/optional.h ortools/base/status.h ortools/base/join.h \
 ortools/gen/ortools/glop/parameters.pb.h \
 ortools/linear_solver/linear_expr.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h \
 ortools/port/proto_utils.h | $(OBJ_DIR)/linear_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slinear_solver$Sclp_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Sclp_interface.$O

objs/linear_solver/cplex_interface.o: \
 ortools/linear_solver/cplex_interface.cc ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/stringprintf.h \
 ortools/base/timer.h ortools/base/basictypes.h \
 ortools/base/time_support.h ortools/linear_solver/linear_solver.h \
 ortools/base/commandlineflags.h ortools/base/optional.h \
 ortools/base/port.h ortools/base/status.h ortools/base/join.h \
 ortools/base/string_view.h ortools/base/strutil.h \
 ortools/gen/ortools/glop/parameters.pb.h \
 ortools/linear_solver/linear_expr.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h \
 ortools/port/proto_utils.h | $(OBJ_DIR)/linear_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slinear_solver$Scplex_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Scplex_interface.$O

objs/linear_solver/glop_interface.o: \
 ortools/linear_solver/glop_interface.cc ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/port.h \
 ortools/base/stringprintf.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/glop/lp_solver.h \
 ortools/gen/ortools/glop/parameters.pb.h ortools/glop/preprocessor.h \
 ortools/glop/revised_simplex.h ortools/glop/basis_representation.h \
 ortools/glop/lu_factorization.h ortools/glop/markowitz.h \
 ortools/base/inlined_vector.h ortools/glop/status.h \
 ortools/lp_data/lp_types.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/util/bitset.h \
 ortools/lp_data/sparse.h ortools/lp_data/permutation.h \
 ortools/base/random.h ortools/util/return_macros.h \
 ortools/lp_data/sparse_column.h ortools/lp_data/sparse_vector.h \
 ortools/graph/iterators.h ortools/util/stats.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/glop/rank_one_update.h \
 ortools/lp_data/lp_utils.h ortools/base/accurate_sum.h \
 ortools/glop/dual_edge_norms.h ortools/lp_data/lp_data.h \
 ortools/util/fp_utils.h ortools/glop/entering_variable.h \
 ortools/glop/primal_edge_norms.h ortools/glop/update_row.h \
 ortools/glop/variables_info.h ortools/glop/reduced_costs.h \
 ortools/util/random_engine.h ortools/glop/variable_values.h \
 ortools/lp_data/lp_print_utils.h ortools/lp_data/sparse_row.h \
 ortools/util/time_limit.h ortools/base/commandlineflags.h \
 ortools/base/memory.h ortools/util/running_stat.h \
 ortools/lp_data/matrix_scaler.h ortools/linear_solver/glop_utils.h \
 ortools/linear_solver/linear_solver.h ortools/base/optional.h \
 ortools/base/status.h ortools/base/join.h ortools/base/string_view.h \
 ortools/base/strutil.h ortools/linear_solver/linear_expr.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h \
 ortools/port/proto_utils.h | $(OBJ_DIR)/linear_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slinear_solver$Sglop_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Sglop_interface.$O

objs/linear_solver/glop_utils.o: ortools/linear_solver/glop_utils.cc \
 ortools/linear_solver/glop_utils.h ortools/linear_solver/linear_solver.h \
 ortools/base/commandlineflags.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/optional.h \
 ortools/base/port.h ortools/base/status.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/string_view.h \
 ortools/base/stringprintf.h ortools/base/strutil.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/gen/ortools/glop/parameters.pb.h \
 ortools/linear_solver/linear_expr.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h \
 ortools/port/proto_utils.h ortools/lp_data/lp_types.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/util/bitset.h | $(OBJ_DIR)/linear_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slinear_solver$Sglop_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Sglop_utils.$O

objs/linear_solver/glpk_interface.o: \
 ortools/linear_solver/glpk_interface.cc | $(OBJ_DIR)/linear_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slinear_solver$Sglpk_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Sglpk_interface.$O

objs/linear_solver/gurobi_interface.o: \
 ortools/linear_solver/gurobi_interface.cc | $(OBJ_DIR)/linear_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slinear_solver$Sgurobi_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Sgurobi_interface.$O

objs/linear_solver/linear_expr.o: ortools/linear_solver/linear_expr.cc \
 ortools/linear_solver/linear_expr.h ortools/base/logging.h \
 ortools/base/integral_types.h ortools/base/macros.h \
 ortools/linear_solver/linear_solver.h ortools/base/commandlineflags.h \
 ortools/base/optional.h ortools/base/port.h ortools/base/status.h \
 ortools/base/join.h ortools/base/basictypes.h ortools/base/string_view.h \
 ortools/base/stringprintf.h ortools/base/strutil.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/gen/ortools/glop/parameters.pb.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h \
 ortools/port/proto_utils.h | $(OBJ_DIR)/linear_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slinear_solver$Slinear_expr.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Slinear_expr.$O

objs/linear_solver/linear_solver.o: \
 ortools/linear_solver/linear_solver.cc \
 ortools/linear_solver/linear_solver.h ortools/base/commandlineflags.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/optional.h ortools/base/port.h \
 ortools/base/status.h ortools/base/join.h ortools/base/basictypes.h \
 ortools/base/string_view.h ortools/base/stringprintf.h \
 ortools/base/strutil.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/glop/parameters.pb.h \
 ortools/linear_solver/linear_expr.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h \
 ortools/port/proto_utils.h ortools/port/file.h \
 ortools/base/accurate_sum.h ortools/base/canonical_errors.h \
 ortools/base/map_util.h ortools/base/mutex.h ortools/base/stl_util.h \
 ortools/linear_solver/model_exporter.h ortools/base/hash.h \
 ortools/linear_solver/model_validator.h ortools/util/fp_utils.h | $(OBJ_DIR)/linear_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slinear_solver$Slinear_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Slinear_solver.$O

objs/linear_solver/model_exporter.o: \
 ortools/linear_solver/model_exporter.cc \
 ortools/linear_solver/model_exporter.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h \
 ortools/base/commandlineflags.h ortools/base/join.h \
 ortools/base/string_view.h ortools/base/map_util.h \
 ortools/base/stringprintf.h ortools/base/strutil.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h ortools/util/fp_utils.h | $(OBJ_DIR)/linear_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slinear_solver$Smodel_exporter.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Smodel_exporter.$O

objs/linear_solver/model_validator.o: \
 ortools/linear_solver/model_validator.cc \
 ortools/linear_solver/model_validator.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h \
 ortools/base/accurate_sum.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/string_view.h \
 ortools/port/proto_utils.h ortools/base/port.h ortools/util/fp_utils.h | $(OBJ_DIR)/linear_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slinear_solver$Smodel_validator.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Smodel_validator.$O

objs/linear_solver/scip_interface.o: \
 ortools/linear_solver/scip_interface.cc | $(OBJ_DIR)/linear_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Slinear_solver$Sscip_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Sscip_interface.$O

ortools/linear_solver/linear_solver.proto: ;

$(GEN_DIR)/ortools/linear_solver/linear_solver.pb.cc: \
 $(SRC_DIR)/ortools/linear_solver/linear_solver.proto \
 $(GEN_DIR)/ortools/util/optional_boolean.pb.cc | $(GEN_DIR)/ortools/linear_solver
	$(PROTOC) --proto_path=$(INC_DIR) $(PROTOBUF_PROTOC_INC) --cpp_out=$(GEN_PATH) $(SRC_DIR)/ortools/linear_solver/linear_solver.proto

$(GEN_DIR)/ortools/linear_solver/linear_solver.pb.h: \
 $(GEN_DIR)/ortools/linear_solver/linear_solver.pb.cc
	$(TOUCH) $(GEN_PATH)$Sortools$Slinear_solver$Slinear_solver.pb.h

$(OBJ_DIR)/linear_solver/linear_solver.pb.$O: \
 $(GEN_DIR)/ortools/linear_solver/linear_solver.pb.cc | $(OBJ_DIR)/linear_solver
	$(CCC) $(CFLAGS) -c $(GEN_PATH)$Sortools$Slinear_solver$Slinear_solver.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Slinear_solver.pb.$O

CP_DEPS = \
 $(SRC_DIR)/ortools/constraint_solver/constraint_solver.h \
 $(SRC_DIR)/ortools/constraint_solver/constraint_solveri.h \
 $(SRC_DIR)/ortools/constraint_solver/hybrid.h \
 $(SRC_DIR)/ortools/constraint_solver/routing_flags.h \
 $(SRC_DIR)/ortools/constraint_solver/routing.h \
 $(SRC_DIR)/ortools/constraint_solver/routing_neighborhoods.h \
 $(SRC_DIR)/ortools/constraint_solver/routing_types.h \
 $(SRC_DIR)/ortools/constraint_solver/sat_constraint.h \
 $(GEN_DIR)/ortools/constraint_solver/assignment.pb.h \
 $(GEN_DIR)/ortools/constraint_solver/demon_profiler.pb.h \
 $(GEN_DIR)/ortools/constraint_solver/model.pb.h \
 $(GEN_DIR)/ortools/constraint_solver/routing_enums.pb.h \
 $(GEN_DIR)/ortools/constraint_solver/routing_parameters.pb.h \
 $(GEN_DIR)/ortools/constraint_solver/search_limit.pb.h \
 $(GEN_DIR)/ortools/constraint_solver/solver_parameters.pb.h

CP_LIB_OBJS = \
 $(OBJ_DIR)/constraint_solver/ac4_mdd_reset_table.$O \
 $(OBJ_DIR)/constraint_solver/ac4r_table.$O \
 $(OBJ_DIR)/constraint_solver/alldiff_cst.$O \
 $(OBJ_DIR)/constraint_solver/assignment.$O \
 $(OBJ_DIR)/constraint_solver/collect_variables.$O \
 $(OBJ_DIR)/constraint_solver/constraint_solver.$O \
 $(OBJ_DIR)/constraint_solver/constraints.$O \
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
 $(OBJ_DIR)/constraint_solver/routing_neighborhoods.$O \
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

objs/constraint_solver/ac4_mdd_reset_table.o: \
 ortools/constraint_solver/ac4_mdd_reset_table.cc ortools/base/int_type.h \
 ortools/base/macros.h ortools/base/int_type_indexed_vector.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/map_util.h ortools/base/stl_util.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/random.h \
 ortools/base/stringprintf.h ortools/base/sysinfo.h ortools/base/timer.h \
 ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Sac4_mdd_reset_table.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sac4_mdd_reset_table.$O

objs/constraint_solver/ac4r_table.o: \
 ortools/constraint_solver/ac4r_table.cc ortools/base/int_type.h \
 ortools/base/macros.h ortools/base/int_type_indexed_vector.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/map_util.h ortools/base/stl_util.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/random.h \
 ortools/base/stringprintf.h ortools/base/sysinfo.h ortools/base/timer.h \
 ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Sac4r_table.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sac4r_table.$O

objs/constraint_solver/alldiff_cst.o: \
 ortools/constraint_solver/alldiff_cst.cc ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/map_util.h ortools/base/random.h \
 ortools/base/sysinfo.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h ortools/util/string_array.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Salldiff_cst.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Salldiff_cst.$O

objs/constraint_solver/assignment.o: \
 ortools/constraint_solver/assignment.cc ortools/base/file.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/status.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/string_view.h ortools/base/hash.h \
 ortools/base/map_util.h ortools/base/recordio.h \
 ortools/base/stringprintf.h \
 ortools/gen/ortools/constraint_solver/assignment.pb.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/random.h \
 ortools/base/sysinfo.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Sassignment.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sassignment.$O

objs/constraint_solver/collect_variables.o: \
 ortools/constraint_solver/collect_variables.cc \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/stl_util.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/map_util.h ortools/base/random.h \
 ortools/base/stringprintf.h ortools/base/sysinfo.h ortools/base/timer.h \
 ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Scollect_variables.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Scollect_variables.$O

objs/constraint_solver/constraint_solver.o: \
 ortools/constraint_solver/constraint_solver.cc \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/map_util.h \
 ortools/base/random.h ortools/base/stringprintf.h ortools/base/sysinfo.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/base/file.h ortools/base/status.h \
 ortools/base/join.h ortools/base/string_view.h ortools/base/recordio.h \
 ortools/base/stl_util.h ortools/constraint_solver/constraint_solveri.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Sconstraint_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sconstraint_solver.$O

objs/constraint_solver/constraints.o: \
 ortools/constraint_solver/constraints.cc ortools/base/integral_types.h \
 ortools/base/join.h ortools/base/basictypes.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/string_view.h \
 ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/map_util.h ortools/base/random.h ortools/base/sysinfo.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h ortools/util/string_array.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Sconstraints.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sconstraints.$O

objs/constraint_solver/count_cst.o: \
 ortools/constraint_solver/count_cst.cc ortools/base/integral_types.h \
 ortools/base/join.h ortools/base/basictypes.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/string_view.h \
 ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/map_util.h ortools/base/random.h ortools/base/sysinfo.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h ortools/util/string_array.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Scount_cst.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Scount_cst.$O

objs/constraint_solver/default_search.o: \
 ortools/constraint_solver/default_search.cc \
 ortools/base/commandlineflags.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/stringprintf.h \
 ortools/base/random.h ortools/base/basictypes.h ortools/base/stl_util.h \
 ortools/constraint_solver/constraint_solver.h ortools/base/hash.h \
 ortools/base/map_util.h ortools/base/sysinfo.h ortools/base/timer.h \
 ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h ortools/util/cached_log.h \
 ortools/util/string_array.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Sdefault_search.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sdefault_search.$O

objs/constraint_solver/demon_profiler.o: \
 ortools/constraint_solver/demon_profiler.cc ortools/base/file.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/status.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/string_view.h ortools/base/hash.h \
 ortools/base/stl_util.h ortools/base/stringprintf.h \
 ortools/base/time_support.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/map_util.h \
 ortools/base/random.h ortools/base/sysinfo.h ortools/base/timer.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h \
 ortools/gen/ortools/constraint_solver/demon_profiler.pb.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Sdemon_profiler.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sdemon_profiler.$O

objs/constraint_solver/deviation.o: \
 ortools/constraint_solver/deviation.cc ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/mathutil.h \
 ortools/base/basictypes.h ortools/base/casts.h \
 ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/map_util.h ortools/base/random.h ortools/base/sysinfo.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/util/bitset.h \
 ortools/util/sorted_interval_list.h ortools/base/span.h \
 ortools/base/inlined_vector.h ortools/util/tuple_set.h \
 ortools/util/string_array.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Sdeviation.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sdeviation.$O

objs/constraint_solver/diffn.o: ortools/constraint_solver/diffn.cc \
 ortools/base/hash.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/map_util.h \
 ortools/base/random.h ortools/base/sysinfo.h ortools/base/timer.h \
 ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h ortools/util/string_array.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Sdiffn.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sdiffn.$O

objs/constraint_solver/element.o: ortools/constraint_solver/element.cc \
 ortools/base/integral_types.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/logging.h ortools/base/macros.h \
 ortools/base/string_view.h ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/map_util.h ortools/base/random.h ortools/base/sysinfo.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h ortools/util/range_minimum_query.h \
 ortools/util/string_array.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Selement.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Selement.$O

objs/constraint_solver/expr_array.o: \
 ortools/constraint_solver/expr_array.cc ortools/base/integral_types.h \
 ortools/base/join.h ortools/base/basictypes.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/string_view.h ortools/base/mathutil.h \
 ortools/base/casts.h ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/map_util.h ortools/base/random.h ortools/base/sysinfo.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/util/bitset.h \
 ortools/util/sorted_interval_list.h ortools/base/span.h \
 ortools/base/inlined_vector.h ortools/util/tuple_set.h \
 ortools/constraint_solver/constraint_solveri.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h ortools/util/string_array.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Sexpr_array.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sexpr_array.$O

objs/constraint_solver/expr_cst.o: ortools/constraint_solver/expr_cst.cc \
 ortools/base/commandlineflags.h ortools/base/integral_types.h \
 ortools/base/join.h ortools/base/basictypes.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/string_view.h ortools/base/stl_util.h \
 ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h ortools/base/hash.h \
 ortools/base/map_util.h ortools/base/random.h ortools/base/sysinfo.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Sexpr_cst.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sexpr_cst.$O

objs/constraint_solver/expressions.o: \
 ortools/constraint_solver/expressions.cc ortools/base/commandlineflags.h \
 ortools/base/integral_types.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/logging.h ortools/base/macros.h \
 ortools/base/string_view.h ortools/base/map_util.h \
 ortools/base/mathutil.h ortools/base/casts.h ortools/base/stl_util.h \
 ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h ortools/base/hash.h \
 ortools/base/random.h ortools/base/sysinfo.h ortools/base/timer.h \
 ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/util/bitset.h \
 ortools/util/sorted_interval_list.h ortools/base/span.h \
 ortools/base/inlined_vector.h ortools/util/tuple_set.h \
 ortools/constraint_solver/constraint_solveri.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h ortools/util/string_array.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Sexpressions.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sexpressions.$O

objs/constraint_solver/gcc.o: ortools/constraint_solver/gcc.cc \
 ortools/base/int_type.h ortools/base/macros.h \
 ortools/base/int_type_indexed_vector.h ortools/base/integral_types.h \
 ortools/base/join.h ortools/base/basictypes.h ortools/base/logging.h \
 ortools/base/string_view.h ortools/base/map_util.h \
 ortools/base/stl_util.h ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/random.h ortools/base/sysinfo.h ortools/base/timer.h \
 ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Sgcc.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sgcc.$O

objs/constraint_solver/graph_constraints.o: \
 ortools/constraint_solver/graph_constraints.cc \
 ortools/base/integral_types.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/logging.h ortools/base/macros.h \
 ortools/base/string_view.h ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/map_util.h ortools/base/random.h ortools/base/sysinfo.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h ortools/util/string_array.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Sgraph_constraints.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sgraph_constraints.$O

objs/constraint_solver/hybrid.o: ortools/constraint_solver/hybrid.cc \
 ortools/constraint_solver/hybrid.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/map_util.h \
 ortools/base/random.h ortools/base/stringprintf.h ortools/base/sysinfo.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/base/callback.h ortools/base/stl_util.h \
 ortools/constraint_solver/constraint_solveri.h ortools/base/join.h \
 ortools/base/string_view.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h ortools/linear_solver/linear_solver.h \
 ortools/base/optional.h ortools/base/port.h ortools/base/status.h \
 ortools/base/strutil.h ortools/gen/ortools/glop/parameters.pb.h \
 ortools/linear_solver/linear_expr.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h \
 ortools/port/proto_utils.h ortools/util/string_array.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Shybrid.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Shybrid.$O

objs/constraint_solver/interval.o: ortools/constraint_solver/interval.cc \
 ortools/base/integral_types.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/logging.h ortools/base/macros.h \
 ortools/base/string_view.h ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/map_util.h ortools/base/random.h ortools/base/sysinfo.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Sinterval.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sinterval.$O

objs/constraint_solver/io.o: ortools/constraint_solver/io.cc \
 ortools/base/callback.h ortools/base/logging.h \
 ortools/base/integral_types.h ortools/base/macros.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/map_util.h \
 ortools/base/stl_util.h ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/random.h \
 ortools/base/stringprintf.h ortools/base/sysinfo.h ortools/base/timer.h \
 ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Sio.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sio.$O

objs/constraint_solver/local_search.o: \
 ortools/constraint_solver/local_search.cc ortools/base/callback.h \
 ortools/base/logging.h ortools/base/integral_types.h \
 ortools/base/macros.h ortools/base/commandlineflags.h \
 ortools/base/hash.h ortools/base/basictypes.h ortools/base/join.h \
 ortools/base/string_view.h ortools/base/map_util.h ortools/base/random.h \
 ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h ortools/base/sysinfo.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h ortools/graph/hamiltonian_path.h \
 ortools/util/vector_or_function.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Slocal_search.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Slocal_search.$O

objs/constraint_solver/model_cache.o: \
 ortools/constraint_solver/model_cache.cc ortools/base/commandlineflags.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/stl_util.h \
 ortools/constraint_solver/constraint_solver.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/map_util.h ortools/base/random.h \
 ortools/base/stringprintf.h ortools/base/sysinfo.h ortools/base/timer.h \
 ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Smodel_cache.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Smodel_cache.$O

objs/constraint_solver/nogoods.o: ortools/constraint_solver/nogoods.cc \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/stl_util.h \
 ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/map_util.h ortools/base/random.h \
 ortools/base/sysinfo.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/util/string_array.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Snogoods.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Snogoods.$O

objs/constraint_solver/pack.o: ortools/constraint_solver/pack.cc \
 ortools/base/integral_types.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/logging.h ortools/base/macros.h \
 ortools/base/string_view.h ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/map_util.h ortools/base/random.h ortools/base/sysinfo.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Spack.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Spack.$O

objs/constraint_solver/range_cst.o: \
 ortools/constraint_solver/range_cst.cc ortools/base/logging.h \
 ortools/base/integral_types.h ortools/base/macros.h \
 ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/map_util.h ortools/base/random.h \
 ortools/base/sysinfo.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Srange_cst.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Srange_cst.$O

objs/constraint_solver/resource.o: ortools/constraint_solver/resource.cc \
 ortools/base/commandlineflags.h ortools/base/integral_types.h \
 ortools/base/join.h ortools/base/basictypes.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/string_view.h ortools/base/mathutil.h \
 ortools/base/casts.h ortools/base/stl_util.h ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h ortools/base/hash.h \
 ortools/base/map_util.h ortools/base/random.h ortools/base/sysinfo.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/util/bitset.h \
 ortools/util/sorted_interval_list.h ortools/base/span.h \
 ortools/base/inlined_vector.h ortools/util/tuple_set.h \
 ortools/constraint_solver/constraint_solveri.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h ortools/util/monoid_operation_tree.h \
 ortools/util/string_array.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Sresource.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sresource.$O

objs/constraint_solver/routing.o: ortools/constraint_solver/routing.cc \
 ortools/constraint_solver/routing.h \
 ortools/base/adjustable_priority_queue-inl.h \
 ortools/base/adjustable_priority_queue.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/callback.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/int_type_indexed_vector.h ortools/base/int_type.h \
 ortools/constraint_solver/constraint_solver.h ortools/base/map_util.h \
 ortools/base/random.h ortools/base/stringprintf.h ortools/base/sysinfo.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h \
 ortools/gen/ortools/constraint_solver/routing_parameters.pb.h \
 ortools/gen/ortools/constraint_solver/routing_enums.pb.h \
 ortools/constraint_solver/routing_types.h ortools/graph/graph.h \
 ortools/base/port.h ortools/graph/iterators.h \
 ortools/util/range_query_function.h ortools/base/stl_util.h \
 ortools/base/thorough_hash.h \
 ortools/constraint_solver/routing_neighborhoods.h \
 ortools/graph/connectivity.h ortools/graph/linear_assignment.h \
 ortools/graph/ebert_graph.h ortools/util/permutation.h \
 ortools/util/zvector.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Srouting.$O

objs/constraint_solver/routing_flags.o: \
 ortools/constraint_solver/routing_flags.cc \
 ortools/constraint_solver/routing_flags.h \
 ortools/base/commandlineflags.h \
 ortools/gen/ortools/constraint_solver/routing_parameters.pb.h \
 ortools/gen/ortools/constraint_solver/routing_enums.pb.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/base/map_util.h ortools/base/logging.h \
 ortools/base/integral_types.h ortools/base/macros.h \
 ortools/constraint_solver/constraint_solver.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/random.h \
 ortools/base/stringprintf.h ortools/base/sysinfo.h ortools/base/timer.h \
 ortools/base/time_support.h ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_flags.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Srouting_flags.$O

objs/constraint_solver/routing_neighborhoods.o: \
 ortools/constraint_solver/routing_neighborhoods.cc \
 ortools/constraint_solver/routing_neighborhoods.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/map_util.h \
 ortools/base/random.h ortools/base/stringprintf.h ortools/base/sysinfo.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h ortools/constraint_solver/routing_types.h \
 ortools/base/callback.h ortools/base/int_type.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_neighborhoods.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Srouting_neighborhoods.$O

objs/constraint_solver/routing_search.o: \
 ortools/constraint_solver/routing_search.cc ortools/base/small_map.h \
 ortools/base/small_ordered_set.h ortools/constraint_solver/routing.h \
 ortools/base/adjustable_priority_queue-inl.h \
 ortools/base/adjustable_priority_queue.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/callback.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/int_type_indexed_vector.h ortools/base/int_type.h \
 ortools/constraint_solver/constraint_solver.h ortools/base/map_util.h \
 ortools/base/random.h ortools/base/stringprintf.h ortools/base/sysinfo.h \
 ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h \
 ortools/gen/ortools/constraint_solver/routing_parameters.pb.h \
 ortools/gen/ortools/constraint_solver/routing_enums.pb.h \
 ortools/constraint_solver/routing_types.h ortools/graph/graph.h \
 ortools/base/port.h ortools/graph/iterators.h \
 ortools/util/range_query_function.h ortools/graph/christofides.h \
 ortools/graph/eulerian_path.h ortools/graph/minimum_spanning_tree.h \
 ortools/graph/connectivity.h ortools/util/vector_or_function.h \
 ortools/linear_solver/linear_solver.h ortools/base/optional.h \
 ortools/base/status.h ortools/base/strutil.h \
 ortools/gen/ortools/glop/parameters.pb.h \
 ortools/linear_solver/linear_expr.h \
 ortools/gen/ortools/linear_solver/linear_solver.pb.h \
 ortools/gen/ortools/util/optional_boolean.pb.h \
 ortools/port/proto_utils.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_search.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Srouting_search.$O

objs/constraint_solver/sat_constraint.o: \
 ortools/constraint_solver/sat_constraint.cc \
 ortools/constraint_solver/sat_constraint.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/map_util.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/random.h \
 ortools/base/stringprintf.h ortools/base/sysinfo.h ortools/base/timer.h \
 ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h ortools/sat/sat_solver.h \
 ortools/base/int_type.h ortools/base/int_type_indexed_vector.h \
 ortools/sat/clause.h ortools/sat/drat_proof_handler.h \
 ortools/sat/drat_checker.h ortools/sat/sat_base.h ortools/base/port.h \
 ortools/sat/model.h ortools/base/typeid.h ortools/sat/drat_writer.h \
 ortools/base/file.h ortools/base/status.h \
 ortools/gen/ortools/sat/sat_parameters.pb.h ortools/util/random_engine.h \
 ortools/util/stats.h ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/memory.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Ssat_constraint.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssat_constraint.$O

objs/constraint_solver/sched_constraints.o: \
 ortools/constraint_solver/sched_constraints.cc \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/map_util.h ortools/base/random.h \
 ortools/base/sysinfo.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h ortools/util/string_array.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Ssched_constraints.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssched_constraints.$O

objs/constraint_solver/sched_expr.o: \
 ortools/constraint_solver/sched_expr.cc ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/map_util.h ortools/base/random.h \
 ortools/base/sysinfo.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Ssched_expr.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssched_expr.$O

objs/constraint_solver/sched_search.o: \
 ortools/constraint_solver/sched_search.cc ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/map_util.h ortools/base/random.h \
 ortools/base/sysinfo.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h ortools/util/string_array.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Ssched_search.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssched_search.$O

objs/constraint_solver/search.o: ortools/constraint_solver/search.cc \
 ortools/base/bitmap.h ortools/base/basictypes.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/casts.h \
 ortools/base/commandlineflags.h ortools/base/hash.h ortools/base/join.h \
 ortools/base/string_view.h ortools/base/map_util.h ortools/base/random.h \
 ortools/base/stl_util.h ortools/base/stringprintf.h ortools/base/timer.h \
 ortools/base/time_support.h \
 ortools/constraint_solver/constraint_solver.h ortools/base/sysinfo.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/util/bitset.h \
 ortools/util/sorted_interval_list.h ortools/base/span.h \
 ortools/base/inlined_vector.h ortools/util/tuple_set.h \
 ortools/constraint_solver/constraint_solveri.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h ortools/util/string_array.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Ssearch.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssearch.$O

objs/constraint_solver/softgcc.o: ortools/constraint_solver/softgcc.cc \
 ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/map_util.h \
 ortools/base/random.h ortools/base/sysinfo.h ortools/base/timer.h \
 ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h ortools/util/string_array.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Ssoftgcc.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssoftgcc.$O

objs/constraint_solver/table.o: ortools/constraint_solver/table.cc \
 ortools/base/commandlineflags.h ortools/base/integral_types.h \
 ortools/base/join.h ortools/base/basictypes.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/string_view.h ortools/base/map_util.h \
 ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h ortools/base/hash.h \
 ortools/base/random.h ortools/base/sysinfo.h ortools/base/timer.h \
 ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h ortools/constraint_solver/sat_constraint.h \
 ortools/sat/sat_solver.h ortools/base/int_type.h \
 ortools/base/int_type_indexed_vector.h ortools/sat/clause.h \
 ortools/sat/drat_proof_handler.h ortools/sat/drat_checker.h \
 ortools/sat/sat_base.h ortools/base/port.h ortools/sat/model.h \
 ortools/base/typeid.h ortools/sat/drat_writer.h ortools/base/file.h \
 ortools/base/status.h ortools/gen/ortools/sat/sat_parameters.pb.h \
 ortools/util/random_engine.h ortools/util/stats.h \
 ortools/sat/pb_constraint.h ortools/sat/restart.h \
 ortools/util/running_stat.h ortools/sat/sat_decision.h \
 ortools/util/integer_pq.h ortools/util/time_limit.h \
 ortools/base/memory.h ortools/util/string_array.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Stable.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Stable.$O

objs/constraint_solver/timetabling.o: \
 ortools/constraint_solver/timetabling.cc ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/map_util.h ortools/base/random.h \
 ortools/base/sysinfo.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Stimetabling.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Stimetabling.$O

objs/constraint_solver/trace.o: ortools/constraint_solver/trace.cc \
 ortools/base/commandlineflags.h ortools/base/integral_types.h \
 ortools/base/join.h ortools/base/basictypes.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/string_view.h ortools/base/map_util.h \
 ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h ortools/base/hash.h \
 ortools/base/random.h ortools/base/sysinfo.h ortools/base/timer.h \
 ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Strace.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Strace.$O

objs/constraint_solver/tree_monitor.o: \
 ortools/constraint_solver/tree_monitor.cc ortools/base/file.h \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/status.h ortools/base/join.h \
 ortools/base/basictypes.h ortools/base/string_view.h ortools/base/hash.h \
 ortools/base/map_util.h ortools/base/stl_util.h \
 ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/random.h \
 ortools/base/sysinfo.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/util/xml_helper.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Stree_monitor.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Stree_monitor.$O

objs/constraint_solver/utilities.o: \
 ortools/constraint_solver/utilities.cc ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/integral_types.h \
 ortools/base/logging.h ortools/base/macros.h ortools/base/join.h \
 ortools/base/string_view.h ortools/base/map_util.h \
 ortools/base/stringprintf.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/random.h \
 ortools/base/sysinfo.h ortools/base/timer.h ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Sutilities.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sutilities.$O

objs/constraint_solver/visitor.o: ortools/constraint_solver/visitor.cc \
 ortools/base/integral_types.h ortools/base/logging.h \
 ortools/base/macros.h ortools/base/map_util.h ortools/base/stl_util.h \
 ortools/constraint_solver/constraint_solver.h \
 ortools/base/commandlineflags.h ortools/base/hash.h \
 ortools/base/basictypes.h ortools/base/random.h \
 ortools/base/stringprintf.h ortools/base/sysinfo.h ortools/base/timer.h \
 ortools/base/time_support.h \
 ortools/gen/ortools/constraint_solver/solver_parameters.pb.h \
 ortools/util/piecewise_linear_function.h \
 ortools/util/saturated_arithmetic.h ortools/base/casts.h \
 ortools/util/bitset.h ortools/util/sorted_interval_list.h \
 ortools/base/span.h ortools/base/inlined_vector.h \
 ortools/util/tuple_set.h ortools/constraint_solver/constraint_solveri.h \
 ortools/base/join.h ortools/base/string_view.h \
 ortools/gen/ortools/constraint_solver/model.pb.h \
 ortools/gen/ortools/constraint_solver/search_limit.pb.h \
 ortools/util/vector_map.h | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sconstraint_solver$Svisitor.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Svisitor.$O

ortools/constraint_solver/assignment.proto: ;

$(GEN_DIR)/ortools/constraint_solver/assignment.pb.cc: \
 $(SRC_DIR)/ortools/constraint_solver/assignment.proto | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(INC_DIR) $(PROTOBUF_PROTOC_INC) --cpp_out=$(GEN_PATH) $(SRC_DIR)/ortools/constraint_solver/assignment.proto

$(GEN_DIR)/ortools/constraint_solver/assignment.pb.h: \
 $(GEN_DIR)/ortools/constraint_solver/assignment.pb.cc
	$(TOUCH) $(GEN_PATH)$Sortools$Sconstraint_solver$Sassignment.pb.h

$(OBJ_DIR)/constraint_solver/assignment.pb.$O: \
 $(GEN_DIR)/ortools/constraint_solver/assignment.pb.cc | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(GEN_PATH)$Sortools$Sconstraint_solver$Sassignment.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sassignment.pb.$O

ortools/constraint_solver/demon_profiler.proto: ;

$(GEN_DIR)/ortools/constraint_solver/demon_profiler.pb.cc: \
 $(SRC_DIR)/ortools/constraint_solver/demon_profiler.proto | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(INC_DIR) $(PROTOBUF_PROTOC_INC) --cpp_out=$(GEN_PATH) $(SRC_DIR)/ortools/constraint_solver/demon_profiler.proto

$(GEN_DIR)/ortools/constraint_solver/demon_profiler.pb.h: \
 $(GEN_DIR)/ortools/constraint_solver/demon_profiler.pb.cc
	$(TOUCH) $(GEN_PATH)$Sortools$Sconstraint_solver$Sdemon_profiler.pb.h

$(OBJ_DIR)/constraint_solver/demon_profiler.pb.$O: \
 $(GEN_DIR)/ortools/constraint_solver/demon_profiler.pb.cc | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(GEN_PATH)$Sortools$Sconstraint_solver$Sdemon_profiler.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sdemon_profiler.pb.$O

ortools/constraint_solver/model.proto: ;

$(GEN_DIR)/ortools/constraint_solver/model.pb.cc: \
 $(SRC_DIR)/ortools/constraint_solver/model.proto \
 $(GEN_DIR)/ortools/constraint_solver/search_limit.pb.cc | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(INC_DIR) $(PROTOBUF_PROTOC_INC) --cpp_out=$(GEN_PATH) $(SRC_DIR)/ortools/constraint_solver/model.proto

$(GEN_DIR)/ortools/constraint_solver/model.pb.h: \
 $(GEN_DIR)/ortools/constraint_solver/model.pb.cc
	$(TOUCH) $(GEN_PATH)$Sortools$Sconstraint_solver$Smodel.pb.h

$(OBJ_DIR)/constraint_solver/model.pb.$O: \
 $(GEN_DIR)/ortools/constraint_solver/model.pb.cc | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(GEN_PATH)$Sortools$Sconstraint_solver$Smodel.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Smodel.pb.$O

ortools/constraint_solver/routing_enums.proto: ;

$(GEN_DIR)/ortools/constraint_solver/routing_enums.pb.cc: \
 $(SRC_DIR)/ortools/constraint_solver/routing_enums.proto | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(INC_DIR) $(PROTOBUF_PROTOC_INC) --cpp_out=$(GEN_PATH) $(SRC_DIR)/ortools/constraint_solver/routing_enums.proto

$(GEN_DIR)/ortools/constraint_solver/routing_enums.pb.h: \
 $(GEN_DIR)/ortools/constraint_solver/routing_enums.pb.cc
	$(TOUCH) $(GEN_PATH)$Sortools$Sconstraint_solver$Srouting_enums.pb.h

$(OBJ_DIR)/constraint_solver/routing_enums.pb.$O: \
 $(GEN_DIR)/ortools/constraint_solver/routing_enums.pb.cc | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(GEN_PATH)$Sortools$Sconstraint_solver$Srouting_enums.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Srouting_enums.pb.$O

ortools/constraint_solver/routing_parameters.proto: ;

$(GEN_DIR)/ortools/constraint_solver/routing_parameters.pb.cc: \
 $(SRC_DIR)/ortools/constraint_solver/routing_parameters.proto \
 $(GEN_DIR)/ortools/constraint_solver/routing_enums.pb.cc \
 $(GEN_DIR)/ortools/constraint_solver/solver_parameters.pb.cc | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(INC_DIR) $(PROTOBUF_PROTOC_INC) --cpp_out=$(GEN_PATH) $(SRC_DIR)/ortools/constraint_solver/routing_parameters.proto

$(GEN_DIR)/ortools/constraint_solver/routing_parameters.pb.h: \
 $(GEN_DIR)/ortools/constraint_solver/routing_parameters.pb.cc
	$(TOUCH) $(GEN_PATH)$Sortools$Sconstraint_solver$Srouting_parameters.pb.h

$(OBJ_DIR)/constraint_solver/routing_parameters.pb.$O: \
 $(GEN_DIR)/ortools/constraint_solver/routing_parameters.pb.cc | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(GEN_PATH)$Sortools$Sconstraint_solver$Srouting_parameters.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Srouting_parameters.pb.$O

ortools/constraint_solver/search_limit.proto: ;

$(GEN_DIR)/ortools/constraint_solver/search_limit.pb.cc: \
 $(SRC_DIR)/ortools/constraint_solver/search_limit.proto | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(INC_DIR) $(PROTOBUF_PROTOC_INC) --cpp_out=$(GEN_PATH) $(SRC_DIR)/ortools/constraint_solver/search_limit.proto

$(GEN_DIR)/ortools/constraint_solver/search_limit.pb.h: \
 $(GEN_DIR)/ortools/constraint_solver/search_limit.pb.cc
	$(TOUCH) $(GEN_PATH)$Sortools$Sconstraint_solver$Ssearch_limit.pb.h

$(OBJ_DIR)/constraint_solver/search_limit.pb.$O: \
 $(GEN_DIR)/ortools/constraint_solver/search_limit.pb.cc | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(GEN_PATH)$Sortools$Sconstraint_solver$Ssearch_limit.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssearch_limit.pb.$O

ortools/constraint_solver/solver_parameters.proto: ;

$(GEN_DIR)/ortools/constraint_solver/solver_parameters.pb.cc: \
 $(SRC_DIR)/ortools/constraint_solver/solver_parameters.proto | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(INC_DIR) $(PROTOBUF_PROTOC_INC) --cpp_out=$(GEN_PATH) $(SRC_DIR)/ortools/constraint_solver/solver_parameters.proto

$(GEN_DIR)/ortools/constraint_solver/solver_parameters.pb.h: \
 $(GEN_DIR)/ortools/constraint_solver/solver_parameters.pb.cc
	$(TOUCH) $(GEN_PATH)$Sortools$Sconstraint_solver$Ssolver_parameters.pb.h

$(OBJ_DIR)/constraint_solver/solver_parameters.pb.$O: \
 $(GEN_DIR)/ortools/constraint_solver/solver_parameters.pb.cc | $(OBJ_DIR)/constraint_solver
	$(CCC) $(CFLAGS) -c $(GEN_PATH)$Sortools$Sconstraint_solver$Ssolver_parameters.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssolver_parameters.pb.$O

