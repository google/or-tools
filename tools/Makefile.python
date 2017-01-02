##############################Windows specific part##############################

# set this variable to the full path to your Python installation
WINDOWS_PATH_TO_PYTHON =

# Set this variable to use it as PYTHONPATH
WINDOWS_PYTHONPATH = 

###############################Unix specific part################################

# Set UNIX_PYTHON_VER to the version number of the Python installation on your computer that you wish to use with or-tools.
#Example : UNIX_PYTHON_VER = 3.5 or UNIX_PYTHON_VER = 2.7
UNIX_PYTHON_VER = 

# Set this variable to use it as PYTHONPATH.
UNIX_PYTHONPATH = 

##################################################################################

.PHONY: rpy install check

# Let's discover something about where we run
ifeq "$(SHELL)" "cmd.exe"
	SYSTEM = win
else
ifeq "$(SHELL)" "sh.exe"
SYSTEM = win
	S = \\
else
	SYSTEM = unix
	S = /
endif
endif

ifeq ($(SYSTEM),win)
PYTHON_EXECUTABLE = $(WINDOWS_PATH_TO_PYTHON)$Spython
#Set PYTHONPATH only when WINDOWS_PYTHONPATH is used. 
ifneq ($(WINDOWS_PYTHONPATH),)
SET_PYTHONPATH = @set PYTHONPATH=$(WINDOWS_PYTHONPATH) &&
endif
else #UNIX
PYTHON_EXECUTABLE = python$(UNIX_PYTHON_VER)
#Set PYTHONPATH only when UNIX_PYTHONPATH is used. 
ifneq ($(UNIX_PYTHONPATH),)
SET_PYTHONPATH = @PYTHONPATH=$(UNIX_PYTHONPATH)
endif

endif #ifeq ($(SYSTEM),win)

install: 
	$(PYTHON_EXECUTABLE) setup.py install --user
	$(SET_PYTHONPATH) $(PYTHON_EXECUTABLE) check_python_deps.py --log=ERROR

check:
	$(SET_PYTHONPATH) $(PYTHON_EXECUTABLE) check_python_deps.py --log=INFO

rpy: $(EX)
	@echo Running $(EX)
	$(SET_PYTHONPATH) $(PYTHON_EXECUTABLE) check_python_deps.py --log=ERROR
	$(SET_PYTHONPATH) $(PYTHON_EXECUTABLE) $(EX) $(ARGS)

print-% : ; @echo $* = $($*)