# General commands
.PHONY: help
help:
	@echo "Tools to generate various deliveries for linux distros"
	@echo "use NOCACHE=1 to \"docker build --no-cache\""
	@echo "<distro>: debian-9, centos-7, ubuntu-16.04, ubuntu-18.04, ubuntu-18.10, all"
	@echo "<language>: cc, java, dotnet, all"
	@echo
	@echo "usage:"
	@echo "make help: Print this help."
	@echo "make delivery: Build 'archives' and 'python' targets."
	@echo "make test_delivery: Build 'test_archives' and 'test_python' targets."
	@echo
	@echo "make archives: Build all OR-Tools archives in export."
	@echo "make test_archives: Test all OR-Tools archives in export for each language."
	@echo "make python: Build manylinux python 'ortools' wheel packages (2.7, 3.5, 3.6, 3.7)."
	@echo "make test_python: Test manylinux python 'ortools' wheel packages (2.7, 3.5, 3.6, 3.7)."
	@echo
	@echo "make docker_<distro>: Build image containing or-tools with its third party prebuilt."
	@echo "make bash_<distro>: Run container using the docker_<distro> image."
	@echo "make archive_<distro>: Build OR-Tools archive for the specified <distro>."
	@echo "make docker_<distro>_<lang>: Build test image containing or-tools archive and <lang> prerequisites."
	@echo "make test_<distro>_<lang>: Test OR-Tools archive for the specified <distro> and <lang>."
	@echo "make bash_<distro>_<lang>: Run container using the docker_<distro>_<lang> image."
	@echo "make test_<distro>: Test OR-Tools archive for all specified language for the specified <distro>."
	@echo
	@echo "make test_<distro>_<language>: Test OR-Tools archive on <distro> distro for <language> language."
	@echo "make clean: Clean all docker images but keep archives (aka don\'t touch export directory)."
	@echo "make distclean: Clean all docker images and remove all archives."
	@echo

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

.PHONY: python
python: manylinux1-pypi

export/python/build-manylinux1.sh: build-manylinux1.sh | export/python
	cp $< $@

export/python/docker.tar: manylinux1.Dockerfile export/python/build-manylinux1.sh ../../makefiles | export/python
	-docker image rm -f or-tools:python 2>/dev/null
	$(DOCKER_BUILD_CMD) \
 -f $< \
 --build-arg SRC_GIT_BRANCH=$(OR_TOOLS_BRANCH) \
 --build-arg SRC_GIT_SHA1=$(OR_TOOLS_SHA1) \
 -t or-tools:python export/python
	docker save or-tools:python -o $@

manylinux1-pypi: export/python/docker.tar
	docker load -i $<
	$(DOCKER_RUN_CMD) -v `pwd`/export:/export or-tools:python /bin/bash -c \
		"/root/build/build-manylinux1.sh /root/src /root/build /export"

bash_python: export/python/docker.tar
	$(DOCKER_RUN_CMD) -v `pwd`/export:/export or-tools:python /bin/bash

export/python: | export
	-mkdir $@

#################
###  ARCHIVES  ##
#################

# Create build docker images with OR-Tools built
.PHONY: docker \
docker_centos-7 \
docker_debian-9 \
docker_ubuntu-16.04 \
docker_ubuntu-18.04 \
docker_ubuntu-18.10
docker: \
docker_centos-7 \
docker_debian-9 \
docker_ubuntu-16.04 \
docker_ubuntu-18.04 \
docker_ubuntu-18.10
docker_centos-7: export/centos-7/docker.tar
docker_debian-9: export/debian-9/docker.tar
docker_ubuntu-16.04: export/ubuntu-16.04/docker.tar
docker_ubuntu-18.04: export/ubuntu-18.04/docker.tar
docker_ubuntu-18.10: export/ubuntu-18.10/docker.tar

# Performance: `docker build` use export/$* to reduce the size of the context
# since everything inside this directory is sent to the docker daemon
# before the image is built...
export/%/docker.tar: %.Dockerfile ../Makefile.cc.java.dotnet ../../makefiles ../../ortools | export/%
	-docker image rm -f or-tools_$*:devel 2>/dev/null
	-rm -f export/$*/docker.tar
	cp or-tools.snk export/$*/
	$(DOCKER_BUILD_CMD) \
 -f $< \
 --build-arg SRC_GIT_BRANCH=$(OR_TOOLS_BRANCH) \
 --build-arg SRC_GIT_SHA1=$(OR_TOOLS_SHA1) \
 -t or-tools_$*:devel export/$*
	docker save or-tools_$*:devel -o $@

