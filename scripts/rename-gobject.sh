#!/bin/bash

set -e

WHERE=$(readlink -f $(dirname $0))

OLD_ns=hwf
OLD_Ns=Hwf
OLD_NS=HWF

NEW_ns=hio
NEW_Ns=Hio
NEW_NS=HIO

OLD_type=server
OLD_Type=Server
OLD_TYPE=SERVER

NEW_type=$OLD_type
NEW_Type=$OLD_Type
NEW_TYPE=$OLD_TYPE

function rename_in() {
    FILE=$1

    cat "$FILE" |                                                                       \
        sed -e "s/$OLD_Ns$OLD_Type/$NEW_Ns$NEW_Type/g" |                                \
        sed -e "s/${OLD_ns}_$OLD_type/${NEW_ns}_$NEW_type/g" |                          \
        sed -e "s/${OLD_NS}_$OLD_TYPE/${NEW_NS}_$NEW_TYPE/g" |                          \
        sed -e "s/${OLD_NS}_TYPE_$OLD_TYPE/${NEW_NS}_TYPE_$NEW_TYPE/g" |                \
        sed -e "s/${OLD_NS}_IS_$OLD_TYPE/${NEW_NS}_IS_$NEW_TYPE/g" > "$FILE".renamed

    if ! diff 1>/dev/null "$FILE" "$FILE".renamed ; then
        "$WHERE"/reindent-file.sh "$FILE".renamed
        /bin/mv -f "$FILE".renamed "$FILE"
        echo "Made changes to $FILE"
    else
        /bin/rm -f "$FILE".renamed
        echo "$FILE is not modified"
    fi
}

if [ $# == 0 ]
then
   echo "$0 requires at least one argument." 1>&2
   echo "Usage: $0 files-to-rename-in" 1>&2
   exit 1
fi
while [ $# -ge 1 ]
do
   if [ -d $1 ]
   then
      echo "Argument of $0 $1 cannot be a directory." 1>&2
      exit 1
   fi
   rename_in $1
   shift 1
done
exit 0

