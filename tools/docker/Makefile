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
	@echo -e "\t${BOLD}python${RESET}: Build alpine and manylinux2014 python 'ortools' wheel packages (3.6+)."
	@echo -e "\t${BOLD}python_<platform>${RESET}: Build all python 'ortools' wheel packages (3.6+) for a specific platform."
	@echo -e "\t${BOLD}python_<platform>_<step>${RESET}: Build all python 'ortools' wheel packages (3.6+) for a specific platform."
	@echo -e "\t${BOLD}python_<target>_<step>${RESET}: Build python 'ortools' wheel packages (3.6+) for a specific target."
	@echo -e "\t${BOLD}save_python_<target>${RESET}: Save python 'ortools' image."
	@echo -e "\t${BOLD}sh_python_<target>${RESET}: Run a container using the python 'ortools' image."
	@echo -e "\t${BOLD}clean_python_<target>${RESET}: Clean manylinux2014 and alpine python 'ortools' wheel packages."
	@echo
	@echo -e "\t${BOLD}<platform>${RESET}:"
	@echo -e "\t\t${BOLD}amd64${RESET}"
	@echo -e "\t\t${BOLD}arm64v8${RESET}"
	@echo
	@echo -e "\t${BOLD}<target>${RESET}: ${BOLD}<platform>_<distro>${RESET}"
	@echo
	@echo -e "\t${BOLD}<distro>${RESET}:"
	@echo -e "\t\t${BOLD}alpine${RESET} (latest)"
	@echo -e "\t\t${BOLD}manylinux${RESET} (latest)"
	@echo -e "\te.g. 'make python_arm64v8_manylinux_export'"
	@echo
	@echo -e "\t${BOLD}<step>${RESET}:"
	@echo -e "\t\t${BOLD}env${RESET}"
	@echo -e "\t\t${BOLD}devel${RESET}"
	@echo -e "\t\t${BOLD}build${RESET}"
	@echo -e "\t\t${BOLD}test${RESET}"
	@echo -e "\t\t${BOLD}export${RESET}"
	@echo
	@echo
	@echo -e "\t${BOLD}amd64_<stage>${RESET}: Build <stage> docker images for ALL DISTROS."
	@echo -e "\t${BOLD}amd64_<distro>_<stage>${RESET}: Build the <stage> docker image for a specific distro."
	@echo -e "\t${BOLD}save_amd64_<stage>${RESET}: Save <stage> docker images for ALL DISTROS."
	@echo -e "\t${BOLD}save_amd64_<distro>_<stage>${RESET}: Save the <stage> docker image for a specific distro."
	@echo -e "\t${BOLD}sh_amd64_<distro>_<stage>${RESET}: Run a container using the <stage> docker image specified (debug purpose)."
	@echo -e "\t${BOLD}clean_amd64_<stage>${RESET}: Clean <stage> docker images for ALL DISTROS."
	@echo -e "\t${BOLD}clean_amd64_<distro>_<stage>${RESET}: Clean the <stage> docker image for a specific distro."
	@echo
	@echo -e "\t${BOLD}amd64_<distro>_test${RESET}: Test OR-Tools archive for ALL LANGUAGES for the specified ${BOLD}<distro>${RESET}."
	@echo -e "\t${BOLD}amd64_<distro>_test_<lang>${RESET}: Test OR-Tools archive for the specified ${BOLD}<distro>${RESET} and ${BOLD}<lang>${RESET}."
	@echo
	@echo -e "\t${BOLD}clean${RESET}: Clean all docker images but keep archives (i.e. don't touch the export directory)."
	@echo -e "\t${BOLD}distclean${RESET}: Clean all docker images and remove all archives."
	@echo
	@echo -e "\t${BOLD}<distro>${RESET}:"
	@echo -e "\t\t${BOLD}alpine-edge${RESET} (latest)"
	@echo -e "\t\t${BOLD}centos-8${RESET} (latest)"
	@echo -e "\t\t${BOLD}centos-7${RESET} (Centos 7 LTS)"
	@echo -e "\t\t${BOLD}debian-11${RESET} (bullseye, latest)"
	@echo -e "\t\t${BOLD}debian-10${RESET} (buster)"
	@echo -e "\t\t${BOLD}fedora-34${RESET} (latest)"
	@echo -e "\t\t${BOLD}fedora-33${RESET}"
	@echo -e "\t\t${BOLD}ubuntu-21.04${RESET} (rolling)"
	@echo -e "\t\t${BOLD}ubuntu-20.04${RESET} (Ubuntu 20.04 LTS, latest)"
	@echo -e "\t\t${BOLD}ubuntu-18.04${RESET} (Ubuntu 18.04 LTS)"
	@echo -e "\t\t${BOLD}ubuntu-16.04${RESET} (Ubuntu 16.04 LTS)"
	@echo
	@echo -e "\t${BOLD}<stage>${RESET}:"
	@echo -e "\t\t${BOLD}env${RESET}"
	@echo -e "\t\t${BOLD}devel${RESET}"
	@echo -e "\t\t${BOLD}third_party${RESET}"
	@echo -e "\t\t${BOLD}build${RESET}"
	@echo -e "\t\t${BOLD}archive${RESET}"
	@echo -e "\t\t${BOLD}test_<lang>${RESET}"
	@echo
	@echo -e "\t${BOLD}<lang>${RESET}: Language to build"
	@echo -e "\t\t${BOLD}cc${RESET} C++"
	@echo -e "\t\t${BOLD}java${RESET} Java (JDK 8.0) SWIG wrappers"
	@echo -e "\t\t${BOLD}dotnet${RESET} .Net Core 3.1 and .Net 5.0 SWIG wrappers"
	@echo
	@echo -e "\te.g. 'make ubuntu-18.04_archive'"
	@echo -e "\te.g. 'make sh_ubuntu-18.04_build'"
	@echo -e "\te.g. 'make ubuntu-18.04_test_cc'"
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

