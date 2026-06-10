#!/usr/bin/env bash
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

# run_gen_pb_docker.sh <path_to_ORToolsGenerated.jl>

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <path_to_ORToolsGenerated.jl>"
    exit 1
fi

LOCAL_PATH=$(realpath "$1")
SCRIPT_DIR=$(dirname "$0")

if [ ! -d "$LOCAL_PATH" ]; then
    echo "Error: $LOCAL_PATH is not a directory."
    exit 1
fi

# Detect if Docker or Podman is available
if command -v docker >/dev/null 2>&1; then
    DOCKER_CMD="docker"
elif command -v podman >/dev/null 2>&1; then
    DOCKER_CMD="podman"
else
    echo "Error: Neither docker nor podman is available."
    exit 1
fi

# Build the image.
echo "Building image using $DOCKER_CMD..."
$DOCKER_CMD build -t gen_pb -f "$SCRIPT_DIR/gen_pb.Dockerfile" "$SCRIPT_DIR"

# Run the image and mount the volume.
echo "Running gen_pb.jl in container using $DOCKER_CMD..."
$DOCKER_CMD run --rm -v "$LOCAL_PATH":/workspace/ORToolsGenerated.jl gen_pb "julia --project=/workspace/ORToolsGenerated.jl -e 'using Pkg; Pkg.instantiate()' && julia --project=/workspace/ORToolsGenerated.jl /workspace/ORToolsGenerated.jl/scripts/gen_pb.jl"
