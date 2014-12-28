ifeq ($(PLATFORM), LINUX)
  DOXYGEN = doxygen
endif
ifeq ($(PLATFORM), MACOSX)
  DOXYGEN = /Applications/Doxygen.app/Contents/Resources/doxygen
endif

clean_refman:
	-$(DELREC) $(OR_ROOT)documentation$Sgenerated$Sor-tools$Sbase
	-$(DELREC) $(OR_ROOT)documentation$Sgenerated$Sor-tools$Sutil
	-$(DELREC) $(OR_ROOT)documentation$Sgenerated$Sor-tools$Sgraph
	-$(DELREC) $(OR_ROOT)documentation$Sgenerated$Sor-tools$Salgorithms
	-$(DELREC) $(OR_ROOT)documentation$Sgenerated$Sor-tools$Slp_data
	-$(DELREC) $(OR_ROOT)documentation$Sgenerated$Sor-tools$Sglop
	-$(DELREC) $(OR_ROOT)documentation$Sgenerated$Sor-tools$Slinear_solver
	-$(DELREC) $(OR_ROOT)documentation$Sgenerated$Sor-tools$Sconstraint_solver
	-$(DELREC) $(OR_ROOT)documentation$Sgenerated$Sor-tools$Sfz

refman:
	$(DOXYGEN) tools/base.doxy
	$(DOXYGEN) tools/util.doxy
	$(DOXYGEN) tools/graph.doxy
	$(DOXYGEN) tools/algorithms.doxy
	$(DOXYGEN) tools/lp_data.doxy
	$(DOXYGEN) tools/glop.doxy
	$(DOXYGEN) tools/linear_solver.doxy
	$(DOXYGEN) tools/constraint_solver.doxy
	$(DOXYGEN) tools/fz.doxy