# Run container using devel docker image
.PHONY: bash_centos-7 bash_debian-9 bash_ubuntu-16.04 bash_ubuntu-18.04 bash_ubuntu-18.10
bash_centos-7: export/centos-7/docker.tar
	$(DOCKER_RUN_CMD) -v `pwd`/export:/export or-tools_centos-7:devel /bin/bash
bash_debian-9: export/debian-9/docker.tar
	$(DOCKER_RUN_CMD) -v `pwd`/export:/export or-tools_debian-9:devel /bin/bash
bash_ubuntu-16.04: export/ubuntu-16.04/docker.tar
	$(DOCKER_RUN_CMD) -v `pwd`/export:/export or-tools_ubuntu-16.04:devel /bin/bash
bash_ubuntu-18.04: export/ubuntu-18.04/docker.tar
	$(DOCKER_RUN_CMD) -v `pwd`/export:/export or-tools_ubuntu-18.04:devel /bin/bash
bash_ubuntu-18.10: export/ubuntu-18.10/docker.tar
	$(DOCKER_RUN_CMD) -v `pwd`/export:/export or-tools_ubuntu-18.10:devel /bin/bash

# Build Archives
.PHONY: archives \
archive_centos-7 \
archive_debian-9 \
archive_ubuntu-16.04 \
archive_ubuntu-18.04 \
archive_ubuntu-18.10
archives: \
archive_centos-7 \
archive_debian-9 \
archive_ubuntu-16.04 \
archive_ubuntu-18.04 \
archive_ubuntu-18.10
archive_centos-7: export/archives/or-tools_centos-7_v$(OR_TOOLS_VERSION).tar.gz
archive_debian-9: export/archives/or-tools_debian-9_v$(OR_TOOLS_VERSION).tar.gz
archive_ubuntu-16.04: export/archives/or-tools_ubuntu-16.04_v$(OR_TOOLS_VERSION).tar.gz
archive_ubuntu-18.04: export/archives/or-tools_ubuntu-18.04_v$(OR_TOOLS_VERSION).tar.gz
archive_ubuntu-18.10: export/archives/or-tools_ubuntu-18.10_v$(OR_TOOLS_VERSION).tar.gz

export/archives/or-tools_%_v$(OR_TOOLS_VERSION).tar.gz: export/%/docker.tar | export/archives
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
export/centos-7: | export
	-mkdir $@
export/debian-9: | export
	-mkdir $@
export/ubuntu-16.04: | export
	-mkdir $@
export/ubuntu-18.04: | export
	-mkdir $@
export/ubuntu-18.10: | export
	-mkdir $@

############
##  TEST  ##
############

# Create test docker image for each language
.PHONY: \
docker_centos-7_cc docker_centos-7_java docker_centos-7_dotnet \
docker_debian-9_cc docker_debian-9_java docker_debian-9_dotnet \
docker_ubuntu-16.04_cc docker_ubuntu-16.04_java docker_ubuntu-16.04_dotnet \
docker_ubuntu-18.04_cc docker_ubuntu-18.04_java docker_ubuntu-18.04_dotnet \
docker_ubuntu-18.10_cc docker_ubuntu-18.10_java docker_ubuntu-18.10_dotnet
docker_centos-7_cc: export/centos-7/docker_cc.tar
docker_centos-7_java: export/centos-7/docker_java.tar
docker_centos-7_donet: export/centos-7/docker_dotnet.tar

docker_debian-9_cc: export/debian-9/docker_cc.tar
docker_debian-9_java: export/debian-9/docker_java.tar
docker_debian-9_dotnet: export/debian-9/docker_dotnet.tar

docker_ubuntu-16.04_cc: export/ubuntu-16.04/docker_cc.tar
docker_ubuntu-16.04_java: export/ubuntu-16.04/docker_java.tar
docker_ubuntu-16.04_dotnet: export/ubuntu-16.04/docker_dotnet.tar

docker_ubuntu-18.04_cc: export/ubuntu-18.04/docker_cc.tar
docker_ubuntu-18.04_java: export/ubuntu-18.04/docker_java.tar
docker_ubuntu-18.04_dotnet: export/ubuntu-18.04/docker_dotnet.tar

docker_ubuntu-18.10_cc: export/ubuntu-18.10/docker_cc.tar
docker_ubuntu-18.10_java: export/ubuntu-18.10/docker_java.tar
docker_ubuntu-18.10_dotnet: export/ubuntu-18.10/docker_dotnet.tar

