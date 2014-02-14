#!/bin/sh

doxygen='doxygen'
unamestr=`uname`
if [[ "$unamestr" == 'Darwin' ]]; then
   doxygen='/Applications/Doxygen.app/Contents/Resources/doxygen'
fi

echo Cleaning old temporary files.
rm -rf src
rm -rf gen
rm -rf tags

echo Extracting source
mkdir src
mkdir gen
mkdir tags
for subdir in algorithms base constraint_solver graph linear_solver util
do
  mkdir src/$subdir
  cp ../../../src/$subdir/README src/$subdir
  for file in ../../../src/$subdir/*.{h,cc}
  do
    ./prepare_for_doxygen.sh $file $subdir > src/$subdir/`basename $file`
  done
done

echo Running doxygen
$doxygen ./doxy.cfg
#doxygen ./base.cfg
#doxygen ./util.cfg
#doxygen ./algorithms.cfg
#doxygen ./graph.cfg
#doxygen ./linear_solver.cfg
#doxygen ./constraint_solver.cfg
