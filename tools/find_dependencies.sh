for deps in $(grep -e "\#include \"$dir" "src/$1/*.h" "src/$1/*.cc" | cut -d '"' -f 2 | sort -u)
do
    echo "$deps"
done
