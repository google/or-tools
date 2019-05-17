// This file contains SWIG bindings to wrap C++ functions with
// protocol buffer arguments, for use in Go.
//
// The main goal is to decouple C++ and Go protocol buffer usage,
// allowing Go code to continue to use Go protocol buffers
// instead of SWIG wrapped protocol buffers.
//
// It achieves this goal by serializing/deserializing the protocol
// buffers when communicating with C++ code.
// Serialization may impose a slight performance penalty; however,
// this is always faster than RPC as no transmission occurs.
//
// The following macros are supported.
// CMsgType refers to a C++ protocol buffer, fully qualified with namespace,
// GoMsgPkg refers to a BUILD target of the Go protocol buffer,
// GoMsgType refers to a Go protocol buffer type: packageName.TypeName.
//
// PROTO_INPUT(CMsgType, GoMsgPkg, GoMsgType)
// PROTO_OUTPUT(CMsgType, GoMsgPkg, GoMsgType)
// PROTO_IN_OUT(CMsgType, GoMsgPkg, GoMsgType)
// PROTO_RETURN(CMsgType, GoMsgPkg, GoMsgType)
// PROTO_ENUM(CMsgType, GoMsgPkg, GoMsgType)
// PROTO_CALLBACK_ARGUMENT(CMsgType, GoMsgPkg, GoMsgType)
//
// NOTES:
// - Only one of PROTO_INPUT, PROTO_OUTPUT, PROTO_IN_OUT may be used for a
//   given proto type. You may combine any one of those with PROTO_RETURN.
//   PROTO_INPUT can be combined with PROTO_OUTPUT/PROTO_IN_OUT as long as the
//   input parameter is passed by value or by a const pointer (since strictly
//   speaking the parameters are not of the same type in this case).
// - For passing protobuf into callback, use PROTO_CALLBACK_ARGUMENT.
//   PROTO_INPUT is implicitly enabled when PROTO_CALLBACK_ARGUMENT is used.
// - For getting/setting fields of a struct, use PROTO_RETURN.
// - For all of the above, the Go interface will accept/return a pointer to
//   GoMsgType.
// - The go_wrap_cc target that uses this file will have to have the following
//   dependency: "//net/proto2/go:proto".
//
// Example:
//   .h file:
//     const ns::MyProto1& SomeFunc(const ns::MyProto2& myInput,
//                                  ns::MyProto3* mutate,
//                                  ns::MyProto4* myOutput);
//   .swig file:
//     PROTO_RETURN(ns::MyProto1,
//                  pb "google3/path/to/my/proto/target_go_proto",
//                  pb.MyProto1)
//     PROTO_INPUT(ns::MyProto2,
//                 pb "google3/path/to/my/proto/target_go_proto",
//                 pb.MyProto2)
//     PROTO_IN_OUT(ns::MyProto3,
//                  pb "google3/path/to/my/proto/target_go_proto",
//                  pb.MyProto3)
//     PROTO_OUTPUT(ns::MyProto4,
//                  pb "google3/path/to/my/proto/target_go_proto",
//                  pb.MyProto4)
//     PROTO_ENUM(ns::MyProto5_MyEnum,
//                pb "google3/path/to/my/proto/target_go_proto",
//                pb.MyProto5_MyEnum)
//   Generated .go file:
//     func SomeFunc(myInput *pb.MyProto2,
//                   mutate *pb.MyProto3,
//                   myOutput *pb.MyProto4) *pb.MyProto1
//     (and a pb.MyProto5_MyEnum can be passed in/out or returned)


%fragment("ProtofuncToGoString", "header", fragment="AllocateString") %{
namespace {

_gostring_ ProtofuncToGoString(proto2::Message* pb) {
  if (pb == nullptr) {
    return Swig_AllocateString(nullptr, 0);
  }
  string proto_string = " " + pb->SerializePartialAsString();
  return Swig_AllocateString(proto_string.data(), proto_string.length());
}

_gostring_ ProtofuncToGoString(const proto2::Message& pb) {
  string proto_string = " " + pb.SerializePartialAsString();
  return Swig_AllocateString(proto_string.data(), proto_string.length());
}

}  // namespace
%}


