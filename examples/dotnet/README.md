# .NetCoreApp examples

The following examples showcase how to use OrTools.<br>
The project solution has examples for C#.

We recommend that all projects you create target `net8.0`,
as this allows you to compile for various frameworks and
keep up-to-date with the latest frameworks.

Wherever you have or-tools installed, be sure to `PackageReference` the `Google.OrTools`
from the project file.

## Execution
Running the examples will involve building them, then running them.<br>
You can run the following command:
```shell
dotnet build <example>.csproj
dotnet run --no-build --project <example>.csproj
```
