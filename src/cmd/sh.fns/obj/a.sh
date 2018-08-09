if test "$undefined"
then
	echo hello
fi
if test $? -ne 0
then
	echo whatever
fi