%define PROTO_RETURN(CMsgType, GoMsgPkg, GoMsgType)

%go_import(GoMsgPkg, "google3/net/proto2/go/proto")

%typemap(gotype) PROTO_RETURN_TYPE_ALL "*"#GoMsgType
%typemap(imtype) PROTO_RETURN_TYPE* "*string"
%typemap(imtype) PROTO_RETURN_TYPE_NON_PTR "string"

%typemap(out, fragment="ProtofuncToGoString") PROTO_RETURN_TYPE* {
  $result = (_gostring_*) malloc(sizeof(_gostring_));
  _gostring_ tmp = ProtofuncToGoString($1);
  memcpy($result, &tmp, sizeof(_gostring_));
}

%typemap(out, fragment="ProtofuncToGoString") PROTO_RETURN_TYPE_NON_PTR {
  $result = ProtofuncToGoString($1);
}

// Support %newobject to dispose of original return value.
%typemap(newfree) PROTO_RETURN_TYPE* {
  delete $1;
}

%typemap(goout, fragment="CopyString") PROTO_RETURN_TYPE* %{
  {
    defer Swig_free((uintptr)(unsafe.Pointer($1)))
    s := swigCopyString(*$1)
    if s != "" {
      $result = new(GoMsgType)
      if err := proto.Unmarshal([]byte(s[1:]), $result); err != nil {
        // We accept partial messages here.
        if _, ok := err.(*proto.RequiredNotSetError); !ok {
          panic(err)
        }
      }
    }
  }
%}

%typemap(goout, fragment="CopyString") PROTO_RETURN_TYPE_NON_PTR %{
  {
    s := swigCopyString($1)
    if s != "" {
      $result = new(GoMsgType)
      if err := proto.Unmarshal([]byte(s[1:]), $result); err != nil {
        // We accept partial messages here.
        if _, ok := err.(*proto.RequiredNotSetError); !ok {
          panic(err)
        }
      }
    }
  }
%}

%typemap(goin) PROTO_RETURN_TYPE %{
  {
    var err error
    var b []byte
    if b, err = proto.Marshal($input); err != nil {
      // We accept partial messages here.
      if _, ok := err.(*proto.RequiredNotSetError); !ok {
        panic(err)
      }
    }
    $result = string(b)
  }
%}

// TODO(user): Support setting of pointer struct fields.
//     This is somewhat complicated, since we use the relevant %typemap(in) for
//     in PROTO_OUTPUT, and having it here breaks usage of both
//     PROTO_{RETURN,OUTPUT} together.

%typemap(in) PROTO_RETURN_TYPE %{
  $1.ParsePartialFromArray($input.p, $input.n);
%}

%apply PROTO_RETURN_TYPE { CMsgType }
%apply PROTO_RETURN_TYPE* { const CMsgType*, CMsgType* }
%apply PROTO_RETURN_TYPE_NON_PTR { CMsgType, const CMsgType&, CMsgType& }
%apply PROTO_RETURN_TYPE_ALL {
    CMsgType, const CMsgType*, CMsgType*, const CMsgType&, CMsgType& }

%enddef // end PROTO_RETURN


%define PROTO_INPUT(CMsgType, GoMsgPkg, GoMsgType)

%go_import(GoMsgPkg, "google3/net/proto2/go/proto")

%typemap(gotype) PROTO_INPUT_TYPE_ALL "*"#GoMsgType
%typemap(imtype) PROTO_INPUT_TYPE_ALL "string"

