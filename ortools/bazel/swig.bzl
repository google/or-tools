# Bazel rules for building swig files.
def _py_wrap_cc_impl(ctx):
  srcs = ctx.files.srcs
  if len(srcs) != 1:
    fail("Exactly one SWIG source file label must be specified.", "srcs")
  module_name = ctx.attr.module_name
  src = ctx.files.srcs[0]
  inputs = set([src])
  inputs += ctx.files.swig_includes
  for dep in ctx.attr.deps:
    inputs += dep.cc.transitive_headers
  inputs += ctx.files._swiglib
  inputs += ctx.files.toolchain_deps
  swig_include_dirs = set(_get_repository_roots(ctx, inputs))
  swig_include_dirs += sorted([f.dirname for f in ctx.files._swiglib])
  args = [
      "-c++", "-python", "-module", module_name, "-o", ctx.outputs.cc_out.path,
      "-outdir", ctx.outputs.py_out.dirname
  ]
  args += ["-l" + f.path for f in ctx.files.swig_includes]
  args += ["-I" + i for i in swig_include_dirs]
  args += [src.path]
  outputs = [ctx.outputs.cc_out, ctx.outputs.py_out]
  ctx.action(
      executable=ctx.executable._swig,
      arguments=args,
      inputs=list(inputs),
      outputs=outputs,
      mnemonic="PythonSwig",
      progress_message="SWIGing " + src.path)
  return struct(files=set(outputs))


_py_wrap_cc = rule(
    attrs={
        "srcs":
            attr.label_list(
                mandatory=True,
                allow_files=True,),
        "swig_includes":
            attr.label_list(
                cfg="data",
                allow_files=True,),
        "deps":
            attr.label_list(
                allow_files=True,
                providers=["cc"],),
        "toolchain_deps":
            attr.label_list(
                allow_files=True,),
        "module_name":
            attr.string(mandatory=True),
        "py_module_name":
            attr.string(mandatory=True),
        "_swig":
            attr.label(
                default=Label("@swig//:swig"),
                executable=True,
                cfg="host",),
        "_swiglib":
            attr.label(
                default=Label("@swig//:templates"),
                allow_files=True,),
    },
    outputs={
        "cc_out": "%{module_name}.cc",
        "py_out": "%{py_module_name}.py",
    },
    implementation=_py_wrap_cc_impl,)
