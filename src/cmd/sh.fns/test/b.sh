echo b.sh: $# $0 $1 $2 $3 $4
f1(){
echo f1: $# $0 $1 $2 $3 $4
}
f2(){
echo f2: $# $0 $1 $2 $3 $4
}
f1 x y z
echo b.sh: $# $0 $1 $2 $3 $4
f2 1 2 3 4 5
echo b.sh: $# $0 $1 $2 $3 $4