%typemap(goin) PROTO_INPUT_TYPE_ALL %{
  {
    var b []byte
    var err error
    if b, err = proto.Marshal($input); err != nil {
      // We accept partial messages here.
      if _, ok := err.(*proto.RequiredNotSetError); !ok {
        panic(err)
      }
    }
    $result = string(b)
  }
%}

%typemap(in) PROTO_INPUT_TYPE %{
  $1.ParsePartialFromArray((char *)$input.p, $input.n);
%}

%typemap(in) PROTO_INPUT_TYPE_PTR (CMsgType t) %{
  t.ParsePartialFromArray((char *)$input.p, $input.n);
  $1 = &t;
%}

// explicitly set to empty so const pointers don't get the typemap for normal
// pointers that may be defined in PROTO_OUTPUT/IN_OUT
%typemap(argout) PROTO_INPUT_TYPE_CONST_PTR ""
%typemap(goargout) PROTO_INPUT_TYPE_CONST_PTR ""

%apply PROTO_INPUT_TYPE { CMsgType }
%apply PROTO_INPUT_TYPE_PTR {
    const CMsgType*, CMsgType*, const CMsgType&, CMsgType& }
%apply PROTO_INPUT_TYPE_CONST_PTR {
    const CMsgType*, const CMsgType& }
%apply PROTO_INPUT_TYPE_ALL {
    CMsgType, const CMsgType*, CMsgType*, const CMsgType&, CMsgType& }

%enddef // end PROTO_INPUT


%define PROTO_OUTPUT(CMsgType, GoMsgPkg, GoMsgType)

%go_import(GoMsgPkg, "google3/net/proto2/go/proto")

%typemap(gotype) PROTO_OUTPUT_TYPE* "*"#GoMsgType
%typemap(imtype) PROTO_OUTPUT_TYPE* "*string"

%typemap(in) PROTO_OUTPUT_TYPE* (CMsgType t) {
  $1 = &t;
}

%typemap(argout, fragment="ProtofuncToGoString") PROTO_OUTPUT_TYPE* {
  *$input = ProtofuncToGoString($1);
}

%typemap(goargout, fragment="CopyString") PROTO_OUTPUT_TYPE* %{
  {
    s := swigCopyString(*$input)
    if s != "" {
      if err := proto.Unmarshal([]byte(s[1:]), $1); err != nil {
        // We accept partial messages here.
        if _, ok := err.(*proto.RequiredNotSetError); !ok {
          panic(err)
        }
      }
    }
  }
%}

%typemap(goin) PROTO_OUTPUT_TYPE* %{
  $result = new(string)
%}

%apply PROTO_OUTPUT_TYPE* { CMsgType* }

%enddef // end PROTO_OUTPUT


%define PROTO_IN_OUT(CMsgType, GoMsgPkg, GoMsgType)

%go_import(GoMsgPkg, "google3/net/proto2/go/proto")

%typemap(gotype) PROTO_IN_OUT_TYPE* "*"#GoMsgType
%typemap(imtype) PROTO_IN_OUT_TYPE* "*string"

// We can't pass the value down as a pointer to a Go string, since
// that would require a pointer to a pointer, which is forbidden by
// the cgo pointer passing rules.  Instead we construct a byte array
// with an 8 byte length followed by the actual data, and type convert
// that to a string pointer.
%typemap(goin) PROTO_IN_OUT_TYPE* %{
  {
    var b []byte
    var err error
    if b, err = proto.Marshal($input); err != nil {
      // We accept partial messages here.
      if _, ok := err.(*proto.RequiredNotSetError); !ok {
        panic(err)
      }
    }
    v := len(b)

    // We need to allocate enough space here for the argout typemap
    // to store a string value.
    c := uintptr(v + 8)
    if c < unsafe.Sizeof("") {
      c = unsafe.Sizeof("")
    }
    b1 := make([]byte, 8, c)
    for i := 0; i < 8; i++ {
      b1[i] = byte(v & 0xff)
      v >>= 8
    }
    b1 = append(b1, b...)
    $result = (*string)(unsafe.Pointer(&b1[0]))
  }
%}

