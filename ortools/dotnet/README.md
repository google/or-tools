# Google OrTools

## Pre-requisites
- .Net Core SDK >= 2.1.302

## Build
The library is compiled against `netstandard2.0`. 

Either use the makefile or you can build in Visual Studio. The workflow is typically
`make test_dotnet` which will build both C# and F# libraries in debug mode. The output will be placed in `<OR_ROOT>/dotnet-test` folder. All tests will be run based on this folder. When you are ready to package the application `make dotnet` will change the version and install a release version in `bin` and in `build`.

## Examples
The Test projects show an example of building the application with `netcoreapp2.0`. 

The F# example folder shows how to compile against the typical .NET Framework installed on machine. Before compiling be sure to run `nuget install -o ./packages` in the example folder. 
