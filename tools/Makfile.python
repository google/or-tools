
##############################Windows specific part##############################

# Please define WINDOWS_PYTHON_PATH (c:\\python27 by default)
WINDOWS_PYTHON_PATH = c:\\python27-64
# Set this variable to use it as PYTHONPATH
WINDOWS_OR_TOOLS_PYTHONPATH = 

###############################Unix specific part################################

# Set this variable to use a different python version. Example : PYTHON_VERSION = 3.5 or PYTHON_VERSION = 2
PYTHON_VERSION = 

# Set this variable to use it as PYTHONPATH
UNIX_OR_TOOLS_PYTHONPATH = 

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
PYTHON_EXECUTABLE = $(WINDOWS_PYTHON_PATH)$Spython
SET_PYTHON_PATH = @set PYTHONPATH=$(WINDOWS_OR_TOOLS_PYTHONPATH) &&
else
PYTHON_EXECUTABLE = python$(PYTHON_VERSION)
SET_PYTHON_PATH = @PYTHONPATH=$(UNIX_OR_TOOLS_PYTHONPATH)
endif

install: 
	$(PYTHON_EXECUTABLE) setup.py install --user
	$(SET_PYTHON_PATH) $(PYTHON_EXECUTABLE) check_python_deps.py --log=ERROR

check:
	$(SET_PYTHON_PATH) $(PYTHON_EXECUTABLE) check_python_deps.py --log=INFO

rpy: $(EX)
	@echo Running $(EX)
	$(SET_PYTHON_PATH) $(PYTHON_EXECUTABLE) check_python_deps.py --log=ERROR
	$(SET_PYTHON_PATH) $(PYTHON_EXECUTABLE) $(EX) $(ARGS)



print-% : ; @echo $* = $($*)