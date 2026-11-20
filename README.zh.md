# OR-Tools - Google 运筹学与优化工具套件

[English](README.md) | [简体中文](README.zh.md)

[![PyPI version](https://img.shields.io/pypi/v/ortools.svg)](https://pypi.org/project/ortools/)
[![PyPI download](https://img.shields.io/pypi/dm/ortools.svg)](https://pypi.org/project/ortools/#files)
\
[![NuGet version](https://img.shields.io/nuget/v/Google.OrTools.svg)](https://www.nuget.org/packages/Google.OrTools)
[![NuGet download](https://img.shields.io/nuget/dt/Google.OrTools.svg)](https://www.nuget.org/packages/Google.OrTools)
\
[![Maven Central](https://img.shields.io/maven-central/v/com.google.ortools/ortools-java)](https://mvnrepository.com/artifact/com.google.ortools/ortools-java)
\
[![Discord](https://img.shields.io/discord/693088862481678374?color=7289DA&logo=discord&style=plastic)](https://discord.gg/ENkQrdf)

Google 针对组合优化难题打造的高性能开源软件套件。

## 目录

*   [关于 OR-Tools](#关于-or-tools)
*   [代码结构映射 (Codemap)](#代码结构映射-codemap)
*   [安装指南](#安装指南)
*   [快速上手](#快速上手)
*   [官方文档](#官方文档)
*   [参与贡献](#参与贡献)
*   [开源许可证](#开源许可证)

<a name="about"></a>
## 关于 OR-Tools

Google 优化工具套件（Google Optimization Tools，简称 OR-Tools）是一款快速、轻便且开源的软件套件，专为求解组合优化难题而设计。

该套件涵盖：

*   两款约束规划求解器（CP* 与 CP-SAT）；
*   两款线性规划求解器（Glop 与 PDLP）；
*   封装了商用及其他开源求解器（包括各类混合整数规划 MIP 求解器）的统一接口；
*   装箱问题（Bin packing）与背包问题（Knapsack）算法；
*   旅行商问题（TSP）与车辆路径规划问题（VRP）算法；
*   图算法（最短路径、最小费用流、最大流、线性求和分配）。

OR-Tools 底层基于 C++ 构建，同时提供了 Python、C# 和 Java 的官方封装绑定。

## 代码结构映射 (Codemap)

本软件套件由以下组件构成：

*   [Makefile](Makefile)：基于 [GNU Make](https://www.gnu.org/software/make/manual/make.html) 构建的顶层入口。
*   [makefiles](makefiles)：辅助 Make 文件、CI 流水线与构建系统文档。
*   [CMakeLists.txt](CMakeLists.txt)：基于 [CMake](https://cmake.org/cmake/help/latest/) 构建的顶层入口。
*   [cmake](cmake)：辅助 CMake 文件、CI 流水线与构建系统文档。
*   [WORKSPACE](WORKSPACE)：基于 [Bazel](https://bazel.build/start/bazel-intro) 构建的顶层入口。
*   [bazel](bazel)：辅助 Bazel 文件、CI 流水线与构建系统文档。
*   [ortools](ortools)：源代码根目录。
    *   [base](ortools/base)：基础实用工具库。
    *   [algorithms](ortools/algorithms)：基础优化算法。
        *   [samples](ortools/algorithms/samples)：精心编排的算法范例。
    *   [graph](ortools/graph)：图算法。
        *   [samples](ortools/graph/samples)：精心编排的图算法范例。
    *   [linear_solver](ortools/linear_solver)：线性规划求解器通用包装层。
        *   [samples](ortools/linear_solver/samples)：精心编排的线性规划范例。
    *   [glop](ortools/glop)：基于单纯形法的线性规划求解器。
        *   [samples](ortools/glop/samples)：精心编排的 Glop 范例。
    *   [pdlp](ortools/pdlp)：基于一阶算法的线性规划求解器。
        *   [samples](ortools/pdlp/samples)：精心编排的 PDLP 范例。
    *   [lp_data](ortools/lp_data)：线性规划模型核心数据结构。
    *   [constraint_solver](ortools/constraint_solver)：约束与路径规划求解器（Routing）。
        *   [docs](ortools/constraint_solver/docs)：该组件文档。
        *   [samples](ortools/constraint_solver/samples)：精心编排的约束求解范例。
    *   [sat](ortools/sat)：SAT 求解器（CP-SAT）。
        *   [docs](ortools/sat/docs)：该组件文档。
        *   [samples](ortools/sat/samples)：精心编排的 CP-SAT 范例。
    *   [bop](ortools/bop)：基于 SAT 的布尔优化求解器。
    *   [util](ortools/util)：约束求解器依赖的基础工具库。
*   [examples](examples)：全量示例项目根目录。
    *   [contrib](examples/contrib)：来自开源社区贡献的范例。
    *   [cpp](examples/cpp)：C++ 示例。
    *   [dotnet](examples/dotnet)：.NET 示例。
    *   [java](examples/java)：Java 示例。
    *   [python](examples/python)：Python 示例。
    *   [notebook](examples/notebook)：Jupyter / IPython Notebooks 交互式教程。
    *   [flatzinc](examples/flatzinc)：FlatZinc 示例。
*   [tools](tools)：发布与分发交付工具（例如 Windows GNU 二进制文件、脚本、发行版 Dockerfile 等）。

## 安装指南

本软件套件已在以下运行环境下经过严密测试：

*   Ubuntu 18.04 LTS 及更高版本（64 位）；
*   Apple macOS Mojave 及更高版本搭配 Xcode 9.x+（64 位）；
*   Microsoft Windows 搭配 Visual Studio 2022（64 位）。

OR-Tools 当前可通过 Makefile 进行构建，同时也提供了 Bazel 和 CMake 的完整构建支持。

有关完整安装指南（涵盖源码构建与预编译二进制包安装），请访问：
https://developers.google.com/optimization/introduction/installing

### 使用 Make 从源码构建（传统方式）

我们提供了基于 Make 的构建流程。<br>请参阅
[Make 构建说明](makefiles/README.md)。

### 使用 CMake 从源码构建

我们提供了基于 CMake 的构建流程。<br>请参阅
[CMake 构建说明](cmake/README.md)。

### 使用 Bazel 从源码构建

我们提供了基于 Bazel 的构建流程。<br>请参阅
[Bazel 构建说明](bazel/README.md)。

## 快速上手

快速学习并上手 OR-Tools 的最佳途径是跟随官方开发者指南中的动手教程：

https://developers.google.com/optimization/introduction/get_started

如果您希望通过阅读代码示例直接学习，请查阅 [examples](examples) 目录下的丰富示例集。

## 官方文档

OR-Tools 的全量官方文档与 API 参考请访问：
https://developers.google.com/optimization/

## 参与贡献

在提交任何 Pull Request（PR）之前，请参阅 [CONTRIBUTING.md](CONTRIBUTING.md) 了解如何签署贡献者许可协议（CLA）。
如果您刚开始接触本项目，建议在提交第一个 PR 前先在 Issue 中讨论提案并达成共识。

## 开源许可证

OR-Tools 软件套件基于 Apache License 2.0 开源许可证授权。
<br>详情请参阅 [LICENSE](LICENSE)。
