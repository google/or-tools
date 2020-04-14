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
	@echo -e "\t${BOLD}python${RESET}: Build manylinux2010 python 'ortools' wheel packages (3.5, 3.6, 3.7)."
	@echo -e "\t${BOLD}test_python${RESET}: Test manylinux2010 python 'ortools' wheel packages (3.5, 3.6, 3.7)."
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
	@echo -e "\t\t${BOLD}centos-8${RESET} (latest)"
	@echo -e "\t\t${BOLD}debian-10${RESET} (latest)"
	@echo -e "\t\t${BOLD}ubuntu-19.04${RESET} (Ubuntu latest)"
	@echo -e "\t\t${BOLD}ubuntu-18.04${RESET} (Ubuntu 18.04 LTS)"
	@echo -e "\t\t${BOLD}ubuntu-16.04${RESET} (Ubuntu 16.04 LTS)"
	@echo
	@echo -e "\t${BOLD}<stage>${RESET}:"
	@echo -e "\t\t${BOLD}env${RESET}"
	@echo -e "\t\t${BOLD}devel${RESET}"
	@echo -e "\t\t${BOLD}third_party${RESET}"
	@echo -e "\t\t${BOLD}build${RESET}"
	@echo
	@echo -e "\t${BOLD}<lang>${RESET}: Language to build"
	@echo -e "\t\t${BOLD}cc${RESET} C++"
	@echo -e "\t\t${BOLD}java${RESET} Java (JDK 8.0) SWIG wrappers"
	@echo -e "\t\t${BOLD}dotnet${RESET} .Net Standard 2.0 SWIG wrappers"
	@echo -e "\t\t${BOLD}all${RESET} all"
	@echo -e "\te.g. 'make ubuntu_test_cc'"
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

# Docker image name prefix.
IMAGE := or-tools/docker

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
export:
	-mkdir $@

export/python: | export
	-mkdir $@

export/python/build-manylinux1.sh: build-manylinux1.sh | export/python
	cp $< $@

.PHONY: docker_python
docker_python: manylinux1.Dockerfile export/python/build-manylinux1.sh ../../makefiles
	#@docker image rm -f $(IMAGE):python 2>/dev/null
	$(DOCKER_BUILD_CMD) \
 --tag $(IMAGE):python \
 --build-arg SRC_GIT_BRANCH=$(OR_TOOLS_BRANCH) \
 --build-arg SRC_GIT_SHA1=$(OR_TOOLS_SHA1) \
 -f $< \
 export/python

save_docker_python: export/python/docker.tar
export/python/docker.tar: docker_python
	-rm -f $@
	docker save $(IMAGE):python -o $@

bash_python: docker_python
	$(DOCKER_RUN_CMD) -v `pwd`/export:/export $(IMAGE):python /bin/bash

.PHONY: python
python: docker_python
	$(DOCKER_RUN_CMD) -v `pwd`/export:/export $(IMAGE):python /bin/bash -c \
 "/root/build/build-manylinux1.sh /root/src /root/build /export/python"

#################
###  ARCHIVES  ##
#################
# $* stem
# $< first prerequist
# $@ target name

# Currently supported distro
DISTROS = centos-8 debian-10 ubuntu-19.10 ubuntu-18.04 ubuntu-16.04
STAGES = env devel third_party build

export/%/or-tools.snk: or-tools.snk | export
	-mkdir -p export/$*
	cp or-tools.snk $@

define make-stage-target
.PHONY: $1
#$$(info Create targets: $1 $(addsuffix _$1, $(DISTROS)).)
targets_$1 = $(addsuffix _$1, $(DISTROS))
.PHONY: $(targets_$1)
$1: $$(targets_$1)
$$(targets_$1): %_$1: %.Dockerfile | export/%/or-tools.snk
	#@docker image rm -f ${IMAGE}:$$*_$1 2>/dev/null
	${DOCKER_BUILD_CMD} \
		--tag ${IMAGE}:$$*_$1 \
    --build-arg SRC_GIT_BRANCH=$(OR_TOOLS_BRANCH) \
    --build-arg SRC_GIT_SHA1=$(OR_TOOLS_SHA1) \
		--target=$1 \
		-f $$< \
		export/$$*

#$$(info Create targets: save_$1 $(addprefix save_, $(addsuffix _$1, $(DISTROS))) (debug).)
save_targets_$1 = $(addprefix save_, $(addsuffix _$1, $(DISTROS)))
.PHONY: save_$1 $(save_targets_$1)
save_$1: $$(save_targets_$1)
$$(save_targets_$1): save_%_$1: cache/%/docker_$1.tar
cache/%/docker_$1.tar: %_$1
	@rm -f $$@
	mkdir -p cache/$$*
	docker save ${IMAGE}:$$*_$1 -o $$@

#$$(info Create targets: $(addprefix sh_, $(addsuffix _$1, $(DISTROS))) (debug).)
sh_targets_$1 = $(addprefix sh_, $(addsuffix _$1, $(DISTROS)))
.PHONY: $(sh_targets_$1)
$$(sh_targets_$1): sh_%_$1: %_$1
	${DOCKER_RUN_CMD} -v `pwd`/export:/export -it --name ${IMAGE}_$$*_$1 ${IMAGE}:$$*_$1 /bin/sh

