#!/bin/bash
# Change regular C++ comments to make them Doxygen-friendly.
# This script is run as a preprocessor to Doxygen since most Google code
# does not specifically use Doxygen style comments.
# The marked up, HTML-ized copies of the code created by Doxygen should
# continue to use the original comment style based on our current config
# files.

runsed='sed -r'
unamestr=`uname`
if [[ "$unamestr" == 'Darwin' ]]; then
   runsed='sed -E'
fi

echo "/** \addtogroup $2"
echo " *  @{"
echo " */"

$runsed 's/\/\*([^\*])/\/\*\*\1/;                  # /* -> /**  \
       s/\/\/\/*/\/\/\//;                 # // -> ///  \
       s/;[ 	]*\/\*\*([^<*]*)/; \/\*\*<\1/; # /** -> /**< on right after code  \
       s/;[ 	]*\/\/\/*([^<])/; \/\/\/<\1/; # /// -> ///< on right after code \
       s/,([ 	]*)\/\/\/*([^<])/,\1\/\/\/<\2/; # /// -> ///< on right after enum \
       s/([[:alnum:]][ 	]*)\/\/\/*([^<])/\1\/\/\/<\2/; # /// -> ///< on right after enum \
       s/DISALLOW_EVIL_CONSTRUCTORS\(.*\)\;//; # /// -> ///< on right after code \
       s/(\/\/\/) *---+([^-].+[^-]) *---+/\1\2/; # /// ---- Bla ---- -> /// Bla
       s/(\/\/\/) *===+([^=].+[^=]) *===+/\1\2/; # /// ==== Bla ==== -> /// Bla
       s/(\/\/\/) *\*\*\*+([^\*].+[^\*]) *\*\*\*+/\1\2/; # /// **** Bla **** -> /// Bla
       s/(\/\/\/) *----*( *)/\1\2/; # /// -------- -> ///
       s/(\/\/\/) *====*( *)/\1\2/; # /// ======== -> ///
       s/(\/\/\/) *\*\*\*\**( *)/\1\2/; # /// ******** -> ///
       s/(\/\/\/) *\* \* \*( \*)* */\1/; # /// * * * * * -> ///
       s/([^A-Z_]TODO|FIXME|BUG)\(([a-z0-9]+)\)/\1(<a href="http:\/\/who\/\2">\2<\/a>)/; # TODO(user) -> TODO(<a href="http://who/user">user</a>) \
       s/(([^A-Z_])((TODO|FIXME)[^A-Z_].*))/\2 @todo \3/; # TODO* -> @todo TODO* \
       s/(([^A-Z_])((BUG)[^A-Z_].*))/\2 @bug \3/; # BUG* -> @bug BUG* \
       s/([ \t]*)ABSTRACT([ \t]*\;)/\1\=0\2/; # void f() ABSTRACT; -> void f() =0; \
       s/DECLARE_string(.*)/DECLARE_STRING\1/; # /// -> ///< on right after code \
       s/DECLARE_bool(.*)/DECLARE_BOOL\1/; # /// -> ///< on right after code \
       s/DECLARE_int32(.*)/DECLARE_INT32\1/; # /// -> ///< on right after code \
       s/DECLARE_uint32(.*)/DECLARE_UINT32\1/; # /// -> ///< on right after code \
       s/DECLARE_int64(.*)/DECLARE_INT64\1/; # /// -> ///< on right after code \
       s/DECLARE_uint64(.*)/DECLARE_UINT64\1/; # /// -> ///< on right after code \
       s/\/\/\/ *(Copyright(.*))/\/\/ \1/; # clutter \
       s/\/\/\/ *(All [rR]ights [rR]eserved(.*))/\/\/ \1/; # clutter \
       s/\/\/\/ *(Date: (.*))/\/\/\/ @file/; # clutter \
       s/\/\/\/ *Author:(.*)/\/\/\/ @file/; # /// Author -> /// @file \
       s/\/\/\/ *Author(.*)/\/\/\/ @file/; # /// Author -> /// @file  \
       s/namespace operations_research \{/namespace operations_research \{\n\/** \\ingroup '"$2"' *\//;' \
  $1

# There are no quotes around $1 to let the script work with standard input if
# $1 is not set.

echo "/** @}*/"
