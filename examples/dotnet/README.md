# Dotnet Core examples

The following examples showcase how to use OrTools. The project solution has examples for both C# and F#.

We recommend that all projects you create target `netcoreapp2.0` as this allows you to compile for various frameworks and keep up-to-date with the latest frameworks.

Wherever you have ortools installed, be sure to reference the `Google.OrTools.dll` from the project file. You will also need to reference the library folder housing native libraries. 

### Linux
To reference a particular folder on linux, you can either: explicitly set the **LD_LIBRARY_PATH**; or create a new configuration file with the path of the library folder in `/etc/ld.so.conf.d/` and then run `sudo ldconfig`. The former will set the path on a system level so that you don't have to use the environment.

### MacOS
To reference a particular folder on linux, you can explicitly set the **DYLD_LIBRARY_PATH**

## CSharp/FSharp project examples

By default all the examples are compiled in a console application with the startup object being the **Classname.Main** so that when compiled the entrypoint will be known.

## NetFx/FSharp compiler examples

Should you have another **netfx** you can compile individual file examples. Please see readme in the individual folders for the target language.
