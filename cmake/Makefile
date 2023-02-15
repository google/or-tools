# Copyright 2010-2022 Google LLC
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

PROJECT := ortools
BUILD_SYSTEM := cmake
BRANCH := $(shell git rev-parse --abbrev-ref HEAD)
SHA1 := $(shell git rev-parse --verify HEAD)

# General commands
.PHONY: help
BOLD=\e[1m
RESET=\e[0m

help:
	@echo -e "${BOLD}SYNOPSIS${RESET}"
	@echo -e "\tmake <target> [NOCACHE=1]"
	@echo
	@echo -e "${BOLD}DESCRIPTION${RESET}"
	@echo -e "\ttest build inside docker container to have a reproductible build."
	@echo
	@echo -e "${BOLD}MAKE TARGETS${RESET}"
	@echo -e "\t${BOLD}help${RESET}: display this help and exit."
	@echo
	@echo -e "\tBuild using docker and the host platform."
	@echo -e "\t${BOLD}<distro>_<stage>${RESET}: build a <stage> docker image for a specific distro."
	@echo -e "\t${BOLD}save_<distro>_<stage>${RESET}: Save a <stage> docker image for a specific distro."
	@echo -e "\t${BOLD}sh_<distro>_<stage>${RESET}: run a container using the <stage> docker image specified (debug purpose)."
	@echo -e "\t${BOLD}clean_<distro>_<stage>${RESET}: Remove a <stage> docker image for a specific distro."
	@echo
	@echo -e "\tWith ${BOLD}<distro>${RESET}:"
	@echo -e "\t\t${BOLD}alpine${RESET} (edge)"
	@echo -e "\t\t${BOLD}archlinux${RESET} (latest)"
	@echo -e "\t\t${BOLD}centos${RESET} (latest)"
	@echo -e "\t\t${BOLD}debian${RESET} (latest)"
	@echo -e "\t\t${BOLD}fedora${RESET} (latest)"
	@echo -e "\t\t${BOLD}opensuse${RESET} (tumbleweed)"
	@echo -e "\t\t${BOLD}ubuntu${RESET} (rolling)"
	@echo -e "\t\t${BOLD}system_deps${RESET} (archlinux with all deps from pacman)"
	@echo -e "\t\t${BOLD}all${RESET}: ALL DISTROS"
	@echo
	@echo -e "\tWith ${BOLD}<stage>${RESET}:"
	@echo -e "\t\t${BOLD}base${RESET} (need by cpp)"
	@echo -e "\t\t${BOLD}swig${RESET} (need by .Net, Java and Python)"
	@echo -e "\t\t${BOLD}<lang>_env${RESET}"
	@echo -e "\t\t${BOLD}<lang>_devel${RESET}"
	@echo -e "\t\t${BOLD}<lang>_build${RESET}"
	@echo -e "\t\t${BOLD}<lang>_test${RESET}"
	@echo -e "\t\t${BOLD}<lang>_install_env${RESET}"
	@echo -e "\t\t${BOLD}<lang>_install_devel${RESET}"
	@echo -e "\t\t${BOLD}<lang>_install_build${RESET}"
	@echo -e "\t\t${BOLD}<lang>_install_test${RESET}"
	@echo
	@echo -e "\tWith ${BOLD}<lang>${RESET}:"
	@echo -e "\t\t${BOLD}cpp${RESET} (C++)"
	@echo -e "\t\t${BOLD}python${RESET} (Python >= 3)"
	@echo -e "\t\t${BOLD}java${RESET} (Java >= 8)"
	@echo -e "\t\t${BOLD}dotnet${RESET} (.Net Core >= 2.1)"
	@echo
	@echo -e "\te.g. 'make ubuntu_base'"
	@echo -e "\te.g. 'make alpine_cpp_build'"
	@echo -e "\te.g. 'make archlinux_cpp_build'"
	@echo
	@echo -e "\tBuild using a toolchain."
	@echo -e "\t${BOLD}<toolchain>_<toolchain_stage>${RESET}: build <stage> docker image for a specific toolchain target."
	@echo -e "\t${BOLD}save_<toolchain>_<toolchain_stage>${RESET}: Save the <stage> docker image for a specific platform."
	@echo -e "\t${BOLD}sh_<toolchain>_<toolchain_stage>${RESET}: run a container using the <stage> docker image specified (debug purpose)."
	@echo -e "\t${BOLD}clean_<toolchain>_<toolchain_stage>${RESET}: Remove cache and docker image."
	@echo -e "\t${BOLD}clean_toolchains${RESET}: Remove ALL cache and docker image."
	@echo
	@echo -e "\tWith ${BOLD}<toolchain>${RESET}:"
	@echo -e "\t\t${BOLD}aarch64${RESET} (bootlin toolchain)"
	@echo -e "\t\t${BOLD}aarch64be${RESET} (bootlin toolchain)"
	@echo -e "\t\t${BOLD}mips64${RESET} (codespace toolchain)"
	@echo -e "\t\t${BOLD}mips64el${RESET} (codespace toolchain)"
	@echo -e "\t\t${BOLD}ppc64${RESET} (bootlin toolchain)"
	@echo -e "\t\t${BOLD}ppc64le${RESET} (bootlin toolchain)"
	@echo
	@echo -e "\tWith ${BOLD}<toolchain_stage>${RESET}:"
	@echo -e "\t\t${BOLD}env${RESET}"
	@echo -e "\t\t${BOLD}devel${RESET}"
	@echo -e "\t\t${BOLD}build${RESET}"
	@echo -e "\t\t${BOLD}test${RESET}"
	@echo -e "\te.g. 'make mips64_build'"
	@echo -e "\te.g. 'make aarch64_test'"
	@echo
	@echo -e "\tBuild using a vagrant machine."
	@echo -e "\t${BOLD}<vm>_<vms_stage>${RESET}: build the vagrant <vm> virtual machine."
	@echo -e "\t${BOLD}sh_<vm>_<lang>${RESET}: ssh to the vagrant machine specified (debug purpose)."
	@echo -e "\t${BOLD}clean_<vm>${RESET}: Remove virtual machine for the specified vm."
	@echo -e "\t${BOLD}clean_vms${RESET}: Remove ALL vagrant box."
	@echo
	@echo -e "\tWith ${BOLD}<vm>${RESET}:"
	@echo -e "\t\t${BOLD}freebsd${RESET} (FreeBSD)"
	@echo
	@echo -e "\tWith ${BOLD}<vms_stage>${RESET}:"
	@echo -e "\t\t${BOLD}build${RESET}"
	@echo
	@echo -e "\t${BOLD}glop_<stage>${RESET}: Build Glop <stage> using an Ubuntu:rolling docker image."
	@echo -e "\t${BOLD}save_glop_<stage>${RESET}: Save Glop <stage> using an Ubuntu:rolling docker image."
	@echo -e "\t${BOLD}sh_glop_<stage>${RESET}: Run a Glop <stage> container using an Ubuntu:rolling docker image (debug purpose)."
	@echo -e "\t${BOLD}clean_glop_<stage>${RESET}: Remove the Glop <stage> docker image (and the save archive if any) for a specific distro."
	@echo
	@echo -e "\tGlobal targets."
	@echo -e "\t${BOLD}clean${RESET}: Remove ALL caches and docker images."
	@echo -e "\t${BOLD}clean_<distro>${RESET}: Remove cache and docker image for the specified distro."
	@echo -e "\t${BOLD}distclean${RESET}: Remove everything."
	@echo
	@echo -e "\t${BOLD}NOCACHE=1${RESET}: use 'docker build --no-cache' when building container (default use cache)."
	@echo
	@echo -e "branch: $(BRANCH)"
	@echo -e "sha1: $(SHA1)"

# Need to add cmd_distro to PHONY otherwise target are ignored since they do not
# contain recipe (using FORCE do not work here)
.PHONY: all
all: build

# Delete all implicit rules to speed up makefile
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:
# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =
# Keep all intermediate files
# ToDo: try to remove it later
.SECONDARY:

# Docker image name prefix.
IMAGE := ${PROJECT}/${BUILD_SYSTEM}

ifdef NOCACHE
DOCKER_BUILD_CMD := docker build --no-cache
DOCKER_BUILDX_CMD := docker buildx build --no-cache
else
DOCKER_BUILD_CMD := docker build
DOCKER_BUILDX_CMD := docker buildx build
endif
DOCKER_RUN_CMD := docker run --rm --init --net=host

# Currently supported distro
DISTROS = alpine archlinux centos debian fedora opensuse system_deps ubuntu
LANGUAGES = cpp python java dotnet

# $* stem
# $< first prerequist
# $@ target name

###############
## PRESTAGES ##
###############
PRESTAGES = base swig
define make-prestage-target =
#$$(info STAGE: $1)
#$$(info Create targets: all_$1 $(addsuffix _$1, $(DISTROS)).)
targets_$1 = $(addsuffix _$1, $(DISTROS))
.PHONY: all_$1 $$(targets_$1)
all_$1: $$(targets_$1)
$$(targets_$1): %_$1: docker/%/Dockerfile
	#@docker image rm -f ${IMAGE}:$$*_$1 2>/dev/null
	${DOCKER_BUILD_CMD} --target=$1 --tag ${IMAGE}:$$*_$1 -f $$< ..

#$$(info Create targets: save_all_$1 $(addprefix save_, $(addsuffix _$1, $(DISTROS))) (debug).)
save_targets_$1 = $(addprefix save_, $(addsuffix _$1, $(DISTROS)))
.PHONY: save_all_$1 $$(save_targets_$1)
save_all_$1: $$(save_targets_$1)
$$(save_targets_$1): save_%_$1: cache/%/docker_$1.tar
cache/%/docker_$1.tar: %_$1
	@rm -f $$@
	mkdir -p cache/$$*
	docker save ${IMAGE}:$$*_$1 -o $$@

#$$(info Create targets: $(addprefix sh_, $(addsuffix _$1, $(DISTROS))) (debug).)
sh_targets_$1 = $(addprefix sh_, $(addsuffix _$1, $(DISTROS)))
.PHONY: $$(sh_targets_$1)
$$(sh_targets_$1): sh_%_$1: %_$1
	${DOCKER_RUN_CMD} -it --name ${PROJECT}_${BUILD_SYSTEM}_$$*_$1 ${IMAGE}:$$*_$1

#$$(info Create targets: clean_all_$1 $(addprefix clean_, $(addsuffix _$1, $(DISTROS))).)
clean_targets_$1 = $(addprefix clean_, $(addsuffix _$1, $(DISTROS)))
.PHONY: clean_all_$1 $$(clean_targets_$1)
clean_all_$1: $$(clean_targets_$1)
$$(clean_targets_$1): clean_%_$1:
	docker image rm -f ${IMAGE}:$$*_$1 2>/dev/null
	rm -f cache/$$*/docker_$1.tar
endef

$(foreach stage,$(PRESTAGES),$(eval $(call make-prestage-target,$(stage))))

############
## STAGES ##
############
STAGES = env devel build test install_env install_devel install_build install_test
define make-stage-target =
#$$(info STAGE: $1)
#$$(info Create targets: all_$1 $(addsuffix _$1, $(DISTROS)).)
targets_$1 = $(addsuffix _$1, $(DISTROS))
.PHONY: all_$1 $$(targets_$1)
all_$1: $$(targets_$1)
$$(targets_$1): %_$1: $(addprefix %_, $(addsuffix _$1, $(LANGUAGES)))

#$$(info Create targets: $(addsuffix _$1, $(LANGUAGES)).)
targets_$1 = $(addsuffix _$1, $(LANGUAGES))
.PHONY: $$(targets_$1)
$$(targets_$1): %_$1: $(addsuffix _$1, $(addsuffix _%, $(DISTROS)))

#$$(info Create targets: $(addsuffix _cpp_$1, $(DISTROS)))
cpp_targets_$1 = $(addsuffix _cpp_$1, $(DISTROS))
#$$(info Create targets: $(addsuffix _dotnet_$1, $(DISTROS)))
dotnet_targets_$1 = $(addsuffix _dotnet_$1, $(DISTROS))
#$$(info Create targets: $(addsuffix _java_$1, $(DISTROS)))
java_targets_$1 = $(addsuffix _java_$1, $(DISTROS))
#$$(info Create targets: $(addsuffix _python_$1, $(DISTROS)))
python_targets_$1 = $(addsuffix _python_$1, $(DISTROS))
.PHONY: $$(cpp_targets_$1) $$(dotnet_targets_$1) $$(java_targets_$1) $$(python_targets_$1)

$$(cpp_targets_$1): %_cpp_$1: docker/%/cpp.Dockerfile %_base
	#@docker image rm -f ${IMAGE}:$$*_cpp_$1 2>/dev/null
	${DOCKER_BUILD_CMD} --target=$1 --tag ${IMAGE}:$$*_cpp_$1 -f $$< ..
$$(dotnet_targets_$1): %_dotnet_$1: docker/%/dotnet.Dockerfile %_swig
	#@docker image rm -f ${IMAGE}:$$*_dotnet_$1 2>/dev/null
	${DOCKER_BUILD_CMD} --target=$1 --tag ${IMAGE}:$$*_dotnet_$1 -f $$< ..
$$(java_targets_$1): %_java_$1: docker/%/java.Dockerfile %_swig
	#@docker image rm -f ${IMAGE}:$$*_java_$1 2>/dev/null
	${DOCKER_BUILD_CMD} --target=$1 --tag ${IMAGE}:$$*_java_$1 -f $$< ..
$$(python_targets_$1): %_python_$1: docker/%/python.Dockerfile %_swig
	#@docker image rm -f ${IMAGE}:$$*_python_$1 2>/dev/null
	${DOCKER_BUILD_CMD} --target=$1 --tag ${IMAGE}:$$*_python_$1 -f $$< ..

# Save $1 docker images (debug).
#$$(info Create targets: save_all_$1 $(addprefix save_, $(addsuffix _$1, $(DISTROS))).)
targets_$1 = $(addprefix save_, $(addsuffix _$1, $(DISTROS)))
.PHONY: save_all_$1 $$(targets_$1)
save_all_$1: $$(targets_$1)
$$(targets_$1): save_%_$1: $(addprefix save_%_, $(addsuffix _$1, $(LANGUAGES)))

#$$(info Create targets: $(addprefix save_, $(addsuffix _$1, $(LANGUAGES))).)
targets_$1 = $(addprefix save_, $(addsuffix _$1, $(LANGUAGES)))
.PHONY: $$(targets_$1)
$$(targets_$1): save_%_$1: $(addprefix save_, $(addsuffix _%_$1, $(DISTROS)))

#$$(info Create targets: $(addprefix save_, $(addsuffix _cpp_$1, $(DISTROS))))
cpp_targets_$1 = $(addprefix save_, $(addsuffix _cpp_$1, $(DISTROS)))
#$$(info Create targets: $(addprefix save_, $(addsuffix _dotnet_$1, $(DISTROS))))
dotnet_targets_$1 = $(addprefix save_, $(addsuffix _dotnet_$1, $(DISTROS)))
#$$(info Create targets: $(addprefix save_, $(addsuffix _java_$1, $(DISTROS))))
java_targets_$1 = $(addprefix save_, $(addsuffix _java_$1, $(DISTROS)))
#$$(info Create targets: $(addprefix save_, $(addsuffix _python_$1, $(DISTROS))))
python_targets_$1 = $(addprefix save_, $(addsuffix _python_$1, $(DISTROS)))
.PHONY: $$(cpp_targets_$1) $$(dotnet_targets_$1) $$(java_targets_$1) $$(python_targets_$1)

$$(cpp_targets_$1): save_%_cpp_$1: cache/%/docker_cpp_$1.tar
cache/%/docker_cpp_$1.tar: %_cpp_$1
	@rm -f $$@
	mkdir -p cache/$$*
	docker save ${IMAGE}:$$*_cpp_$1 -o $$@
$$(dotnet_targets_$1): save_%_dotnet_$1: cache/%/docker_dotnet_$1.tar
cache/%/docker_dotnet_$1.tar: %_dotnet_$1
	@rm -f $$@
	mkdir -p cache/$$*
	docker save ${IMAGE}:$$*_dotnet_$1 -o $$@
$$(java_targets_$1): save_%_java_$1: cache/%/docker_java_$1.tar
cache/%/docker_java_$1.tar: %_java_$1
	@rm -f $$@
	mkdir -p cache/$$*
	docker save ${IMAGE}:$$*_java_$1 -o $$@
$$(python_targets_$1): save_%_python_$1: cache/%/docker_python_$1.tar
cache/%/docker_python_$1.tar: %_python_$1
	@rm -f $$@
	mkdir -p cache/$$*
	docker save ${IMAGE}:$$*_python_$1 -o $$@

# Run a container using the <distro>_<lang>_$1 image (debug).
#$$(info Create targets: $(addprefix sh_, $(addsuffix _cpp_$1, $(DISTROS))))
cpp_targets_$1 = $(addprefix sh_, $(addsuffix _cpp_$1, $(DISTROS)))
#$$(info Create targets: $(addprefix sh_, $(addsuffix _dotnet_$1, $(DISTROS))))
dotnet_targets_$1 = $(addprefix sh_, $(addsuffix _dotnet_$1, $(DISTROS)))
#$$(info Create targets: $(addprefix sh_, $(addsuffix _java_$1, $(DISTROS))))
java_targets_$1 = $(addprefix sh_, $(addsuffix _java_$1, $(DISTROS)))
#$$(info Create targets: $(addprefix sh_, $(addsuffix _python_$1, $(DISTROS))))
python_targets_$1 = $(addprefix sh_, $(addsuffix _python_$1, $(DISTROS)))
.PHONY: $$(cpp_targets_$1) $$(dotnet_targets_$1) $$(java_targets_$1) $$(python_targets_$1)

$$(cpp_targets_$1): sh_%_cpp_$1: %_cpp_$1
	${DOCKER_RUN_CMD} -it --name ${PROJECT}_${BUILD_SYSTEM}_$$*_cpp_$1 ${IMAGE}:$$*_cpp_$1
$$(dotnet_targets_$1): sh_%_dotnet_$1: %_dotnet_$1
	${DOCKER_RUN_CMD} -it --name ${PROJECT}_${BUILD_SYSTEM}_$$*_dotnet_$1 ${IMAGE}:$$*_dotnet_$1
$$(java_targets_$1): sh_%_java_$1: %_java_$1
	${DOCKER_RUN_CMD} -it --name ${PROJECT}_${BUILD_SYSTEM}_$$*_java_$1 ${IMAGE}:$$*_java_$1
$$(python_targets_$1): sh_%_python_$1: %_python_$1
	${DOCKER_RUN_CMD} -it --name ${PROJECT}_${BUILD_SYSTEM}_$$*_python_$1 ${IMAGE}:$$*_python_$1

# Clean $1 docker images.
#$$(info Create targets: clean_all_$1 $(addprefix clean_, $(addsuffix _$1, $(DISTROS))).)
targets_$1 = $(addprefix clean_, $(addsuffix _$1, $(DISTROS)))
.PHONY: clean_all_$1 $(targets_$1)
clean_all_$1: $$(targets_$1)
$$(targets_$1): clean_%_$1: $(addprefix clean_%_, $(addsuffix _$1, $(LANGUAGES)))

#$$(info Create targets: $(addprefix clean_, $(addsuffix _$1, $(LANGUAGES))).)
targets_$1 = $(addprefix clean_, $(addsuffix _$1, $(LANGUAGES)))
.PHONY: $$(targets_$1)
$$(targets_$1): clean_%_$1: $(addprefix clean_, $(addsuffix _%_$1, $(DISTROS)))

#$$(info Create targets: $(addprefix clean_, $(addsuffix _cpp_$1, $(DISTROS))))
cpp_targets_$1 = $(addprefix clean_, $(addsuffix _cpp_$1, $(DISTROS)))
#$$(info Create targets: $(addprefix clean_, $(addsuffix _dotnet_$1, $(DISTROS))))
dotnet_targets_$1 = $(addprefix clean_, $(addsuffix _dotnet_$1, $(DISTROS)))
#$$(info Create targets: $(addprefix clean_, $(addsuffix _java_$1, $(DISTROS))))
java_targets_$1 = $(addprefix clean_, $(addsuffix _java_$1, $(DISTROS)))
#$$(info Create targets: $(addprefix clean_, $(addsuffix _python_$1, $(DISTROS))))
python_targets_$1 = $(addprefix clean_, $(addsuffix _python_$1, $(DISTROS)))
.PHONY: $$(cpp_targets_$1) $$(dotnet_targets_$1) $$(java_targets_$1) $$(python_targets_$1)

$$(cpp_targets_$1): clean_%_cpp_$1:
	docker image rm -f ${IMAGE}:$$*_cpp_$1 2>/dev/null
	rm -f cache/$$*/docker_cpp_$1.tar
$$(dotnet_targets_$1): clean_%_dotnet_$1:
	docker image rm -f ${IMAGE}:$$*_dotnet_$1 2>/dev/null
	rm -f cache/$$*/docker_dotnet_$1.tar
$$(java_targets_$1): clean_%_java_$1:
	docker image rm -f ${IMAGE}:$$*_java_$1 2>/dev/null
	rm -f cache/$$*/docker_java_$1.tar
$$(python_targets_$1): clean_%_python_$1:
	docker image rm -f ${IMAGE}:$$*_python_$1 2>/dev/null
	rm -f cache/$$*/docker_python_$1.tar
endef

$(foreach stage,$(STAGES),$(eval $(call make-stage-target,$(stage))))

###############
## TOOLCHAIN ##
###############
TOOLCHAIN_TARGETS = \
 aarch64 aarch64be \
 mips64 mips64el \
 ppc64 ppc64le
TOOLCHAIN_STAGES = env devel build test

define toolchain-stage-target =
#$$(info STAGE: $1)
#$$(info Create targets: toolchain_$1 $(addsuffix _$1, $(TOOLCHAIN_TARGETS)).)
targets_toolchain_$1 = $(addsuffix _$1, $(TOOLCHAIN_TARGETS))
.PHONY: toolchain_$1 $$(targets_toolchain_$1)
toolchain_$1: $$(targets_toolchain_$1)
$$(targets_toolchain_$1): %_$1: docker/toolchain/Dockerfile
	#@docker image rm -f ${IMAGE}:$$*_$1 2>/dev/null
	${DOCKER_BUILD_CMD} \
 --tag ${IMAGE}:$$*_$1 \
 --build-arg TARGET=$$* \
 --target=$1 \
 -f $$< \
 ..

#$$(info Create targets: save_toolchain_$1 $(addprefix save_, $(addsuffix _$1, $(TOOLCHAIN_TARGETS))) (debug).)
save_targets_toolchain_$1 = $(addprefix save_, $(addsuffix _$1, $(TOOLCHAIN_TARGETS)))
.PHONY: save_toolchain_$1 $$(save_targets_toolchain_$1)
save_toolchain_$1: $$(save_targets_toolchain_$1)
$$(save_targets_toolchain_$1): save_%_$1: cache/%/docker_$1.tar
cache/%/docker_$1.tar: %_$1
	@rm -f $$@
	mkdir -p cache/$$*
	docker save ${IMAGE}:$$*_$1 -o $$@

#$$(info Create targets: $(addprefix sh_, $(addsuffix _$1, $(TOOLCHAIN_TARGETS))) (debug).)
sh_targets_toolchain_$1 = $(addprefix sh_, $(addsuffix _$1, $(TOOLCHAIN_TARGETS)))
.PHONY: $$(sh_targets_toolchain_$1)
$$(sh_targets_toolchain_$1): sh_%_$1: %_$1
	${DOCKER_RUN_CMD} -it --name ${IMAGE}_$$*_$1 ${IMAGE}:$$*_$1

#$$(info Create targets: clean_toolchain_$1 $(addprefix clean_, $(addsuffix _$1, $(TOOLCHAIN_TARGETS))).)
clean_targets_toolchain_$1 = $(addprefix clean_, $(addsuffix _$1, $(TOOLCHAIN_TARGETS)))
.PHONY: clean_toolchain_$1 $$(clean_targets_toolchain_$1)
clean_toolchain_$1: $$(clean_targets_toolchain_$1)
$$(clean_targets_toolchain_$1): clean_%_$1:
	docker image rm -f ${IMAGE}:$$*_$1 2>/dev/null
	rm -f cache/$$*/docker_$1.tar
endef

$(foreach stage,$(TOOLCHAIN_STAGES),$(eval $(call toolchain-stage-target,$(stage))))

## MERGE ##
.PHONY: clean_toolchains
clean_toolchains: $(addprefix clean_toolchain_, $(TOOLCHAIN_STAGES))
	-rmdir $(addprefix cache/, $(TOOLCHAIN_TARGETS))

#############
## VAGRANT ##
#############
VMS = freebsd

targets_freebsd = $(addprefix freebsd_, $(LANGUAGES))
.PHONY: freebsd $(targets_freebsd)
freebsd: $(targets_freebsd)
$(targets_freebsd): freebsd_%: vagrant/freebsd/%/Vagrantfile
	@cd vagrant/freebsd/$* && vagrant destroy -f
	cd vagrant/freebsd/$* && vagrant box update
	cd vagrant/freebsd/$* && vagrant up

# SSH to a freebsd_<lang> vagrant machine (debug).
targets_freebsd = $(addprefix sh_freebsd_, $(LANGUAGES))
.PHONY: $(targets_freebsd)
$(targets_freebsd): sh_freebsd_%:
	cd vagrant/freebsd/$* && vagrant up
	cd vagrant/freebsd/$* && vagrant ssh

# Clean FreeBSD vagrant machine
targets_freebsd = $(addprefix clean_freebsd_, $(LANGUAGES))
.PHONY: clean_freebsd $(targets_freebsd)
clean_freebsd: $(targets_freebsd)
$(targets_freebsd): clean_freebsd_%:
	cd vagrant/freebsd/$* && vagrant destroy -f
	-rm -rf vagrant/freebsd/$*/.vagrant

##########
## GLOP ##
##########
glop_targets = $(addprefix glop_, $(STAGES))
.PHONY: $(glop_targets)
$(glop_targets): glop_%: docker/glop/Dockerfile
	#@docker image rm -f ${IMAGE}:$@ 2>/dev/null
	${DOCKER_BUILD_CMD} --target=$* --tag ${IMAGE}:$@ -f $< ..

save_glop_targets = $(addprefix save_glop_, $(STAGES))
.PHONY: $(save_glop_targets)
$(save_glop_targets): save_glop_%: cache/glop/docker_%.tar
cache/glop/docker_%.tar: glop_%
	@rm -f $@
	mkdir -p cache/$*
	docker save ${IMAGE}:$< -o $@

sh_glop_targets = $(addprefix sh_glop_, $(STAGES))
.PHONY: $(sh_glop_targets)
$(sh_glop_targets): sh_glop_%: glop_%
	${DOCKER_RUN_CMD} -it --name ${PROJECT}_${BUILD_SYSTEM}_$< ${IMAGE}:$<

clean_glop_targets = $(addprefix clean_glop_, $(STAGES))
.PHONY: $(clean_glop_targets)
$(clean_glop_targets): clean_glop_%
	docker image rm -f ${IMAGE}:glop_$* 2>/dev/null
	rm -f cache/glop/docker_$*.tar

###########
## CLEAN ##
###########
targets = $(addprefix clean_, $(DISTROS))
vms = $(addprefix clean_, $(VMS))
glops = $(addprefix clean_glop_, $(STAGES))
.PHONY: clean $(targets)
clean: $(targets) $(vms) $(glops)
	docker container prune -f
	docker image prune -f
	-rmdir cache/glop
	-rmdir cache
$(targets): clean_%: $(addprefix clean_%_, $(PRESTAGES)) $(addprefix clean_%_, $(STAGES))
	-rmdir cache/$*

.PHONY: distclean
distclean: clean
	-docker container rm -f $$(docker container ls -aq)
	-docker image rm -f $$(docker image ls -aq)
	-vagrant box remove -f generic/freebsd12
