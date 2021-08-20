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
	@echo -e "\t${BOLD}python${RESET}: Build manylinux2010 and alpine python 'ortools' wheel packages (3.6+)."
	@echo -e "\t${BOLD}python_<target>${RESET}: Build python 'ortools' wheel packages (3.6+) for a specific target."
	@echo -e "\t${BOLD}test_python${RESET}: Test manylinux2010 and alpine python 'ortools' wheel packages (3.6+)."
	@echo -e "\t${BOLD}clean_python${RESET}: Clean manylinux2010 and alpine python 'ortools' wheel packages."
	@echo
	@echo -e "\t${BOLD}<target>${RESET}:"
	@echo -e "\t\t${BOLD}manylinux${RESET} (latest)"
	@echo -e "\t\t${BOLD}alpine${RESET} (latest)"
	@echo
	@echo -e "\t${BOLD}<stage>${RESET}: build <stage> docker images for ALL DISTROS."
	@echo -e "\t${BOLD}<distro>_<stage>${RESET}: build the <stage> docker image for a specific distro."
	@echo -e "\t${BOLD}save_<stage>${RESET}: Save <stage> docker images for ALL DISTROS."
	@echo -e "\t${BOLD}save_<distro>_<stage>${RESET}: Save the <stage> docker image for a specific distro."
	@echo -e "\t${BOLD}sh_<distro>_<stage>${RESET}: run a container using the <stage> docker image specified (debug purpose)."
	@echo
	@echo -e "\t${BOLD}<distro>_test${RESET}: Test OR-Tools archive for ALL LANGUAGES for the specified ${BOLD}<distro>${RESET}."
	@echo -e "\t${BOLD}<distro>_test_<lang>${RESET}: Test OR-Tools archive for the specified ${BOLD}<distro>${RESET} and ${BOLD}<lang>${RESET}."
	@echo
	@echo -e "\t${BOLD}clean${RESET}: Clean all docker images but keep archives (i.e. don't touch the export directory)."
	@echo -e "\t${BOLD}distclean${RESET}: Clean all docker images and remove all archives."
	@echo
	@echo -e "\t${BOLD}<distro>${RESET}:"
	@echo -e "\t\t${BOLD}alpine-edge${RESET} (latest)"
	@echo -e "\t\t${BOLD}centos-7${RESET} (Centos 7 LTS)"
	@echo -e "\t\t${BOLD}centos-8${RESET} (latest)"
	@echo -e "\t\t${BOLD}debian-10${RESET} (latest)"
	@echo -e "\t\t${BOLD}fedora-33${RESET} (latest)"
	@echo -e "\t\t${BOLD}ubuntu-20.10${RESET} (Ubuntu latest)"
	@echo -e "\t\t${BOLD}ubuntu-20.04${RESET} (Ubuntu 20.04 LTS)"
	@echo -e "\t\t${BOLD}ubuntu-18.04${RESET} (Ubuntu 18.04 LTS)"
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
	@echo -e "\t\t${BOLD}dotnet${RESET} .Net Standard 2.0 SWIG wrappers"
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
export:
	-mkdir $@

export/python: | export
	-mkdir $@

export/python/build-manylinux1.sh: build-manylinux1.sh | export/python
	cp $< $@

TARGETS = manylinux alpine
targets = $(addprefix python_, $(TARGETS))

.PHONY: python
python: $(targets)

.PHONY: python_manylinux
python_manylinux: ortools_python_manylinux
	$(DOCKER_RUN_CMD) \
 -v `pwd`/export:/export \
 -it $< \
 /bin/bash -c \
 "/root/build/build-manylinux1.sh /root/src /root/build /export/python"

.PHONY: ortools_python_manylinux
ortools_python_manylinux: manylinux.Dockerfile export/python/build-manylinux1.sh ../../makefiles
	#@docker image rm -f $@ 2>/dev/null
	$(DOCKER_BUILD_CMD) \
 --tag $@ \
 --build-arg SRC_GIT_BRANCH=$(OR_TOOLS_BRANCH) \
 --build-arg SRC_GIT_SHA1=$(OR_TOOLS_SHA1) \
 --target=build \
 -f $< \
 export/python

