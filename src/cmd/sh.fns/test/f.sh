f() {
	echo f: $1 $2 $3
	g $1 $2 $3
}
f a b c
g() { echo g: $1 $2 $3 ; }
g x y z
f 1 2 3
