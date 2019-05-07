# General commands
.PHONY: help
BOLD=\e[1m
RESET=\e[0m

help:
	@echo -e "${BOLD}SYNOPSIS${RESET}"
	@echo -e "\tmake <target> [NOCACHE=1]"
	@echo
	@echo -e "${BOLD}DESCRIPTION${RESET}"
	@echo -e "\tTools to generate various deliveries for linux distros"
	@echo
	@echo -e "${BOLD}MAKE TARGETS${RESET}"
	@echo -e "\t${BOLD}help${RESET}: display this help and exit."
	@echo
	@echo -e "\t${BOLD}delivery${RESET}: Build ${BOLD}archives${RESET} and ${BOLD}python${RESET} targets."
	@echo -e "\t${BOLD}test_delivery${RESET}: Build ${BOLD}test_archives${RESET} and ${BOLD}test_python${RESET} targets."
	@echo
	@echo -e "\t${BOLD}archives${RESET}: Build all OR-Tools archives in export."
	@echo -e "\t${BOLD}test_archives${RESET}: Test each OR-Tools archives for all ${BOLD}<distro>${RESET} and ${BOLD}<lang>${RESET}."
	@echo
	@echo -e "\t${BOLD}python${RESET}: Build manylinux2010 python 'ortools' wheel packages (2.7, 3.5, 3.6, 3.7)."
	@echo -e "\t${BOLD}test_python${RESET}: Test manylinux2010 python 'ortools' wheel packages (2.7, 3.5, 3.6, 3.7)."
	@echo
	@echo -e "\t${BOLD}docker_<distro>${RESET}: Build an image container with or-tools sources and third parties already prebuilt."
	@echo -e "\t${BOLD}bash_<distro>${RESET}: Run a container using the ${BOLD}docker_<distro>${RESET} image."
	@echo
	@echo -e "\t${BOLD}archive_<distro>${RESET}: Build OR-Tools archive for the specified ${BOLD}<distro>${RESET}."
	@echo -e "\t${BOLD}docker_<distro>_<lang>${RESET}: Build test image containing or-tools archive and ${BOLD}<lang>${RESET} prerequisites."
	@echo -e "\t${BOLD}test_<distro>_<lang>${RESET}: Test OR-Tools archive for the specified ${BOLD}<distro>${RESET} and ${BOLD}<lang>${RESET}."
	@echo -e "\t${BOLD}bash_<distro>_<lang>${RESET}: Run a container using the ${BOLD}docker_<distro>_<lang>${RESET} image."
	@echo -e "\t${BOLD}test_<distro>${RESET}: Test OR-Tools archive for all specified language for the specified ${BOLD}<distro>${RESET}."
	@echo
	@echo -e "\t${BOLD}test_<distro>_<language>${RESET}: Test OR-Tools archive on ${BOLD}<distro>${RESET} distro for ${BOLD}<lang>${RESET} language."
	@echo
	@echo -e "\t${BOLD}clean${RESET}: Clean all docker images but keep archives (i.e. don't touch the export directory)."
	@echo -e "\t${BOLD}distclean${RESET}: Clean all docker images and remove all archives."
	@echo
	@echo -e "\t${BOLD}<distro>${RESET}:"
	@echo -e "\t\t${BOLD}centos-7${RESET} (latest)"
	@echo -e "\t\t${BOLD}debian-9${RESET} (latest)"
	@echo -e "\t\t${BOLD}ubuntu-18.10${RESET} (Ubuntu latest)"
	@echo -e "\t\t${BOLD}ubuntu-18.04${RESET} (Ubuntu 18.04 LTS)"
	@echo -e "\t\t${BOLD}ubuntu-16.04${RESET} (Ubuntu 16.04 LTS)"
	@echo -e "\te.g. 'make test_ubuntu'"
	@echo
	@echo -e "\t${BOLD}<lang>${RESET}: Language to build"
	@echo -e "\t\t${BOLD}cc${RESET} C++"
	@echo -e "\t\t${BOLD}java${RESET} Java (JDK 8.0) SWIG wrappers"
	@echo -e "\t\t${BOLD}dotnet${RESET} .Net Standard 2.0 SWIG wrappers"
	@echo -e "\t\t${BOLD}all${RESET} all"
	@echo -e "\te.g. 'make test_ubuntu_cc'"
	@echo
	@echo -e "\t${BOLD}NOCACHE=1${RESET}: use 'docker build --no-cache' when building container (default use cache)."
	@echo
	@echo -e "${BOLD}NOTES${RESET}"
	@echo -e "\tAll generated code will be located in the export/ folder, use target ${BOLD}distclean${RESET} to remove it."
	@echo

# Delete all implicit rules to speed up makefile
.SUFFIXES:
# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =
# keep all intermediate files e.g. export/docker_*.tar
# src: https://www.gnu.org/software/make/manual/html_node/Special-Targets.html
.SECONDARY:

OR_TOOLS_BRANCH := $(shell git rev-parse --abbrev-ref HEAD)
OR_TOOLS_SHA1 := $(shell git rev-parse --verify HEAD)
include ../../Version.txt
OR_TOOLS_PATCH := $(shell git rev-list --count HEAD)
OR_TOOLS_VERSION := $(OR_TOOLS_MAJOR).$(OR_TOOLS_MINOR).$(OR_TOOLS_PATCH)
ifdef PRE_RELEASE
OR_TOOLS_VERSION := $(OR_TOOLS_VERSION)-beta
endif
$(info branch: $(OR_TOOLS_BRANCH))
$(info SHA1: $(OR_TOOLS_SHA1))
$(info version: $(OR_TOOLS_VERSION))

DOCKER_RUN_CMD := docker run --rm -it --init
ifdef NOCACHE
DOCKER_BUILD_CMD := docker build --no-cache
else
DOCKER_BUILD_CMD := docker build
endif
#################
###  DELIVERY  ##
#################
.PHONY: delivery
delivery: python archives

.PHONY: test_delivery
test_delivery: test_archives

###############
###  PYTHON  ##
###############
.PHONY: docker_python
docker_python: export/python/docker.tar

export/python/build-manylinux1.sh: build-manylinux1.sh | export/python
	cp $< $@

export/python/docker.tar: manylinux1.Dockerfile export/python/build-manylinux1.sh ../../makefiles
	-docker image rm -f or-tools:python 2>/dev/null
	-rm -f $@
	$(DOCKER_BUILD_CMD) \
 -f $< \
 --build-arg SRC_GIT_BRANCH=$(OR_TOOLS_BRANCH) \
 --build-arg SRC_GIT_SHA1=$(OR_TOOLS_SHA1) \
 -t or-tools:python export/python
	docker save or-tools:python -o $@

bash_python: export/python/docker.tar
	$(DOCKER_RUN_CMD) -v `pwd`/export:/export or-tools:python /bin/bash

.PHONY: python
python: export/python/docker.tar
	docker load -i $<
	$(DOCKER_RUN_CMD) -v `pwd`/export:/export or-tools:python /bin/bash -c \
		"/root/build/build-manylinux1.sh /root/src /root/build /export/python"

export/python: | export
	-mkdir $@

#################
###  ARCHIVES  ##
#################

# Currently supported distro
DISTROS = centos-7 debian-9 ubuntu-18.10 ubuntu-18.04 ubuntu-16.04

# Create build docker images with OR-Tools built
targets = $(addprefix docker_, $(DISTROS))
.PHONY: docker $(targets)
docker: $(targets)
$(targets): docker_%: export/%/docker_devel.tar

# Performance: `docker build` use export/$* to reduce the size of the context
# since everything inside this directory is sent to the docker daemon
# before the image is built...
export/%/docker_devel.tar: %.Dockerfile ../Makefile.cc.java.dotnet ../../makefiles ../../ortools
	-mkdir -p export/$*
	-docker image rm -f or-tools_$*:devel 2>/dev/null
	-rm -f $@
	cp or-tools.snk export/$*/
	$(DOCKER_BUILD_CMD) \
 -f $< \
 --build-arg SRC_GIT_BRANCH=$(OR_TOOLS_BRANCH) \
 --build-arg SRC_GIT_SHA1=$(OR_TOOLS_SHA1) \
 -t or-tools_$*:devel export/$*
	docker save or-tools_$*:devel -o $@

# Run a container using devel docker image
targets = $(addprefix bash_, $(DISTROS))
.PHONY: $(targets)
$(targets): bash_%: export/%/docker_devel.tar
	${DOCKER_RUN_CMD} -v `pwd`/export:/export ${IMAGE}_$*:devel /bin/bash

# Build Archives
targets = $(addprefix archive_, $(DISTROS))
.PHONY: archives $(targets)
archives: $(targets)
$(targets): archive_%: export/archives/or-tools_%_v$(OR_TOOLS_VERSION).tar.gz

export/archives/or-tools_%_v$(OR_TOOLS_VERSION).tar.gz: export/%/docker_devel.tar | export/archives
	-rm -f export/archives/or-tools_$*_v*.tar.gz
	-rm -f export/archives/or-tools_flatzinc_$*_v*.tar.gz
	docker load -i $<
	$(DOCKER_RUN_CMD) -w /root/or-tools -v `pwd`/export:/export or-tools_$*:devel /bin/sh -c \
		"make archive && make test_archive && cp *.tar.gz /export/$*"
	$(DOCKER_RUN_CMD) -w /root/or-tools -v `pwd`/export:/export or-tools_$*:devel /bin/sh -c \
		"make fz_archive && make test_fz_archive && cp *.tar.gz /export/$*"
	mv export/$*/or-tools_flatzinc_*.tar.gz export/archives/or-tools_flatzinc_$*_v$(OR_TOOLS_VERSION).tar.gz
	mv export/$*/or-tools_*.tar.gz export/archives/or-tools_$*_v$(OR_TOOLS_VERSION).tar.gz

# Export Dir
export:
	-mkdir $@
# generic rule export/% prevent other rules
# e.g. export/%/docker.devel.tar -> need an exhaustive list
export/archives: | export
	-mkdir $@

############
##  TEST  ##
############
# Create test docker image for each language
targets = $(addsuffix _cc, $(addprefix docker_, $(DISTROS)))
.PHONY: $(targets)
$(targets): docker_%_cc: export/%/docker_cc.tar
export/%/docker_cc.tar: test/%/cc.Dockerfile export/archives/or-tools_%_v$(OR_TOOLS_VERSION).tar.gz
	-docker image rm -f or-tools_$*:cc 2>/dev/null
	-rm -f $@
	$(DOCKER_BUILD_CMD) -f $< -t or-tools_$*:cc export/archives
	docker save or-tools_$*:cc -o $@

targets = $(addsuffix _java, $(addprefix docker_, $(DISTROS)))
.PHONY: $(targets)
$(targets): docker_%_java: export/%/docker_java.tar
export/%/docker_java.tar: test/%/java.Dockerfile export/archives/or-tools_%_v$(OR_TOOLS_VERSION).tar.gz
	-docker image rm -f or-tools_$*:java 2>/dev/null
	-rm -f $@
	$(DOCKER_BUILD_CMD) -f $< -t or-tools_$*:java export/archives
	docker save or-tools_$*:java -o $@

targets = $(addsuffix _dotnet, $(addprefix docker_, $(DISTROS)))
.PHONY: $(targets)
$(targets): docker_%_dotnet: export/%/docker_dotnet.tar
export/%/docker_dotnet.tar: test/%/dotnet.Dockerfile export/archives/or-tools_%_v$(OR_TOOLS_VERSION).tar.gz
	-docker image rm -f or-tools_$*:dotnet 2>/dev/null
	-rm -f $@
	$(DOCKER_BUILD_CMD) -f $< -t or-tools_$*:dotnet export/archives
	docker save or-tools_$*:dotnet -o $@

# Run a container using <language> docker image
targets = $(addsuffix _cc, $(addprefix bash_, $(DISTROS)))
.PHONY: $(targets)
$(targets): bash_%_cc: export/%/docker_cc.tar
	$(DOCKER_RUN_CMD) or-tools_$*:cc /bin/bash

targets = $(addsuffix _java, $(addprefix bash_, $(DISTROS)))
.PHONY: $(targets)
$(targets): bash_%_java: export/%/docker_java.tar
	$(DOCKER_RUN_CMD) or-tools_$*:java /bin/bash

targets = $(addsuffix _dotnet, $(addprefix bash_, $(DISTROS)))
.PHONY: $(targets)
$(targets): bash_%_dotnet: export/%/docker_dotnet.tar
	$(DOCKER_RUN_CMD) or-tools_$*:dotnet /bin/bash

# Test Archive
targets = $(addsuffix _cc, $(addprefix test_, $(DISTROS)))
.PHONY: test_archives_cc $(targets)
test_archives_cc: $(targets)
$(targets): test_%_cc: export/%/test_cc.log
export/%/test_cc.log: export/%/docker_cc.tar
	docker load -i $<
	$(DOCKER_RUN_CMD) or-tools_$*:cc /bin/sh -c "cd or-tools_*_v* && make test_cc"
	@date > $@

targets = $(addsuffix _java, $(addprefix test_, $(DISTROS)))
.PHONY: test_archives_java $(targets)
test_archives_java: $(targets)
$(targets): test_%_java: export/%/test_java.log
export/%/test_java.log: export/%/docker_java.tar
	docker load -i $<
	$(DOCKER_RUN_CMD) or-tools_$*:java /bin/sh -c "cd or-tools_*_v* && make test_java"
	@date > $@

targets = $(addsuffix _dotnet, $(addprefix test_, $(DISTROS)))
.PHONY: test_archives_dotnet $(targets)
test_archives_dotnet: $(targets)
$(targets): test_%_dotnet: export/%/test_dotnet.log
export/%/test_dotnet.log: export/%/docker_dotnet.tar
	docker load -i $<
	$(DOCKER_RUN_CMD) or-tools_$*:dotnet /bin/sh -c "cd or-tools_*_v* && make test_dotnet"
	@date > $@

targets = $(addprefix test_, $(DISTROS))
.PHONY: test_archives $(targets)
$(targets): test_%: test_%_cc test_%_java test_%_dotnet
test_archives: $(targets)

#############
##  CLEAN  ##
#############
targets = $(addprefix clean_, $(DISTROS))
.PHONY: clean $(targets) clean_python
clean: $(targets) clean_python
$(targets): clean_%:
	-rm -f export/$*/test_dotnet.log
	-rm -f export/$*/test_java.log
	-rm -f export/$*/test_cc.log
	-rm -f export/archives/or-tools_flatzinc_$*_v*.tar.gz
	-rm -f export/archives/or-tools_$*_v*.tar.gz

clean_python:
	-rm -f export/python/ortools-*.whl

targets = $(addprefix distclean_, $(DISTROS))
.PHONY: distclean $(targets) distclean_python
distclean: $(targets) distclean_python | export/archives
	-docker container rm $$(docker container ls -f status=exited -q)
#	-docker image rm $$(docker image ls --all -q)
	docker image prune --force
	rmdir export/archives
	rmdir export
$(targets): distclean_%: clean_%
	-docker image rm -f or-tools_$*:dotnet 2>/dev/null
	-rm -f export/$*/docker_dotnet.tar
	-docker image rm -f or-tools_$*:java 2>/dev/null
	-rm -f export/$*/docker_java.tar
	-docker image rm -f or-tools_$*:cc 2>/dev/null
	-rm -f export/$*/docker_cc.tar
	-docker image rm -f or-tools_$*:devel 2>/dev/null
	-rm -f export/$*/docker_devel.tar
	-rm -f export/$*/or-tools.snk
	-rmdir export/$*

distclean_python: clean_python
	-docker image rm -f or-tools:python 2>/dev/null
	-rm -f export/python/docker.tar
	-rm -f export/python/build-manylinux*.sh
	-rmdir export/python

##########################
##  MINIZINC CHALLENGE  ##
##########################
MZN_TAG=or-tools-minizinc-challenge:2018v3

minizinc-challenge-image:
	docker build -f minizinc-challenge.Dockerfile -t $(MZN_TAG) .

minizinc-challenge-image-no-cache:
	docker build --no-cache -f minizinc-challenge.Dockerfile -t $(MZN_TAG) .

minizinc-challenge-test:
	docker run $(MZN_TAG) solver /minizinc/test.mzn /minizinc/2.dzn
	docker run $(MZN_TAG) solver --free-search /minizinc/test.mzn /minizinc/2.dzn
	docker run $(MZN_TAG) solver -p 2 /minizinc/test.mzn /minizinc/2.dzn

minizinc-challenge-bash:
	docker run -it $(MZN_TAG) /bin/bash

minizinc-challenge-export:
	docker tag $(MZN_TAG) laurentperron/$(MZN_TAG)
	docker push laurentperron/$(MZN_TAG)