.PHONY: python_alpine
python_alpine: ortools_python_alpine
	$(DOCKER_RUN_CMD) \
 -v `pwd`/export:/export \
 -it $< \
 /bin/sh -c \
 "cp *.whl /export/python"

.PHONY: ortools_python_alpine
ortools_python_alpine: alpine.Dockerfile ../../makefiles | export/python
	#@docker image rm -f $@ 2>/dev/null
	$(DOCKER_BUILD_CMD) \
 --tag $@ \
 --build-arg SRC_GIT_BRANCH=$(OR_TOOLS_BRANCH) \
 --build-arg SRC_GIT_SHA1=$(OR_TOOLS_SHA1) \
 --target=build \
 -f $< \
 export/python

.PHONY: save_docker_python
save_docker_python: export/python/manylinux.tar export/python/alpine.tar

export/python/%.tar: ortools_python_%
	-rm -f $@
	docker save $< -o $@

targets = $(addprefix bash_python_, $(TARGETS))
$(targets): bash_python_%: ortools_python_%
	$(DOCKER_RUN_CMD) -v `pwd`/export:/export -it --name ortools_python $<

#################
###  ARCHIVES  ##
#################
# $* stem
# $< first prerequist
# $@ target name

# Currently supported distro
DISTROS = alpine-edge centos-7 centos-8 debian-10 fedora-33 ubuntu-18.04 ubuntu-20.04 ubuntu-20.10
STAGES = env devel third_party build

export/%/or-tools.snk: or-tools.snk | export
	-mkdir -p export/$*
	cp or-tools.snk $@

define make-stage-target
targets_$1 = $(addsuffix _$1, $(DISTROS))
.PHONY: $1
$1: $$(targets_$1)
.PHONY: $(targets_$1)
$$(targets_$1): %_$1: %.Dockerfile | export/%/or-tools.snk
	#@docker image rm -f ${IMAGE}:$$*_$1 2>/dev/null
	${DOCKER_BUILD_CMD} \
		--tag ${IMAGE}:$$*_$1 \
    --build-arg SRC_GIT_BRANCH=$(OR_TOOLS_BRANCH) \
    --build-arg SRC_GIT_SHA1=$(OR_TOOLS_SHA1) \
		--target=$1 \
		-f $$< \
		export/$$*

save_targets_$1 = $(addprefix save_, $(addsuffix _$1, $(DISTROS)))
.PHONY: save_$1
save_$1: $$(save_targets_$1)
.PHONY: $(save_targets_$1)
$$(save_targets_$1): save_%_$1: cache/%/docker_$1.tar
cache/%/docker_$1.tar: %_$1
	@rm -f $$@
	mkdir -p cache/$$*
	docker save ${IMAGE}:$$*_$1 -o $$@

sh_targets_$1 = $(addprefix sh_, $(addsuffix _$1, $(DISTROS)))
.PHONY: $(sh_targets_$1)
$$(sh_targets_$1): sh_%_$1: %_$1
	${DOCKER_RUN_CMD} -v `pwd`/export:/export -it --name ortools_$$*_$1 ${IMAGE}:$$*_$1

clean_targets_$1 = $(addprefix clean_, $(addsuffix _$1, $(DISTROS)))
.PHONY: clean_$1
clean_$1: $$(clean_targets_$1)
.PHONY: $(clean_targets_$1)
$$(clean_targets_$1): clean_%_$1:
	docker image rm -f ${IMAGE}:$$*_$1 2>/dev/null
	rm -f cache/$$*/docker_$1.tar
endef

$(foreach stage,$(STAGES),$(eval $(call make-stage-target,$(stage))))

# Build Archives
targets = $(addsuffix _archive, $(DISTROS))
.PHONY: archives
archives: $(targets)
.PHONY: $(targets)
$(targets): %_archive: \
 export/archives/or-tools_%_v$(OR_TOOLS_VERSION).tar.gz \
 export/archives/or-tools_flatzinc_%_v$(OR_TOOLS_VERSION).tar.gz

