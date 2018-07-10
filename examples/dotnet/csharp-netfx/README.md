# Examples of using or-tools in C#

This file describes how to use the or-tools .NET binary archive in C# 

## Execution

Running the examples will involve compiling them, then running them. You can run the following command for your target operating system.

For example you can compile and run `csflow.cs`. This assumes you have the archive library in a folder called `bin`.


### Windows (32 bit)
```
csc /target:exe /out:bin\csflow.exe /platform:x86 /lib:bin /r:Google.OrTools.dll examples\csharp\csflow.cs 
bin\csflow.exe
```

### Windows (64 bit)
```
csc /target:exe /out:bin/csflow.exe /platform:x64 /lib:bin /r:Google.OrTools.dll  examples\csharp\csflow.cs
bin\csflow.exe
```

### Linux (framework 4.6+ via mono must be installed)
```
mcs /target:exe /out:bin/csflow.exe /platform:anycpu /lib:bin /r:Google.OrTools.dll examples/csharp/csflow.cs
mono bin/csflow.exe
```

### Mac OS X (framework 4.6+ via mono must be installed)
```
mcs /target:exe /out:bin/csflow.exe /platform:anycpu /lib:bin /r:Google.OrTools.dll examples/csharp/csflow.cs
DYLD_FALLBACK_LIBRARY_PATH=lib mono64 bin/csflow.exe
```