DOCKER_RUN_CMD := docker run --rm --init
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
# $* stem
# $< first prerequist
# $@ target name

export:
	-mkdir $@

export/python: | export
	-mkdir $@

export/python/%/build-manylinux.sh: python/build-manylinux.sh | export/python
	-mkdir -p export/python/$*
	cp $< $@

# Currently supported distro
PYTHON_DISTROS = alpine manylinux
PYTHON_STAGES = env devel build test

## AMD64
PYTHON_AMD_DISTROS = $(addprefix amd64_, $(PYTHON_DISTROS))
define make-python-stage-target =
python_targets_$1 = $(addprefix python_, $(addsuffix _$1, $(PYTHON_AMD_DISTROS)))
.PHONY: python_amd64_$1
python_amd64_$1: $(python_targets_$1)
.PHONY: $(python_targets_$1)
$$(python_targets_$1): python_amd64_%_$1: python/amd64/%.Dockerfile export/python/%/build-manylinux.sh
	@docker image rm -f ${IMAGE}:python_amd64_$$*_$1 2>/dev/null
	${DOCKER_BUILD_CMD} \
 --tag ${IMAGE}:python_amd64_$$*_$1 \
 --build-arg SRC_GIT_BRANCH=$(OR_TOOLS_BRANCH) \
 --build-arg SRC_GIT_SHA1=$(OR_TOOLS_SHA1) \
 --target=$1 \
 -f $$< \
 export/python/$$*

save_python_targets_$1 = $(addprefix save_python_, $(addsuffix _$1, $(PYTHON_AMD_DISTROS)))
.PHONY: $(save_python_targets_$1)
$$(save_python_targets_$1): save_python_amd64_%_$1: cache/python/amd64/%/docker_$1.tar
cache/python/amd64/%/docker_$1.tar: python_amd64_%_$1
	@rm -f $$@
	mkdir -p cache/python/amd64/$$*
	docker save ${IMAGE}:python_amd64_$$*_$1 -o $$@

sh_python_targets_$1 = $(addprefix sh_python_, $(addsuffix _$1, $(PYTHON_AMD_DISTROS)))
.PHONY: $(sh_python_targets_$1)
$$(sh_python_targets_$1): sh_python_amd64_%_$1: python_amd64_%_$1 | export
	${DOCKER_RUN_CMD} \
 -v `pwd`/export:/export \
 -it \
 --name ortools_python_amd64_$$*_$1 \
 ${IMAGE}:python_amd64_$$*_$1

clean_python_targets_$1 = $(addprefix clean_python_, $(addsuffix _$1, $(PYTHON_AMD_DISTROS)))
.PHONY: clean_python_amd64_$1
clean_python_amd64_$1: $(clean_python_targets_$1)
.PHONY: $(clean_python_targets_$1)
$$(clean_python_targets_$1): clean_python_amd64_%_$1:
	docker image rm -f ${IMAGE}:python_amd64_$$*_$1 2>/dev/null
	rm -f cache/python/amd64/$$*/docker_$1.tar
