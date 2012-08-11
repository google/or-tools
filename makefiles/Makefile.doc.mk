ifeq ($(PLATFORM), LINUX)
  DOXYGEN = doxygen
endif
ifeq ($(PLATFORM), MACOSX)
  DOXYGEN = /Applications/Doxygen.app/Contents/Resources/doxygen
endif

refman:
	cd documentations/doxygen
	$(DOXYGEN) or-tools.doxy
