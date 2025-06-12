# Copyright 2010-2025 Google LLC
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

ifdef NOCACHE
DOCKER_BUILD_CMD := docker build --no-cache
else
DOCKER_BUILD_CMD := docker build
endif
DOCKER_RUN_CMD := docker run --rm --init

MZN_SUFFIX=2025v2
DOCKER_NAME=cp-sat-minizinc-challenge
MZN_TAG=${DOCKER_NAME}:${MZN_SUFFIX}
MZN_LS_TAG=${DOCKER_NAME}-ls:${MZN_SUFFIX}

# Standard CP-SAT build.
.PHONY: minizinc-challenge
minizinc-challenge: minizinc-challenge.Dockerfile
	${DOCKER_BUILD_CMD} -f minizinc-challenge.Dockerfile -t ${MZN_TAG} .

.PHONY: minizinc-challenge-test
minizinc-challenge-test: minizinc-challenge
	${DOCKER_RUN_CMD} ${MZN_TAG} solver -v /minizinc/test.mzn /minizinc/2.dzn
	${DOCKER_RUN_CMD} ${MZN_TAG} solver -v --free-search /minizinc/test.mzn /minizinc/2.dzn
	${DOCKER_RUN_CMD} ${MZN_TAG} solver -v -p 2 /minizinc/test.mzn /minizinc/2.dzn

.PHONY: minizinc-challenge-check
minizinc-challenge-check: minizinc-challenge
	${DOCKER_RUN_CMD} ${MZN_TAG} grep FZNEXEC /minizinc/mzn-exec-fd

.PHONY: minizinc-challenge-bash
minizinc-challenge-bash:
	${DOCKER_RUN_CMD} -it --name minizinc_chalenge ${MZN_TAG}

.PHONY: minizinc-challenge-export
minizinc-challenge-export: minizinc-challenge
	docker tag ${MZN_TAG} laurentperron/${MZN_TAG}
	docker push laurentperron/${MZN_TAG}


# Local search build.
.PHONY: minizinc-challenge-ls
minizinc-challenge-ls: minizinc-challenge-ls.Dockerfile
	${DOCKER_BUILD_CMD} -f minizinc-challenge-ls.Dockerfile -t ${MZN_LS_TAG} .

.PHONY: minizinc-challenge-ls-test
minizinc-challenge-ls-test: minizinc-challenge-ls
	${DOCKER_RUN_CMD} ${MZN_LS_TAG} solver -v /minizinc/test.mzn /minizinc/2.dzn
	${DOCKER_RUN_CMD} ${MZN_LS_TAG} solver -v --free-search /minizinc/test.mzn /minizinc/2.dzn
	${DOCKER_RUN_CMD} ${MZN_LS_TAG} solver -v -p 2 /minizinc/test.mzn /minizinc/2.dzn

.PHONY: minizinc-challenge-ls-check
minizinc-challenge-ls-check: minizinc-challenge-ls
	${DOCKER_RUN_CMD} ${MZN_LS_TAG} grep FZNEXEC /minizinc/mzn-exec-fd
	${DOCKER_RUN_CMD} ${MZN_LS_TAG} grep FZNEXEC /minizinc/mzn-exec-free
	${DOCKER_RUN_CMD} ${MZN_LS_TAG} grep FZNEXEC /minizinc/mzn-exec-par

.PHONY: minizinc-challenge-ls-bash
minizinc-challenge-ls-bash:
	${DOCKER_RUN_CMD} -it --name minizinc_chalenge-ls ${MZN_LS_TAG}

.PHONY: minizinc-challenge-ls-export
minizinc-challenge-ls-export: minizinc-challenge-ls
	docker tag ${MZN_LS_TAG} laurentperron/${MZN_LS_TAG}
	docker push laurentperron/${MZN_LS_TAG}