endef

$(foreach stage,$(PYTHON_STAGES),$(eval $(call make-python-stage-target,$(stage))))

# "python_amd64_<distro>_export" step
python_targets_export = $(addprefix python_, $(addsuffix _export, $(PYTHON_AMD_DISTROS)))
.PHONY: python_amd64
python_amd64: $(python_targets_export)
.PHONY: $(python_targets_export)
$(python_targets_export): python_amd64_%_export: python_amd64_%_build | export
	${DOCKER_RUN_CMD} \
 -v `pwd`/export:/export \
 -it ${IMAGE}:python_amd64_$*_build \
 "cp build*/python/dist/*.whl /export/python"

## ARM64v8
PYTHON_ARM_DISTROS = $(addprefix arm64v8_, $(PYTHON_DISTROS))
define make-python-arm-stage-target =
python_targets_$1 = $(addprefix python_, $(addsuffix _$1, $(PYTHON_ARM_DISTROS)))
.PHONY: python_arm64v8_$1
python_arm64v8_$1: $(python_targets_$1)
.PHONY: $(python_targets_$1)
$$(python_targets_$1): python_arm64v8_%_$1: python/arm64v8/%.Dockerfile export/python/%/build-manylinux.sh
	@docker image rm -f ${IMAGE}:python_arm64v8_$$*_$1 2>/dev/null
	${DOCKER_BUILD_CMD} \
 --tag ${IMAGE}:python_arm64v8_$$*_$1 \
 --build-arg SRC_GIT_BRANCH=$(OR_TOOLS_BRANCH) \
 --build-arg SRC_GIT_SHA1=$(OR_TOOLS_SHA1) \
 --target=$1 \
 -f $$< \
 export/python/$$*

save_python_targets_$1 = $(addprefix save_python_, $(addsuffix _$1, $(PYTHON_ARM_DISTROS)))
.PHONY: $(save_python_targets_$1)
$$(save_python_targets_$1): save_python_arm64v8_%_$1: cache/python/arm64v8/%/docker_$1.tar
cache/python/arm64v8/%/docker_$1.tar: python_arm64v8_%_$1
	@rm -f $$@
	mkdir -p cache/python/arm64v8/$$*
	docker save ${IMAGE}:python_arm64v8_$$*_$1 -o $$@

sh_python_targets_$1 = $(addprefix sh_python_, $(addsuffix _$1, $(PYTHON_ARM_DISTROS)))
.PHONY: $(sh_python_targets_$1)
$$(sh_python_targets_$1): sh_python_arm64v8_%_$1: python_arm64v8_%_$1 | export
	${DOCKER_RUN_CMD} \
 -v `pwd`/export:/export \
 -it \
 --name ortools_python_arm64v8_$$*_$1 \
 ${IMAGE}:python_arm64v8_$$*_$1

clean_python_targets_$1 = $(addprefix clean_python_, $(addsuffix _$1, $(PYTHON_ARM_DISTROS)))
.PHONY: clean_python_arm64v8_$1
clean_python_arm64v8_$1: $(clean_python_targets_$1)
.PHONY: $(clean_python_targets_$1)
$$(clean_python_targets_$1): clean_python_arm64v8_%_$1:
	docker image rm -f ${IMAGE}:python_arm64v8_$$*_$1 2>/dev/null
	rm -f cache/python/arm64v8/$$*/docker_$1.tar
endef

$(foreach stage,$(PYTHON_STAGES),$(eval $(call make-python-arm-stage-target,$(stage))))

# "python_arm64v8_<distro>_export" step
python_arm_targets_export = $(addprefix python_, $(addsuffix _export, $(PYTHON_ARM_DISTROS)))
.PHONY: python_arm64v8
python_arm64v8: $(python_arm_targets_export)
.PHONY: $(python_arm_targets_export)
$(python_arm_targets_export): python_arm64v8_%_export: python_arm64v8_%_build | export
	${DOCKER_RUN_CMD} \
 -v `pwd`/export:/export \
 -it ${IMAGE}:python_arm64v8_$*_build \
 "cp build*/python/dist/*.whl /export/python"

## MERGE ##
.PHONY: python
python: python_amd64 python_arm64v8

#################
###  ARCHIVES  ##
#################
# $* stem
# $< first prerequist
# $@ target name

