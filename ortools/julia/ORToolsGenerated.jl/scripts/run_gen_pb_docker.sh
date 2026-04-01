#!/usr/bin/env bash
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