export/%/docker_cc.tar: test/%/cc.Dockerfile export/archives/or-tools_%_v$(OR_TOOLS_VERSION).tar.gz
	-docker image rm -f or-tools_$*:cc 2>/dev/null
	$(DOCKER_BUILD_CMD) -f $< -t or-tools_$*:cc export/archives
	docker save or-tools_$*:cc -o $@
export/%/docker_java.tar: test/%/java.Dockerfile export/archives/or-tools_%_v$(OR_TOOLS_VERSION).tar.gz
	-docker image rm -f or-tools_$*:java 2>/dev/null
	$(DOCKER_BUILD_CMD) -f $< -t or-tools_$*:java export/archives
	docker save or-tools_$*:java -o $@
export/%/docker_dotnet.tar: test/%/dotnet.Dockerfile export/archives/or-tools_%_v$(OR_TOOLS_VERSION).tar.gz
	-docker image rm -f or-tools_$*:dotnet 2>/dev/null
	$(DOCKER_BUILD_CMD) -f $< -t or-tools_$*:dotnet export/archives
	docker save or-tools_$*:dotnet -o $@

# Run container using <language> docker image
.PHONY: bash_centos-7_cc bash_centos-7_java bash_centos-7_dotnet
bash_centos-7_cc: export/centos-7/docker_cc.tar
	$(DOCKER_RUN_CMD) or-tools_centos-7:cc /bin/bash
bash_centos-7_java: export/centos-7/docker_java.tar
	$(DOCKER_RUN_CMD) or-tools_centos-7:java /bin/bash
bash_centos-7_dotnet: export/centos-7/docker_dotnet.tar
	$(DOCKER_RUN_CMD) or-tools_centos-7:dotnet /bin/bash

.PHONY: bash_debian-9_cc bash_debian-9_java bash_debian-9_dotnet
bash_debian-9_cc: export/debian-9/docker_cc.tar
	$(DOCKER_RUN_CMD) or-tools_debian-9:cc /bin/bash
bash_debian-9_java: export/debian-9/docker_java.tar
	$(DOCKER_RUN_CMD) or-tools_debian-9:java /bin/bash
bash_debian-9_dotnet: export/debian-9/docker_dotnet.tar
	$(DOCKER_RUN_CMD) or-tools_debian-9:dotnet /bin/bash

.PHONY: bash_ubuntu-16.04_cc bash_ubuntu-16.04_java bash_ubuntu-16.04_dotnet
bash_ubuntu-16.04_cc: export/ubuntu-16.04/docker_cc.tar
	$(DOCKER_RUN_CMD) or-tools_ubuntu-16.04:cc /bin/bash
bash_ubuntu-16.04_java: export/ubuntu-16.04/docker_java.tar
	$(DOCKER_RUN_CMD) or-tools_ubuntu-16.04:java /bin/bash
bash_ubuntu-16.04_dotnet: export/ubuntu-16.04/docker_dotnet.tar
	$(DOCKER_RUN_CMD) or-tools_ubuntu-16.04:dotnet /bin/bash

.PHONY: bash_ubuntu-18.04_cc bash_ubuntu-18.04_java bash_ubuntu-18.04_dotnet
bash_ubuntu-18.04_cc: export/ubuntu-18.04/docker_cc.tar
	$(DOCKER_RUN_CMD) or-tools_ubuntu-18.04:cc /bin/bash
bash_ubuntu-18.04_java: export/ubuntu-18.04/docker_java.tar
	$(DOCKER_RUN_CMD) or-tools_ubuntu-18.04:java /bin/bash
bash_ubuntu-18.04_dotnet: export/ubuntu-18.04/docker_dotnet.tar
	$(DOCKER_RUN_CMD) or-tools_ubuntu-18.04:dotnet /bin/bash

.PHONY: bash_ubuntu-18.10_cc bash_ubuntu-18.10_java bash_ubuntu-18.10_dotnet
bash_ubuntu-18.10_cc: export/ubuntu-18.10/docker_cc.tar
	$(DOCKER_RUN_CMD) or-tools_ubuntu-18.10:cc /bin/bash
bash_ubuntu-18.10_java: export/ubuntu-18.10/docker_java.tar
	$(DOCKER_RUN_CMD) or-tools_ubuntu-18.10:java /bin/bash
bash_ubuntu-18.10_dotnet: export/ubuntu-18.10/docker_dotnet.tar
	$(DOCKER_RUN_CMD) or-tools_ubuntu-18.10:dotnet /bin/bash