%typemap(in) PROTO_IN_OUT_TYPE* (CMsgType t) %{
  {
    size_t v = 0;
    const char* s = (const char*)$input;
    for (size_t i = 0; i < 8; i++) {
      if (i < sizeof(v))
        v |= (size_t)*s++ << (i*8);
    }
    t.ParsePartialFromArray((const char*)$input + 8, v);
    $1 = &t;
  }
%}

%typemap(argout, fragment="ProtofuncToGoString") PROTO_IN_OUT_TYPE* {
  *$input = ProtofuncToGoString($1);
}

%typemap(goargout, fragment="CopyString") PROTO_IN_OUT_TYPE* %{
  {
    s := swigCopyString(*$input)
    if s != "" {
      if err := proto.Unmarshal([]byte(s[1:]), $1); err != nil {
        // We accept partial messages here.
        if _, ok := err.(*proto.RequiredNotSetError); !ok {
          panic(err)
        }
      }
    }
  }
%}

%apply PROTO_IN_OUT_TYPE* { CMsgType* }

%enddef // end PROTO_IN_OUT


%define PROTO_ENUM(CMsgType, GoMsgPkg, GoMsgType)

%go_import(GoMsgPkg)

// By value.
%typemap(gotype) PROTO_ENUM_TYPE "GoMsgType"
%typemap(imtype) PROTO_ENUM_TYPE "int"

%typemap(goin) PROTO_ENUM_TYPE %{
  $result = int($input)
%}

%typemap(in) PROTO_ENUM_TYPE %{
  $1 = static_cast<CMsgType>($input);
%}

%typemap(out) PROTO_ENUM_TYPE {
  $result = static_cast<intgo>($1);
}

%typemap(goout) PROTO_ENUM_TYPE {
  $result = GoMsgType($1)
}

%apply PROTO_ENUM_TYPE { CMsgType }

// By pointer.
%typemap(gotype) PROTO_ENUM_TYPE* "*"#GoMsgType
%typemap(imtype) PROTO_ENUM_TYPE* "*int"

%typemap(goin) PROTO_ENUM_TYPE* %{
  if $input != nil {
    res := int(*$input)
    $result = &res
  }
%}

%typemap(in) PROTO_ENUM_TYPE* {
  $1 = reinterpret_cast<CMsgType*>($input);
}

%typemap(out) PROTO_ENUM_TYPE* {
  $result = reinterpret_cast<intgo*>($1);
}

%typemap(goout) PROTO_ENUM_TYPE* {
  if $1 != nil {
    res := GoMsgType(*$1)
    $result = &res
  }
}

%typemap(goargout) PROTO_ENUM_TYPE* %{
  if $input != nil {
    *$1 = GoMsgType(*$input)
  }
%}

%apply PROTO_ENUM_TYPE* { CMsgType*, const CMsgType* }

%enddef // end PROTO_ENUM_INPUT

%define PROTO_CALLBACK_ARGUMENT(CMsgType, GoMsgPkg, GoMsgType)

// imtype (string) => gotype (GoMsgType*)
%typemap(godirectorin,fragment="CopyString") PROTO_INPUT_TYPE_ALL %{
  {
    s := swigCopyString($input)
    if s != "" {
      $result = new(GoMsgType)
      if err := proto.Unmarshal([]byte(s[1:]), $result); err != nil {
        // We accept partial messages here.
        if _, ok := err.(*proto.RequiredNotSetError); !ok {
          panic(err)
        }
      }
    }
  }
%}

// CMsgType => imtype (string)
%typemap(directorin, fragment="ProtofuncToGoString") PROTO_INPUT_TYPE_ALL {
  $input = ProtofuncToGoString($1);
}

PROTO_INPUT(CMsgType, GoMsgPkg, GoMsgType)

%enddef  // end PROTO_CALLBACK_ARGUMENT
