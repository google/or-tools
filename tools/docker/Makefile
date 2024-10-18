# Copyright 2010-2024 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# General commands
.PHONY: help
BOLD=\e[1m
RESET=\e[0m

help:
	@echo -e "${BOLD}SYNOPSIS${RESET}"
	@echo -e "\tmake <TARGET> [NOCACHE=1]"
	@echo
	@echo -e "${BOLD}DESCRIPTION${RESET}"
	@echo -e "\tTools to generate various deliveries for linux distros"
	@echo
	@echo -e "${BOLD}MAIN TARGETS${RESET}"
	@echo -e "\t${BOLD}help${RESET}: display this help and exit."
	@echo
	@echo -e "\t${BOLD}delivery${RESET}: Build ${BOLD}archives${RESET} and ${BOLD}python${RESET} targets."
	@echo -e "\t${BOLD}test_delivery${RESET}: Build ${BOLD}test_archives${RESET} and ${BOLD}test_python${RESET} targets."
	@echo
	@echo -e "\t${BOLD}archives${RESET}: Build all OR-Tools archives in export."
	@echo -e "\t${BOLD}test_archives${RESET}: Test each OR-Tools archives for all ${BOLD}<distro>${RESET} and ${BOLD}<lang>${RESET}."
	@echo
	@echo -e "${BOLD}PYTHON TARGETS${RESET}"
	@echo -e "\t${BOLD}python${RESET}: Build alpine and manylinux2014 python 'ortools' wheel packages (3.8+)."
	@echo -e "\t${BOLD}python_<platform>${RESET}: Build all python 'ortools' wheel packages (3.8+) for a specific platform."
	@echo -e "\t${BOLD}python_<platform>_<step>${RESET}: Build all python 'ortools' wheel packages (3.8+) for a specific platform."
	@echo -e "\t${BOLD}python_<target>_<step>${RESET}: Build python 'ortools' wheel packages (3.8+) for a specific target."
	@echo -e "\t${BOLD}save_python_<target>${RESET}: Save python 'ortools' image."
	@echo -e "\t${BOLD}clean_python_<target>${RESET}: Clean manylinux2014 and alpine python 'ortools' wheel packages."
	@echo -e "\t${BOLD}sh_python_<target>${RESET}: Run a container using the python 'ortools' image."
	@echo
	@echo -e "\t${BOLD}<platform>${RESET}:"
	@echo -e "\t\t${BOLD}amd64${RESET}"
	@echo -e "\t\t${BOLD}arm64v8${RESET}"
	@echo
	@echo -e "\t${BOLD}<target>${RESET}:"
	@echo -e "\t\t${BOLD}<platform>_<distro>${RESET}"
	@echo -e "\t\t${BOLD}<platform>_manylinux_cp<version>${RESET}"
	@echo
	@echo -e "\t${BOLD}<distro>${RESET}:"
	@echo -e "\t\t${BOLD}alpine${RESET} (latest)"
	@echo -e "\t\t${BOLD}manylinux${RESET} (manylinux2014)"
	@echo
	@echo -e "\t${BOLD}<version>${RESET}:"
	@echo -e "\t\t${BOLD}38${RESET} Python3.8"
	@echo -e "\t\t${BOLD}39${RESET} Python3.9"
	@echo -e "\t\t${BOLD}310${RESET} Python3.10"
	@echo -e "\t\t${BOLD}311${RESET} Python3.11"
	@echo -e "\t\t${BOLD}312${RESET} Python3.12"
	@echo -e "\t\t${BOLD}313${RESET} Python3.13"
	@echo
	@echo -e "\t${BOLD}<step>${RESET}:"
	@echo -e "\t\t${BOLD}env${RESET}"
	@echo -e "\t\t${BOLD}devel${RESET}"
	@echo -e "\t\t${BOLD}build${RESET}"
	@echo -e "\t\t${BOLD}test${RESET}"
	@echo -e "\t\t${BOLD}export${RESET}"
	@echo -e "\te.g. 'make python_amd64_manylinux_cp39_export'"
	@echo -e "\te.g. 'make python_arm64v8_alpine_export'"
	@echo
	@echo
	@echo -e "${BOLD}ARCHIVE TARGETS${RESET}"
	@echo -e "\t${BOLD}<platform>_<stage>${RESET}: Build <stage> docker images for ALL DISTROS and ALL LANGUAGES."
	@echo -e "\t${BOLD}<target>${RESET}: Build the docker image for a specific platform,distro,lang,stage."
	@echo -e "\t${BOLD}save_<target>${RESET}: Save <stage> docker images for ALL DISTROS."
	@echo -e "\t${BOLD}sh_<target>${RESET}: Run a container using the docker image specified (debug purpose)."
	@echo -e "\t${BOLD}clean_<target>${RESET}: Clean the docker image specified."
	@echo
	@echo -e "\t${BOLD}clean${RESET}: Clean all docker images but keep archives (i.e. don't touch the export directory)."
	@echo -e "\t${BOLD}distclean${RESET}: Clean all docker images and remove all archives."
	@echo
	@echo -e "\t${BOLD}<target>${RESET}:"
	@echo -e "\t\t${BOLD}<platform>_<distro>_<stage>${RESET}"
	@echo
	@echo -e "\t${BOLD}<platform>${RESET}:"
	@echo -e "\t\t${BOLD}amd64${RESET}"
	@echo -e "\t\t${BOLD}arm64${RESET}"
#	@echo -e "\t\t${BOLD}riscv64 (RISC-V 64bits, experimental)${RESET}"
	@echo
	@echo -e "\t${BOLD}<distro>${RESET}:"
	@echo -e "\t\t${BOLD}almalinux-9${RESET} (latest)"
	@echo -e "\t\t${BOLD}alpine-edge${RESET} (latest)"
	@echo -e "\t\t${BOLD}archlinux${RESET} (latest)"
	@echo -e "\t\t${BOLD}debian-sid${RESET} (unstable)"
#	@echo -e "\t\t${BOLD}debian-13${RESET} (Trixie)"
	@echo -e "\t\t${BOLD}debian-12${RESET} (Bookworm)"
	@echo -e "\t\t${BOLD}debian-11${RESET} (Bullseye)"
#	@echo -e "\t\t${BOLD}debian-10${RESET} (buster)"
	@echo -e "\t\t${BOLD}fedora-39${RESET}"
	@echo -e "\t\t${BOLD}fedora-38${RESET}"
	@echo -e "\t\t${BOLD}fedora-37${RESET}"
	@echo -e "\t\t${BOLD}opensuse-leap${RESET} (latest)"
	@echo -e "\t\t${BOLD}rockylinux-9${RESET} (latest)"
	@echo -e "\t\t${BOLD}ubuntu-24.04${RESET} (Ubuntu 24.04 LTS, latest)"
	@echo -e "\t\t${BOLD}ubuntu-23.10${RESET} (Ubuntu 23.10, rolling)"
	@echo -e "\t\t${BOLD}ubuntu-23.04${RESET} (Ubuntu 23.04)"
	@echo -e "\t\t${BOLD}ubuntu-22.04${RESET} (Ubuntu 22.04 LTS)"
	@echo -e "\t\t${BOLD}ubuntu-20.04${RESET} (Ubuntu 20.04 LTS)"
	@echo
	@echo -e "\t${BOLD}<stage>${RESET}:"
	@echo -e "\t\t${BOLD}env${RESET}"
	@echo -e "\t\t${BOLD}devel${RESET}"
	@echo -e "\t\t${BOLD}<lang>_build${RESET}"
	@echo -e "\t\t${BOLD}<lang>_archive${RESET}"
	@echo -e "\t\t${BOLD}<lang>_export${RESET}"
	@echo -e "\t\t${BOLD}<lang>_test${RESET}"
	@echo
	@echo -e "\t${BOLD}<lang>${RESET}: Language to build"
	@echo -e "\t\t${BOLD}cpp${RESET} C++"
	@echo -e "\t\t${BOLD}dotnet${RESET} .Net Core 3.1 and/or .Net 6.0 wrappers"
	@echo -e "\t\t${BOLD}java${RESET} Java (JDK 8.0) wrappers"
	@echo -e "\t\t${BOLD}python${RESET} Python 3.8+ wrappers"
	@echo
	@echo -e "\te.g. 'make sh_amd64_ubuntu-22.04_cpp_build'"
	@echo -e "\te.g. 'make amd64_ubuntu-22.04_cpp_archive'"
	@echo -e "\te.g. 'make amd64_ubuntu-22.04_cpp_export'"
	@echo -e "\te.g. 'make amd64_ubuntu-22.04_cpp_test'"
	@echo
	@echo -e "\tnote: Few custom merge targets (e.g. ${BOLD}export${RESET}) are also provided."
	@echo -e "\te.g. 'make amd64_almalinux-9_export'"
	@echo -e "\te.g. 'make arm64_almalinux-9_export'"
	@echo
	@echo -e "\t${BOLD}NOCACHE=1${RESET}: use 'docker build --no-cache' when building container (default use cache)."
	@echo -e "\t${BOLD}VERBOSE=1${RESET}: use 'docker build --progress=plain' when building container."
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
# OR_TOOLS_MAJOR & OR_TOOLS_MINOR
include ../../Version.txt
OR_TOOLS_PATCH := $(shell git rev-list --count v${OR_TOOLS_MAJOR}.0..HEAD || echo 0)
OR_TOOLS_VERSION := ${OR_TOOLS_MAJOR}.${OR_TOOLS_MINOR}.${OR_TOOLS_PATCH}
ifdef PRE_RELEASE
OR_TOOLS_VERSION := ${OR_TOOLS_VERSION}-rc
endif
$(info branch: ${OR_TOOLS_BRANCH})
$(info SHA1: ${OR_TOOLS_SHA1})
$(info version: ${OR_TOOLS_VERSION})

# Docker image name prefix.
IMAGE := or-tools/docker

DOCKER_BUILD_CMD := docker build
DOCKER_BUILDX_CMD := docker buildx build
ifdef NOCACHE
DOCKER_BUILD_CMD := ${DOCKER_BUILD_CMD} --no-cache
DOCKER_BUILDX_CMD := ${DOCKER_BUILDX_CMD} --no-cache
endif
ifdef VERBOSE
DOCKER_BUILD_CMD := ${DOCKER_BUILD_CMD} --progress=plain
DOCKER_BUILDX_CMD := ${DOCKER_BUILDX_CMD} --progress=plain
endif
DOCKER_RUN_CMD := docker run --rm --init --net=host

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
PYTHON_PLATFORMS := amd64 arm64v8
PYTHON_DISTROS := manylinux alpine
PYTHON_STAGES := env devel build test

export:
	-mkdir $@

export/python: | export
	-mkdir $@

cache:
	-mkdir $@

cache/python: | cache
	-mkdir $@

## MANYLINUX ##
PYTHON_VERSIONS := 38 39 310 311 312

export/python/manylinux: | export/python
	-mkdir -p $@

export/python/manylinux/build-manylinux.sh: python/build-manylinux.sh | export/python/manylinux
	cp $< $@

define manylinux_inner =
#$$(info manylinux_inner: PLATFORM:'$1' VERSION:'$2' STAGE:'$3')

.PHONY: python_$1_manylinux_cp$2_$3
python_$1_manylinux_cp$2_$3: python/$1/manylinux.Dockerfile export/python/manylinux/build-manylinux.sh
	@docker image rm -f ${IMAGE}:$$@ 2>/dev/null
	${DOCKER_BUILDX_CMD} --platform linux/$1 \
 --tag ${IMAGE}:$$@ \
 --build-arg GIT_BRANCH=${OR_TOOLS_BRANCH} \
 --build-arg GIT_SHA1=${OR_TOOLS_SHA1} \
 --build-arg OR_TOOLS_PATCH=${OR_TOOLS_PATCH} \
 --build-arg PYTHON_VERSION=$2 \
 --target=$3 \
 -f $$< \
 export/python/manylinux

.PHONY: save_python_$1_manylinux_cp$2_$3
save_python_$1_manylinux_cp$2_$3: cache/python/docker_$1_manylinux_cp$2_$3.tar
cache/python/docker_$1_manylinux_cp$2_$3.tar: python_$1_manylinux_cp$2_$3
	@rm -f $$@
	mkdir -p cache/python/
	docker save ${IMAGE}:$$< -o $$@

.PHONY: clean_python_$1_manylinux_cp$2_$3
clean_python_$1_manylinux_cp$2_$3: python/$1/manylinux.Dockerfile export/python/manylinux/build-manylinux.sh
	docker image rm -f ${IMAGE}:python_$1_manylinux_cp$2_$3 2>/dev/null
	rm -f cache/python/docker_$1_manylinux_cp$2_$3.tar

# Debug purpose
.PHONY: sh_python_$1_manylinux_cp$2_$3
sh_python_$1_manylinux_cp$2_$3: python_$1_manylinux_cp$2_$3
	${DOCKER_RUN_CMD} \
 -v `pwd`/export:/export \
 -it \
 --name ortools_$$< \
 ${IMAGE}:$$<
endef

define manylinux_outer =
#$$(info manylinux_outer: PLATFORM: '$1' VERSION: '$2')

$$(foreach stage,${PYTHON_STAGES},$$(eval $$(call manylinux_inner,$1,$2,$${stage})))

.PHONY: python_$1_manylinux_cp$2_export
python_$1_manylinux_cp$2_export: python_$1_manylinux_cp$2_build
	${DOCKER_RUN_CMD} \
 -v `pwd`/export:/export \
 -it \
 --name ortools_$$< \
 ${IMAGE}:$$< \
 "cp build*/python/dist/*.whl /export/python"
endef

$(foreach version,${PYTHON_VERSIONS},$(eval $(call manylinux_outer,amd64,${version})))
$(foreach version,${PYTHON_VERSIONS},$(eval $(call manylinux_outer,arm64v8,${version})))

# Merge
define manylinux_merge =
#$$(info manylinux_merge: PLATFORM:'$1' STAGE:'$2')

.PHONY: python_$1_manylinux_$2
python_$1_manylinux_$2: $(addprefix python_$1_manylinux_cp, $(addsuffix _$2, ${PYTHON_VERSIONS}))
.PHONY: save_python_$1_manylinux_$2
save_python_$1_manylinux_$2: $(addprefix save_python_$1_manylinux_cp, $(addsuffix _$2, ${PYTHON_VERSIONS}))
.PHONY: clean_python_$1_manylinux_$2
clean_python_$1_manylinux_$2: $(addprefix clean_python_$1_manylinux_cp, $(addsuffix _$2, ${PYTHON_VERSIONS}))
endef

$(foreach stage,${PYTHON_STAGES} export,$(eval $(call manylinux_merge,amd64,${stage})))
$(foreach stage,${PYTHON_STAGES} export,$(eval $(call manylinux_merge,arm64v8,${stage})))

## ALPINE ##
export/python/alpine: | export/python
	-mkdir -p $@

define alpine_loop =
#$$(info alpine_loop: PLATFORM:'$1' STAGE:'$2')

.PHONY: python_$1_alpine_$2
python_$1_alpine_$2: python/$1/alpine.Dockerfile | export/python/alpine
	@docker image rm -f ${IMAGE}:$$@ 2>/dev/null
	${DOCKER_BUILD_CMD} \
 --tag ${IMAGE}:$$@ \
 --build-arg GIT_BRANCH=${OR_TOOLS_BRANCH} \
 --build-arg GIT_SHA1=${OR_TOOLS_SHA1} \
 --build-arg OR_TOOLS_PATCH=${OR_TOOLS_PATCH} \
 --target=$2 \
 -f $$< \
 export/python/alpine

.PHONY: save_python_$1_alpine_$2
save_python_$1_alpine_$2: cache/python/docker_$1_alpine_$2.tar
cache/python/docker_$1_alpine_$2.tar: python_$1_alpine_$2 | cache/python
	@rm -f $$@
	docker save ${IMAGE}:$$< -o $$@

.PHONY: clean_python_$1_alpine_$2
clean_python_$1_alpine_$2: python/$1/alpine.Dockerfile | export/python/alpine
	docker image rm -f ${IMAGE}:python_$1_alpine_$2 2>/dev/null
	rm -f cache/python/docker_$1_alpine_$2.tar

# Debug purpose
.PHONY: sh_python_$1_alpine_$2
sh_python_$1_alpine_$2: python_$1_alpine_$2
	${DOCKER_RUN_CMD} \
 -v `pwd`/export:/export \
 -it \
 --name ortools_$$< \
 ${IMAGE}:$$<
endef

$(foreach stage,${PYTHON_STAGES},$(eval $(call alpine_loop,amd64,${stage})))
$(foreach stage,${PYTHON_STAGES},$(eval $(call alpine_loop,arm64v8,${stage})))

alpine_export_targets =  $(addprefix python_, $(addsuffix _alpine_export, ${PYTHON_PLATFORMS}))
.PHONY: ${alpine_export_targets}
${alpine_export_targets}: python_%_alpine_export: python_%_alpine_build | export
	${DOCKER_RUN_CMD} \
 -v `pwd`/export:/export \
 -it \
 --name ortools_$< \
 ${IMAGE}:$< \
 "cp build*/python/dist/*.whl /export/python"

## MERGE DISTRO ##
define python_distro_merge =
#$$(info python_distro_merge: PLATFORM:'$1' STAGE:'$2')

.PHONY: python_$1_$2
python_$1_$2: $(addprefix python_$1_, $(addsuffix _$2, ${PYTHON_DISTROS}))
.PHONY: save_python_$1_$2
save_python_$1_$2: $(addprefix save_python_$1_, $(addsuffix _$2, ${PYTHON_DISTROS}))
.PHONY: clean_python_$1_$2
clean_python_$1_$2: $(addprefix clean_python_$1_, $(addsuffix _$2, ${PYTHON_DISTROS}))
endef

$(foreach stage,${PYTHON_STAGES} export,$(eval $(call python_distro_merge,amd64,${stage})))
$(foreach stage,${PYTHON_STAGES} export,$(eval $(call python_distro_merge,arm64v8,${stage})))

## MERGE PLATFORM ##
define clean_python_platform =
#$$(info clean_python_platform: PLATFORM:'$1')

.PHONY: clean_python_$1
clean_python_$1: $(addprefix clean_python_$1_, ${PYTHON_STAGES})
endef

$(foreach platform,${PYTHON_PLATFORMS},$(eval $(call clean_python_platform,${platform})))


define python_platform_merge =
#$$(info python_platform_merge: STAGE:'$1')

.PHONY: python_$1
python_$1: $(addprefix python_, $(addsuffix _$1, ${PYTHON_PLATFORMS}))
.PHONY: save_python_$1
save_python_$1: $(addprefix save_python_, $(addsuffix _$1, ${PYTHON_PLATFORMS}))
.PHONY: clean_python_$1
clean_python_$1: $(addprefix clean_python_, $(addsuffix _$1, ${PYTHON_PLATFORMS}))
endef

$(foreach stage,${PYTHON_STAGES} export,$(eval $(call python_platform_merge,${stage})))

# Alias
.PHONY: python
python: python_amd64_export

.PHONY: clean_python
clean_python: $(addprefix clean_python_, ${PYTHON_PLATFORMS})
	-rm -rf cache/python/*
	-rm -rf export/python

#################
###  ARCHIVES  ##
#################
# $* stem
# $< first prerequist
# $@ target name

export/%/or-tools.snk: or-tools.snk | export
	-mkdir -p export/$*
	cp $< $@

# generic rule export/% prevent other rules
# e.g. export/%/docker.devel.tar -> need an exhaustive list
export/archives: | export
	-mkdir $@

# Currently supported platform
PLATFORMS := amd64 arm64 # riscv64

# Currently supported distro
DISTROS := \
 almalinux-9 \
 alpine-edge \
 archlinux \
 debian-11 debian-12 debian-sid \
 fedora-37 fedora-38 fedora-39 \
 opensuse-leap \
 rockylinux-9 \
 ubuntu-20.04 ubuntu-22.04 ubuntu-23.04 ubuntu-23.10 ubuntu-24.04

# List of stages
STAGES := env devel

define build_stage =
#$$(info build_stage: PLATFORM:'$1' DISTRO:'$2' STAGE:'$3')

.PHONY: $1_$2_$3
$1_$2_$3: images/$2.Dockerfile | export/$1/$2/or-tools.snk
	#@docker image rm -f ${IMAGE}:$$@ 2>/dev/null
	${DOCKER_BUILDX_CMD} --platform linux/$1 \
 --tag ${IMAGE}:$$@ \
 --build-arg SRC_GIT_BRANCH=${OR_TOOLS_BRANCH} \
 --build-arg SRC_GIT_SHA1=${OR_TOOLS_SHA1} \
 --build-arg OR_TOOLS_PATCH=${OR_TOOLS_PATCH} \
 --target=$3 \
 -f $$< \
 export/$1/$2

.PHONY: save_$1_$2_$3
save_$1_$2_$3: cache/$1/$2/docker_$3.tar
cache/$1/$2/docker_$3.tar: $1_$2_$3
	@rm -f $$@
	mkdir -p cache/$1/$2
	docker save ${IMAGE}:$$< -o $$@

.PHONY: sh_$1_$2_$3
sh_$1_$2_$3: $1_$2_$3 | export
	${DOCKER_RUN_CMD} \
 -v $$$$(pwd)/export:/export \
 -it \
 --name ortools_$$< \
 ${IMAGE}:$$<

.PHONY: clean_$1_$2_$3
clean_$1_$2_$3:
	docker image rm -f ${IMAGE}:$1_$2_$3 2>/dev/null
	rm -f cache/$1/$2/docker_$3.tar
endef

define build_distro =
#$$(info build_distro: PLATFORM:'$1' DISTRO:'$2')

$$(foreach stage,${STAGES},$$(eval $$(call build_stage,$1,$2,$${stage})))
endef

define build_platform =
#$$(info build_platform: PLATFORM:'$1')

$$(foreach distro,${DISTROS},$$(eval $$(call build_distro,$1,$${distro})))
endef

$(foreach platform,${PLATFORMS},$(eval $(call build_platform,${platform})))

# List language dependent stages
LANGS := cpp dotnet java python
LANGS_EXPORT := cpp dotnet java
LANG_STAGES := build archive

define build_lang_stage =
#$$(info build_lang_stage: PLATFORM:'$1' DISTRO:'$2' LANG:'$3' STAGE:'$4')

.PHONY: $1_$2_$3_$4
$1_$2_$3_$4: images/$2.Dockerfile | export/$1/$2/or-tools.snk
	#@docker image rm -f ${IMAGE}:$$@ 2>/dev/null
	${DOCKER_BUILDX_CMD} --platform linux/$1 \
 --tag ${IMAGE}:$$@ \
 --build-arg SRC_GIT_BRANCH=${OR_TOOLS_BRANCH} \
 --build-arg SRC_GIT_SHA1=${OR_TOOLS_SHA1} \
 --build-arg OR_TOOLS_PATCH=${OR_TOOLS_PATCH} \
 --target=$3_$4 \
 -f $$< \
 export/$1/$2

.PHONY: save_$1_$2_$3_$4
save_$1_$2_$3_$4: cache/$1/$2/docker_$3_$4.tar
cache/$1/$2/docker_$3_$4.tar: $1_$2_$3_$4
	@rm -f $$@
	mkdir -p cache/$1/$2
	docker save ${IMAGE}:$$< -o $$@

.PHONY: sh_$1_$2_$3_$4
sh_$1_$2_$3_$4: $1_$2_$3_$4 | export
	${DOCKER_RUN_CMD} \
 -v $$$$(pwd)/export:/export \
 -it \
 --name ortools_$$< \
 ${IMAGE}:$$<

.PHONY: clean_$1_$2_$3_$4
clean_$1_$2_$3_$4:
	docker image rm -f ${IMAGE}:$1_$2_$3_$4 2>/dev/null
	rm -f cache/$1/$2/docker_$3_$4.tar
endef

define build_lang_distro =
#$$(info build_lang_distro: PLATFORM:'$1' DISTRO:'$2')

$$(foreach stage,${LANG_STAGES},$$(eval $$(call build_lang_stage,$1,$2,cpp,$${stage})))
$$(foreach stage,${LANG_STAGES},$$(eval $$(call build_lang_stage,$1,$2,dotnet,$${stage})))
$$(foreach stage,${LANG_STAGES},$$(eval $$(call build_lang_stage,$1,$2,java,$${stage})))
$$(foreach stage,${LANG_STAGES},$$(eval $$(call build_lang_stage,$1,$2,python,$${stage})))
endef

define build_lang_platform =
#$$(info build_lang_platform: PLATFORM:'$1')

$$(foreach distro,${DISTROS},$$(eval $$(call build_lang_distro,$1,$${distro})))
endef

$(foreach platform,${PLATFORMS},$(eval $(call build_lang_platform,${platform})))

# Special language dependent export target
define export_archive =
#$$(info export_archive: PLATFORM:'$1' DISTRO:'$2' LANG:'$3')

.PHONY: $1_$2_$3_export
$1_$2_$3_export: export/archives/or-tools_$1_$2_$3_v${OR_TOOLS_VERSION}.tar.gz \

export/archives/or-tools_$1_$2_$3_v${OR_TOOLS_VERSION}.tar.gz: $1_$2_$3_archive | export/archives
	-rm -f export/archives/or-tools_$1_$2_$3_v*.tar.gz
	-mkdir -p export/$1/$2
	${DOCKER_RUN_CMD} \
 -w /root/or-tools \
 -v $$$$(pwd)/export:/export \
 ${IMAGE}:$$< \
 "cp or-tools_*_v*.tar.gz /export/$1/$2/"
	cp export/$1/$2/or-tools_*_$3*_v*.tar.gz $$@

.PHONY: clean_$1_$2_$3_export
clean_$1_$2_$3_export:
	rm -f export/archives/or-tools_$1_$2_$3_v${OR_TOOLS_VERSION}.tar.gz
endef

define export_distro =
#$$(info export_distro: PLATFORM:'$1' DISTRO:'$2')

$$(foreach lang,${LANGS},$$(eval $$(call export_archive,$1,$2,$${lang})))

.PHONY: $1_$2_export
$1_$2_export: $(addprefix $1_$2_, $(addsuffix _export, ${LANGS_EXPORT}))

.PHONY: clean_$1_$2_export
clean_$1_$2_export: $(addprefix clean_$1_$2_, $(addsuffix _export, ${LANGS}))
endef

define export_platform =
#$$(info export_platform: PLATFORM:'$1')

$$(foreach distro,${DISTROS},$$(eval $$(call export_distro,$1,$${distro})))
endef

$(foreach platform,${PLATFORMS},$(eval $(call export_platform,${platform})))

## MERGE ##
define merge_stage =
#$$(info merge_stage: PLATFORM:'$1' STAGE:'$2')

.PHONY: $1_$2
$1_$2: $(addprefix $1_, $(addsuffix _$2, ${DISTROS}))

.PHONY: clean_$1_$2
clean_$1_$2: $(addprefix clean_$1_, $(addsuffix _$2, ${DISTROS}))
endef

define merge_platform =
#$$(info merge_platform: PLATFORM:'$1')

$$(foreach stage,${STAGES} export,$$(eval $$(call merge_stage,$1,$${stage})))
$$(foreach stage,${LANG_STAGES} export,$$(eval $$(call merge_stage,$1,cpp_$${stage})))
$$(foreach stage,${LANG_STAGES} export,$$(eval $$(call merge_stage,$1,dotnet_$${stage})))
$$(foreach stage,${LANG_STAGES} export,$$(eval $$(call merge_stage,$1,java_$${stage})))
$$(foreach stage,${LANG_STAGES} export,$$(eval $$(call merge_stage,$1,python_$${stage})))

build_targets := $(addprefix $1_, ${STAGES} export) \
 $(addprefix $1_cpp_, ${LANG_STAGES} export) \
 $(addprefix $1_dotnet_, ${LANG_STAGES} export) \
 $(addprefix $1_java_, ${LANG_STAGES} export) \
 $(addprefix $1_python_, ${LANG_STAGES} export)
.PHONY: $${build_targets}
$${build_targets}: $1_%: $(addprefix $1_, $(addsuffix _%, ${DISTROS}))

.PHONY: $1_export
$1_export: $1_cpp_export $1_dotnet_export $1_java_export #$1_python_export

clean_targets := $(addprefix clean_$1_, ${STAGES} export) \
 $(addprefix clean_$1_cpp_, ${LANG_STAGES} export) \
 $(addprefix clean_$1_dotnet_, ${LANG_STAGES} export) \
 $(addprefix clean_$1_java_, ${LANG_STAGES} export) \
 $(addprefix clean_$1_python_, ${LANG_STAGES} export)
.PHONY: $${clean_targets}
$${clean_targets}: clean_$1_%: $(addprefix clean_$1_, $(addsuffix _%, ${DISTROS}))

.PHONY: clean_$1
clean_$1: $${clean_targets}
endef

$(foreach platform,${PLATFORMS},$(eval $(call merge_platform,${platform})))

# Alias
.PHONY: archives
archives: amd64_export

.PHONY: clean_archives
clean_archives: clean_amd64

############
##  TEST  ##
############
define test_lang =
#$$(info test_lang: PLATFORM:'$1' DISTRO:'$2' LANG:'$3')

.PHONY: $1_$2_$3_test
$1_$2_$3_test: test/$2/$3.Dockerfile $1_$2_$3_export
	#@docker image rm -f ${IMAGE}:$$@ 2>/dev/null
	${DOCKER_BUILDX_CMD} --platform linux/$1 \
 --tag ${IMAGE}:$$@ \
 -f $$< \
 export/archives

.PHONY: save_$1_$2_$3_test
save_$1_$2_$3_test: cache/$1/$2/docker_$3_test.tar
cache/$1/$2/docker_$3_test.tar: $1_$2_$3_test
	@rm -f $$@
	mkdir -p cache/$1/$2
	docker save ${IMAGE}:$$< -o $$@

.PHONY: sh_$1_$2_$3_test
sh_$1_$2_$3_test: $1_$2_$3_test
	${DOCKER_RUN_CMD} \
 -v `pwd`/export:/export \
 -it \
 --name ortools_$$< \
 ${IMAGE}:$$<

.PHONY: clean_$1_$2_$3_test
clean_$1_$2_$3_test:
	docker image rm -f ${IMAGE}:$1_$2_$3_test 2>/dev/null
	rm -f cache/$1/$2/docker_$3_test.tar
endef

define test_distro =
#$$(info test_distro: PLATFORM:'$1' DISTRO:'$2')

$$(foreach lang,${LANGS},$$(eval $$(call test_lang,$1,$2,$${lang})))

.PHONY: $1_$2_test
$1_$2_test: $(addprefix $1_$2_, $(addsuffix _test, ${LANGS}))

.PHONY: clean_$1_$2_test
clean_$1_$2_test: $(addprefix clean_$1_$2_, $(addsuffix _test, ${LANGS}))
endef

define test_platform =
#$$(info test_platform: PLATFORM:'$1')

$$(foreach distro,${DISTROS},$$(eval $$(call test_distro,$1,$${distro})))

.PHONY: $1_test
$1_test: $(addprefix $1_, $(addsuffix _test, ${DISTROS}))

.PHONY: clean_$1_test
clean_$1_test: $(addprefix clean_$1_, $(addsuffix _test, ${DISTROS}))
endef

$(foreach platform,${PLATFORMS},$(eval $(call test_platform,${platform})))

# Alias
.PHONY: test
test: $(addsuffix _test, ${PLATFORMS})

.PHONY: clean_test
test: $(addprefix clean_, $(addsuffix _test, ${PLATFORMS}))

#############
##  CLEAN  ##
#############
.PHONY: clean
clean: \
 clean_python \
 clean_archives \
 clean_test
	docker container prune -f
	docker image prune -f
	-rm -rf cache

.PHONY: distclean
distclean: clean
	-docker container ls -a
	-docker container prune -f
	-docker image ls -a
	-docker image prune -a -f
	-docker system df
	-docker system prune -a -f
	-rm -rf export
