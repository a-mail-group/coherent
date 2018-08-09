fn1 ()
{
	echo fn line args count $# are $0 $1 $2 $3 $4 $5 $6
	echo line 2
	rm foo
	echo redirected >foo
	cat foo | od -c
}
fn1 a b c