# Currently supported distro
DISTROS = \
 alpine-edge \
 centos-7 centos-8 \
 debian-10 debian-11 \
 fedora-33 fedora-34 \
 ubuntu-16.04 ubuntu-18.04 ubuntu-20.04 ubuntu-21.04
STAGES = env devel third_party build

export/%/or-tools.snk: or-tools.snk | export
	-mkdir -p export/$*
	cp $< $@

# generic rule export/% prevent other rules
# e.g. export/%/docker.devel.tar -> need an exhaustive list
export/archives: | export
	-mkdir $@

define make-stage-target =
targets_$1 = $(addprefix amd64_, $(addsuffix _$1, $(DISTROS)))
.PHONY: amd64_$1
amd64_$1: $$(targets_$1)
.PHONY: $(targets_$1)
$$(targets_$1): amd64_%_$1: amd64/%.Dockerfile | export/%/or-tools.snk
	#@docker image rm -f ${IMAGE}:amd64_$$*_$1 2>/dev/null
	${DOCKER_BUILD_CMD} \
 --tag ${IMAGE}:amd64_$$*_$1 \
 --build-arg SRC_GIT_BRANCH=$(OR_TOOLS_BRANCH) \
 --build-arg SRC_GIT_SHA1=$(OR_TOOLS_SHA1) \
 --target=$1 \
 -f $$< \
 export/$$*

save_targets_$1 = $(addprefix save_amd64_, $(addsuffix _$1, $(DISTROS)))
.PHONY: save_amd64_$1
save_amd64_$1: $$(save_targets_$1)
.PHONY: $(save_targets_$1)
$$(save_targets_$1): save_amd64_%_$1: cache/amd64/%/docker_$1.tar
cache/amd64/%/docker_$1.tar: amd64_%_$1
	@rm -f $$@
	mkdir -p cache/amd64/$$*
	docker save ${IMAGE}:amd64_$$*_$1 -o $$@

sh_targets_$1 = $(addprefix sh_amd64_, $(addsuffix _$1, $(DISTROS)))
.PHONY: $(sh_targets_$1)
$$(sh_targets_$1): sh_amd64_%_$1: amd64_%_$1 | export
	${DOCKER_RUN_CMD} \
 -v `pwd`/export:/export \
 -it \
 --name ortools_amd64_$$*_$1 \
 ${IMAGE}:amd64_$$*_$1

clean_targets_$1 = $(addprefix clean_amd64_, $(addsuffix _$1, $(DISTROS)))
.PHONY: clean_amd64_$1
clean_amd64_$1: $$(clean_targets_$1)
.PHONY: $(clean_targets_$1)
$$(clean_targets_$1): clean_amd64_%_$1:
	docker image rm -f ${IMAGE}:amd64_$$*_$1 2>/dev/null
	rm -f cache/amd64/$$*/docker_$1.tar
endef

$(foreach stage,$(STAGES),$(eval $(call make-stage-target,$(stage))))

# Build Archives
targets = $(addprefix amd64_, $(addsuffix _archive, $(DISTROS)))
.PHONY: amd64_archives
amd64_archives: $(targets)
.PHONY: $(targets)
$(targets): amd64_%_archive: \
 export/archives/or-tools_amd64_%_v$(OR_TOOLS_VERSION).tar.gz \
 export/archives/or-tools_amd64_flatzinc_%_v$(OR_TOOLS_VERSION).tar.gz

export/archives/or-tools_amd64_%_v$(OR_TOOLS_VERSION).tar.gz: amd64_%_build | export/archives
	-rm -f export/archives/or-tools_amd64_$*_v*.tar.gz
	$(DOCKER_RUN_CMD) \
 -w /root/or-tools \
 -v `pwd`/export:/export \
 $(IMAGE):amd64_$*_build \
 "make archive && make test_archive && cp *.tar.gz /export/$*"
	mv export/$*/or-tools_*.tar.gz $@

export/archives/or-tools_amd64_flatzinc_%_v$(OR_TOOLS_VERSION).tar.gz: amd64_%_build | export/archives
	-rm -f export/archives/or-tools_amd64_flatzinc_$*_v*.tar.gz
	$(DOCKER_RUN_CMD) \
 -w /root/or-tools \
 -v `pwd`/export:/export \
 $(IMAGE):amd64_$*_build \
 "make fz_archive && make test_fz_archive && cp *.tar.gz /export/$*"
	mv export/$*/or-tools_flatzinc_*.tar.gz $@

