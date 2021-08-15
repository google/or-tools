# Introduction

This file describes how to use OR-Tools in python with the binary archives.

OR-Tools online documentation is
[here](https://developers.google.com/optimization)

Full installation instructions are located
[here](https://developers.google.com/optimization/install/python/)

These modules have been tested under:

  - Ubuntu 18.04 LTS and up (64 bit).
  - CentOS 8 (64 bit).
  - Debian 10 (64 bit).
  - MacOS 10.15 Catalina (64 bit).
  - Microsoft Windows with Visual Studio 2019 (64-bit)

Upon decompressing the archive, you will get the following structure:

```
or-tools/
  [LICENSE-2.0.txt](LICENSE-2.0.txt)     <- Apache License
  [README.md](README.md)                 <- This file
  [examples/data/](examples/data)        <- Data for the examples
  [examples/python](examples/python)     <- Python examples
  [examples/notebook](examples/notebook) <- Jupyter/IPython notebooks
```

# Installation

To install the package:

1. Make sure python and pip are installed

2. Make sure pip is up to date:

```shell
python -m pip -V
```

should return 9.0.1  otherwise, run: `python -m pip install --upgrade --user pip`

3. Fetch `ortools` from Pypi:

```shell
python -m pip install --upgrade --user ortools
```

It should pull the latest version of OR-Tools.

# Run Examples

To run a first example:

```shell
python examples/python/hidato_table.py
```
