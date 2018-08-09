fn1 ()
{
	echo fn line args count $# are $0 $1 $2 $3 $4 $5 $6
	echo line 2
	rm foo
	echo redirected >foo
	cat foo | od -c
}
fn1 a b c
echo $?
fn2 () { echo fn2 on one line ; }
fn2
fn3 () { foo=baz; }
echo Before fn3: $foo
fn3
echo After fn3: $foo
fn4 () { echo $#; }
fn4
fn4 1 2 3
fn5 () {
	if true ; then echo true ; else echo false ; fi
	if false ; then echo not true ; else echo not false ; fi
}
fn5
fn7 () {
	for i in a b c d
	do
		echo $i:
	done
}
fn7
fn7 () {
	n=1
	while [ $n -lt 5 ]
	do
		echo n = $n
		n=`expr $n + 1`
	done
}
fn7
fn7
exit