# Test Archive
.PHONY: test_archives test_archives_cc test_archives_java test_archives_dotnet \
test_centos-7 \
test_debian-9 \
test_ubuntu-16.04 \
test_ubuntu-18.04 \
test_ubuntu-18.10
test_archives: \
test_centos-7 \
test_debian-9 \
test_ubuntu-16.04 \
test_ubuntu-18.04 \
test_ubuntu-18.10

test_archives_cc: \
test_centos-7_cc \
test_debian-9_cc \
test_ubuntu-16.04_cc \
test_ubuntu-18.04_cc \
test_ubuntu-18.10_cc

test_archives_java: \
test_centos-7_java \
test_debian-9_java \
test_ubuntu-16.04_java \
test_ubuntu-18.04_java \
test_ubuntu-18.10_java

test_archives_dotnet: \
test_centos-7_dotnet \
test_debian-9_dotnet \
test_ubuntu-16.04_dotnet \
test_ubuntu-18.04_dotnet \
test_ubuntu-18.10_dotnet

test_centos-7: test_centos-7_cc test_centos-7_java test_centos-7_dotnet
test_debian-9: test_debian-9_cc test_debian-9_java test_debian-9_dotnet
test_ubuntu-16.04: test_ubuntu-16.04_cc test_ubuntu-16.04_java test_ubuntu-16.04_dotnet
test_ubuntu-18.04: test_ubuntu-18.04_cc test_ubuntu-18.04_java test_ubuntu-18.04_dotnet
test_ubuntu-18.10: test_ubuntu-18.10_cc test_ubuntu-18.10_java test_ubuntu-18.10_dotnet

.PHONY: test_centos-7_cc test_centos-7_java test_centos-7_dotnet
test_centos-7_cc: export/centos-7/test_cc.log
test_centos-7_java: export/centos-7/test_java.log
test_centos-7_dotnet: export/centos-7/test_dotnet.log

.PHONY: test_debian-9_cc test_debian-9_java test_debian-9_dotnet
test_debian-9_cc: export/debian-9/test_cc.log
test_debian-9_java: export/debian-9/test_java.log
test_debian-9_dotnet: export/debian-9/test_dotnet.log

.PHONY: test_ubuntu-16.04_cc test_ubuntu-16.04_java test_ubuntu-16.04_dotnet
test_ubuntu-16.04_cc: export/ubuntu-16.04/test_cc.log
test_ubuntu-16.04_java: export/ubuntu-16.04/test_java.log
test_ubuntu-16.04_dotnet: export/ubuntu-16.04/test_dotnet.log

.PHONY: test_ubuntu-18.04_cc test_ubuntu-18.04_java test_ubuntu-18.04_dotnet
test_ubuntu-18.04_cc: export/ubuntu-18.04/test_cc.log
test_ubuntu-18.04_java: export/ubuntu-18.04/test_java.log
test_ubuntu-18.04_dotnet: export/ubuntu-18.04/test_dotnet.log

.PHONY: test_ubuntu-18.10_cc test_ubuntu-18.10_java test_ubuntu-18.10_dotnet
test_ubuntu-18.10_cc: export/ubuntu-18.10/test_cc.log
test_ubuntu-18.10_java: export/ubuntu-18.10/test_java.log
test_ubuntu-18.10_dotnet: export/ubuntu-18.10/test_dotnet.log

# tee capture status so we enable pipefail to get the return of `docker run` instead.
export/%/test_cc.log: export/%/docker_cc.tar
	docker load -i $<
	$(DOCKER_RUN_CMD) or-tools_$*:cc /bin/sh -c "cd or-tools_*_v* && make test_cc"
export/%/test_java.log: export/%/docker_java.tar
	docker load -i $<
	$(DOCKER_RUN_CMD) or-tools_$*:java /bin/sh -c "cd or-tools_*_v* && make test_java"
export/%/test_dotnet.log: export/%/docker_dotnet.tar
	docker load -i $<
	$(DOCKER_RUN_CMD) or-tools_$*:dotnet /bin/sh -c "cd or-tools_*_v* && make test_dotnet"

#############
##  CLEAN  ##
#############
.PHONY: clean
clean: clean_containers clean_images

.PHONY: clean_images
clean_images: clean_containers
	-docker image ls --all
	-docker image prune --force

.PHONY: clean_containers
clean_containers:
	-docker container ls --all
	-docker container rm $$(docker container ls -f status=exited -q)

.PHONY: distclean
distclean: clean
	-docker container rm $$(docker container ls --all -q)
	-docker image rm $$(docker image ls --all -q)
	rm -rf export

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
