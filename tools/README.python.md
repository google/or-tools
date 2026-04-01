# Introduction

This file describes how to use OR-Tools in python with the binary archives.

OR-Tools online documentation is located at
https://developers.google.com/optimization

Full installation instructions are located at
https://developers.google.com/optimization/install/python/

These modules have been tested under:

  - AlmaLinux 9 (64-bit).
  - Alpine Edge (64-bit).
  - ArchLinux (64-bit).
  - Debian 11 and up (64-bit).
  - Fedora 40 and up (64-bit).
  - OpenSuse Leap (64-bit).
  - Rocky Linux 9 (64-bit)
  - Ubuntu 20.04 LTS and up (64-bit).
  - MacOS 12.5 Monterey (64-bit).
  - Microsoft Windows with Visual Studio 2019 and 2022 (64-bit)

## Codemap

Upon decompressing the archive, you will get the following structure:

```
or-tools/
  [LICENSE](LICENSE)                     <- Apache License.
  [README.md](README.md)                 <- This file.
  [examples/python](examples/python)     <- Python examples.
  [examples/notebook](examples/notebook) <- Jupyter/IPython notebooks.
```

Warning: While OR-Tools ships with SCIP, please consult the SCIP license
to ensure that you are complying with it if you want to use this solver.

## Installation

To install the package:

1. Make sure Python and pip are installed

2. Make sure pip is up to date: the command

```shell
python -m pip -V
```

should return 9.0.1 or a more recent version. Otherwise, update pip manually:
`python -m pip install --upgrade --user pip`

3. Fetch `ortools` from Pypi:

```shell
python -m pip install --upgrade --user ortools
```

It should pull the latest version of OR-Tools.

## Usage

To run a first example:

```shell
python examples/python/hidato_table.py
```