## MERGE ##
.PHONY: archives
archives: amd64_archives #arm64v8_archives

############
##  TEST  ##
############
LANGS = cc java dotnet

define make-test-target =
targets_$1 = $(addprefix amd64_, $(addsuffix _test_$1, $(DISTROS)))
.PHONY: amd64_test_$1
amd64_test_$1: $$(targets_$1)
.PHONY: $(targets_$1)
$$(targets_$1): amd64_%_test_$1: test/amd64/%/$1.Dockerfile amd64_%_archive
	#@docker image rm -f $(IMAGE):amd64_$$*_test_$1 2>/dev/null
	$(DOCKER_BUILD_CMD) \
 --tag $(IMAGE):amd64_$$*_test_$1 \
 -f $$< \
 export/archives

save_targets_test_$1 = $(addprefix save_amd64_, $(addsuffix _test_$1, $(DISTROS)))
.PHONY: save_amd64_test_$1
save_amd64_test_$1: $$(save_targets_test_$1)
.PHONY: $(save_targets_test_$1)
$$(save_targets_test_$1): save_amd64_%_test_$1: cache/amd64/%/docker_test_$1.tar
cache/amd64/%/docker_test_$1.tar: amd64_%_test_$1
	@rm -f $$@
	mkdir -p cache/amd64/$$*
	docker save ${IMAGE}:amd64_$$*_test_$1 -o $$@

sh_targets_$1 = $(addprefix sh_amd64_, $(addsuffix _test_$1, $(DISTROS)))
.PHONY: $(sh_targets_$1)
$$(sh_targets_$1): sh_amd64_%_test_$1: %_$1
	${DOCKER_RUN_CMD} \
 -v `pwd`/export:/export \
 -it \
 --name ortools_amd64_$$*_$1 \
 ${IMAGE}:amd64_$$*_test_$1

clean_targets_$1 = $(addprefix clean_amd64_, $(addsuffix _test_$1, $(DISTROS)))
.PHONY: clean_amd64_$1
clean_amd64_$1: $$(clean_targets_$1)
.PHONY: $(clean_targets_$1)
$$(clean_targets_$1): clean_amd64_%_test_$1:
	docker image rm -f ${IMAGE}:amd64_$$*_test_$1 2>/dev/null
	rm -f cache/amd64/$$*/docker_test_$1.tar
endef

$(foreach lang,$(LANGS),$(eval $(call make-test-target,$(lang))))

targets = $(addprefix amd64_, $(addsuffix _test, $(DISTROS)))
.PHONY: amd64_test
amd64_test: $(targets)
.PHONY: $(targets)
$(targets): amd64_%_test: $(addprefix amd64_%_test_, $(LANGS))

targets = $(addprefix clean_amd64_, $(addsuffix _test, $(DISTROS)))
.PHONY: clean_amd64_test
clean_amd64_test: $(targets)
.PHONY: $(targets)
$(targets): clean_amd64_%_test: $(addprefix clean_amd64_%_test_, $(LANGS))

## MERGE ##
.PHONY: test
test: amd64_test #arm64v8_test

#############
##  CLEAN  ##
#############
clean_targets = $(addprefix clean_, $(DISTROS))

.PHONY: $(clean_targets)
$(clean_targets): clean_%: $(addprefix clean_amd64_%_, $(STAGES)) clean_amd64_%_test
	-rmdir cache/$*
	-rm -f export/archives/*.tar.gz
	-rm -f export/$*/or-tools.snk
	-rmdir export/$*

.PHONY: clean_python
clean_python: $(addprefix clean_python_amd64_, $(PYTHON_STAGES)) $(addprefix clean_python_arm64v8_, $(PYTHON_STAGES))
	-rm -rf cache/python/*
	-rm -f export/python/*.whl
	-rm -f export/python/*.tar
	-rm -f export/python/*.sh
	-rmdir export/python

.PHONY: clean
clean: $(clean_targets) clean_python
	docker container prune -f
	docker image prune -f
	-rm -rf cache

.PHONY: distclean
distclean: clean | export/archives
	-docker container rm -f $$(docker container ls -f status=exited -q)
	-docker image rm -f $$(docker image ls --all -q)
	-rm -rf export/archives
	-rm -rf export

##########################
##  MINIZINC CHALLENGE  ##
##########################
MZN_TAG=or-tools-minizinc-challenge:2021v4

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
