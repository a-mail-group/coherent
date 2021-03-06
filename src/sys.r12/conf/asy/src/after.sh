# Script to perform post-processing after a kernel link with the 'asy' driver
# enabled. This script expects a single argument which is the name of the
# target kernel that was built.

if [ $# -eq 1 ]; then
	/conf/asypatch -v $1 </etc/default/async
else
	echo "$CWD/$0" ":" Bad arguments to postprocessing script
fi

