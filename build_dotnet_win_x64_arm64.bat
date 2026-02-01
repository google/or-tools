@echo off

if %PROCESSOR_ARCHITECTURE%==AMD64 (
echo "build on x64"
cmake -S . -B build_x64 -DBUILD_DEPS=ON -DBUILD_DOTNET=ON
cmake --build build_x64 --config Release --target ALL_BUILD -j -v
cmake -A ARM64 -S . -B build_arm64 -DBUILD_DEPS=ON -DBUILD_DOTNET=ON -DOR_TOOLS_PROTOC_EXECUTABLE=..\build_x64\Release\bin\protoc.exe
cmake --build build_arm64 --config Release --target ALL_BUILD -j -v
)

if %PROCESSOR_ARCHITECTURE%==ARM64 (
echo "build on arm64"
cmake -S . -B build_arm64 -DBUILD_DEPS=ON -DBUILD_DOTNET=ON
cmake --build build_arm64 --config Release --target ALL_BUILD -j -v
cmake -A x64 -S . -B build_x64 -DBUILD_DEPS=ON -DBUILD_DOTNET=ON -DOR_TOOLS_PROTOC_EXECUTABLE=..\build_arm64\Release\bin\protoc.exe
cmake --build build_x64 --config Release --target ALL_BUILD -j -v
)

