#!/bin/bash
# Adapted from
# http://marcelbraghetto.github.io/a-simple-triangle/2019/03/10/part-06/

fetch_brew_dependency() {
    FORMULA_NAME=$1

    echo "Fetching Brew dependency: '$FORMULA_NAME'."

    if brew ls --versions $FORMULA_NAME > /dev/null; then
        echo "Dependency '$FORMULA_NAME' is already installed, continuing ..."
    else
        echo "Dependency '$FORMULA_NAME' is not installed, installing via Homebrew ..."
        brew install $FORMULA_NAME
    fi
}

fetch_brew_dependency "wget"
fetch_brew_dependency "cmake"

# If required, download and configure the Emscripten SDK.

if [ ! -d "emscripten" ]; then
    echo "Fetching Emscripten SDK ..."

    # Download the Emscripten SDK as a zip file from GitHub.
    wget https://github.com/emscripten-core/emsdk/archive/master.zip

    # Unzip the Emscripten SDK.
    unzip -q master.zip

    # Rename it to 'emscripten'.
    mv emsdk-master emscripten

    # Clean up the zip file we downloaded.
    rm master.zip

    pushd emscripten
        echo "Updating Emscripten SDK ..."
        ./emsdk update

        echo "Installing latest Emscripten SDK ..."
        ./emsdk install latest

        echo "Activating latest Emscripten SDK ..."
        ./emsdk activate latest
    popd
fi