export/archives/or-tools_%_v$(OR_TOOLS_VERSION).tar.gz: %_build | export/archives
	-rm -f export/archives/or-tools_$*_v*.tar.gz
	$(DOCKER_RUN_CMD) -w /root/or-tools -v `pwd`/export:/export $(IMAGE):$*_build \
 "make archive && make test_archive && cp *.tar.gz /export/$*"
	mv export/$*/or-tools_*.tar.gz $@

export/archives/or-tools_flatzinc_%_v$(OR_TOOLS_VERSION).tar.gz: %_build | export/archives
	-rm -f export/archives/or-tools_flatzinc_$*_v*.tar.gz
	$(DOCKER_RUN_CMD) -w /root/or-tools -v `pwd`/export:/export $(IMAGE):$*_build \
 "make fz_archive && make test_fz_archive && cp *.tar.gz /export/$*"
	mv export/$*/or-tools_flatzinc_*.tar.gz $@

# generic rule export/% prevent other rules
# e.g. export/%/docker.devel.tar -> need an exhaustive list
export/archives: | export
	-mkdir $@

############
##  TEST  ##
############
LANGS = cc java dotnet

define make-test-target
targets_$1 = $(addsuffix _test_$1, $(DISTROS))
.PHONY: test_$1
test_$1: $$(targets_$1)
.PHONY: $(targets_$1)
$$(targets_$1): %_test_$1: test/%/$1.Dockerfile %_archive
	#@docker image rm -f $(IMAGE):$$*_test_$1 2>/dev/null
	$(DOCKER_BUILD_CMD) \
		--tag $(IMAGE):$$*_test_$1 \
		-f $$< \
		export/archives

save_targets_test_$1 = $(addprefix save_, $(addsuffix _test_$1, $(DISTROS)))
.PHONY: save_test_$1
save_test_$1: $$(save_targets_test_$1)
.PHONY: $(save_targets_test_$1)
$$(save_targets_test_$1): save_%_test_$1: cache/%/docker_test_$1.tar
cache/%/docker_test_$1.tar: %_test_$1
	@rm -f $$@
	mkdir -p cache/$$*
	docker save ${IMAGE}:$$*_test_$1 -o $$@

sh_targets_$1 = $(addprefix sh_, $(addsuffix _test_$1, $(DISTROS)))
.PHONY: $(sh_targets_$1)
$$(sh_targets_$1): sh_%_test_$1: %_$1
	${DOCKER_RUN_CMD} -v `pwd`/export:/export -it --name ortools_$$*_$1 ${IMAGE}:$$*_test_$1

clean_targets_$1 = $(addprefix clean_, $(addsuffix _test_$1, $(DISTROS)))
.PHONY: clean_$1
clean_$1: $$(clean_targets_$1)
.PHONY: $(clean_targets_$1)
$$(clean_targets_$1): clean_%_test_$1:
	docker image rm -f ${IMAGE}:$$*_test_$1 2>/dev/null
	rm -f cache/$$*/docker_test_$1.tar
endef

$(foreach lang,$(LANGS),$(eval $(call make-test-target,$(lang))))

targets = $(addsuffix _test, $(DISTROS))
.PHONY: $(targets)
$(targets): %_test: %_test_cc %_test_java %_test_dotnet
.PHONY: test_archives
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
	-docker image rm -f ortools_python_manylinux 2>/dev/null
	-docker image rm -f ortools_python_alpine 2>/dev/null
	-rm -f export/python/ortools-*.whl
	-rm -f export/python/manylinux.tar
	-rm -f export/python/alpine.tar
	-rm -f export/python/build-manylinux*.sh
	-rmdir export/python

.PHONY: clean
clean: $(targets) clean_cc clean_java clean_dotnet clean_python
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
MZN_TAG=or-tools-minizinc-challenge:2021v3

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
