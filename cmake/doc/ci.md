# CI: Makefile/Docker/Vagrant testing
To test the build on various distro, I'm using docker containers and a Makefile for orchestration.

pros:
* You are independent of third party CI runner config (e.g. github actions runners or Travis-CI VM images).
* You can run it locally on your linux system.
* Most CI provide runner with docker and Makefile already installed (e.g. tarvis-ci [minimal images](https://docs.travis-ci.com/user/languages/minimal-and-generic/).

cons:
* Only GNU/Linux distro supported.
* Could take few GiB (~30 GiB for all distro and all languages)
  * ~500MiB OS + C++/CMake tools,
  * ~150 MiB Python,
  * ~400 MiB dotnet-sdk,
  * ~400 MiB java-jdk.

# Usage
To get the help simply type:
```sh
make
```

note: you can also use from top directory
```sh
make --directory=cmake
```

## Example
For example to test `Python` inside an `Alpine` container:
```sh
make alpine_python_test
```

# Docker
Dockerfile is splitted in several stages.

![docker](docker.svg)

# Vagrant
Vagrant is a tool for building and managing virtual machine environments in a single workflow.  
It is curently used to test FreeBSD inside a VirtualBox since we don't have any
FreeBSD machine.

Vagrant call an image/container a *box*.

note: Currently only github MacOS runner provide virtualization support (i.e. [VirtualBox](https://www.virtualbox.org/)).

## Basic usage
Once `vagrant` and `VirtualBox` are installed, all commands must be run where the `Vagrantfile` is
located.

Build and run a new *box*:
```sh
vagrant up
```
note: If you run `virtualbox` you should see it.

Connect to a vagrant *box*:
```sh
vagrant ssh
[vagrant@freebsd12 ~]$ ...
```

Execute few commands:
```sh
vagrant ssh -c "pwd; ls project ..."
```

Stop and delete a *box*:
```sh
vagrant destroy -f
```
