#!/bin/bash
# Opens a set of files in emacs and executes the emacs-format-function.
# Assumes the function named emacs-format-function is defined in the
# file named emacs-format-file.
# From http://www.cslab.pepperdine.edu/warford/BatchIndentationEmacs.html

set -e

WHERE=$(readlink -f $(dirname $0))
#echo "$WHERE"/emacs-format-file.el

function die() {
    echo "$*" 1>&2
    exit 1
}

if [ $# == 0 ]
then
   echo "Usage: $0 files-to-indent" 1>&2
   die "$0 requires at least one argument."
fi
while [ $# -ge 1 ]
do
   if [ -d $1 ]
   then
      die "Argument of $0 $1 cannot be a directory."
   fi
   test -e $1 || die "$0: $1 not found."
   # batch mode is verbose on stderr so we silence it, remove redirect to diagnose issues
   emacs --batch $1 -l "$WHERE"/emacs-format-file.el -f emacs-format-function 1>/dev/null 2>&1 || die "Emacs failed"
   shift 1
done
exit 0