#$$(info Create targets: $(addprefix clean_, $(addsuffix _$1, $(DISTROS))).)
clean_targets_$1 = $(addprefix clean_, $(addsuffix _$1, $(DISTROS)))
.PHONY: clean_$1 $(clean_targets_$1)
clean_$1: $$(clean_targets_$1)
$$(clean_targets_$1): clean_%_$1:
	docker image rm -f ${IMAGE}:$$*_$1 2>/dev/null
	rm -f cache/$$*/docker_$1.tar
endef

$(foreach stage,$(STAGES),$(eval $(call make-stage-target,$(stage))))

# Build Archives
targets = $(addsuffix _archive, $(DISTROS))
.PHONY: archives $(targets)
archives: $(targets)
$(targets): %_archive: export/archives/or-tools_%_v$(OR_TOOLS_VERSION).tar.gz

export/archives/or-tools_%_v$(OR_TOOLS_VERSION).tar.gz: %_build | export/archives
	-rm -f export/archives/or-tools_$*_v*.tar.gz
	-rm -f export/archives/or-tools_flatzinc_$*_v*.tar.gz
	$(DOCKER_RUN_CMD) -w /root/or-tools -v `pwd`/export:/export $(IMAGE):$*_build /bin/sh -c \
 "make archive && make test_archive && cp *.tar.gz /export/$*"
	$(DOCKER_RUN_CMD) -w /root/or-tools -v `pwd`/export:/export $(IMAGE):$*_build /bin/sh -c \
 "make fz_archive && make test_fz_archive && cp *.tar.gz /export/$*"
	mv export/$*/or-tools_flatzinc_*.tar.gz export/archives/or-tools_flatzinc_$*_v$(OR_TOOLS_VERSION).tar.gz
	mv export/$*/or-tools_*.tar.gz export/archives/or-tools_$*_v$(OR_TOOLS_VERSION).tar.gz

# generic rule export/% prevent other rules
# e.g. export/%/docker.devel.tar -> need an exhaustive list
export/archives: | export
	-mkdir $@

############
##  TEST  ##
############
# Test Archive
targets = $(addsuffix _test_cc, $(DISTROS))
.PHONY: test_cc $(targets)
test_cc: $(targets)
$(targets): %_test_cc: test/%/cc.Dockerfile %_archive
	#@docker image rm -f $(IMAGE):$*_cc 2>/dev/null
	$(DOCKER_BUILD_CMD) \
		--tag $(IMAGE):$*_cc \
		-f $< \
		export/archives
	$(DOCKER_RUN_CMD) $(IMAGE):$*_cc /bin/sh -c "cd or-tools_*_v* && make test_cc"
	@date > $@

targets = $(addsuffix _test_java, $(DISTROS))
.PHONY: test_java $(targets)
test_java: $(targets)
$(targets): %_test_java: test/%/java.Dockerfile %_archive
	#@docker image rm -f $(IMAGE):$*_java 2>/dev/null
	$(DOCKER_BUILD_CMD) \
		--tag $(IMAGE):$*_java \
		-f $< \
		export/archives
	$(DOCKER_RUN_CMD) $(IMAGE):$*_java /bin/sh -c "cd or-tools_*_v* && make test_java"
	@date > $@

targets = $(addsuffix _test_dotnet, $(DISTROS))
.PHONY: test_dotnet $(targets)
test_dotnet: $(targets)
$(targets): %_test_dotnet: test/%/dotnet.Dockerfile %_archive
	#@docker image rm -f $(IMAGE):$*_dotnet 2>/dev/null
	$(DOCKER_BUILD_CMD) \
		--tag $(IMAGE):$*_dotnet \
		-f $< \
		export/archives
	$(DOCKER_RUN_CMD) $(IMAGE):$*_dotnet /bin/sh -c "cd or-tools_*_v* && make test_dotnet"
	@date > $@

targets = $(addsuffix _test, $(DISTROS))
.PHONY: test_archives $(targets)
$(targets): %_test: %_test_cc %_test_java %_test_dotnet
test_archives: $(targets)

#############
##  CLEAN  ##
#############
targets = $(addprefix clean_, $(DISTROS))

.PHONY: $(targets)
$(targets): clean_%: $(addprefix clean_%_, $(STAGES))
	docker image rm -f ${IMAGE}:$*_cc 2>/dev/null
	docker image rm -f ${IMAGE}:$*_java 2>/dev/null
	docker image rm -f ${IMAGE}:$*_dotnet 2>/dev/null
	-rmdir cache/$*
	-rm -f export/archives/or-tools_flatzinc_$*_v*.tar.gz
	-rm -f export/archives/or-tools_$*_v*.tar.gz
	-rm -f export/$*/or-tools.snk
	-rmdir export/$*

.PHONY: clean_python
clean_python:
	-docker image rm -f $(IMAGE):python 2>/dev/null
	-rm -f export/python/ortools-*.whl
	-rm -f export/python/docker.tar
	-rm -f export/python/build-manylinux*.sh
	-rmdir export/python

.PHONY: clean
clean: $(targets) clean_python
	docker container prune -f
	docker image prune -f
	-rmdir cache

.PHONY: distclean
distclean: clean | export/archives
	-docker container rm -f $$(docker container ls -f status=exited -q)
	-docker image rm -f $$(docker image ls --all -q)
	rmdir export/archives
	rmdir export

##########################
##  MINIZINC CHALLENGE  ##
##########################
MZN_TAG=or-tools-minizinc-challenge:2019v3

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
