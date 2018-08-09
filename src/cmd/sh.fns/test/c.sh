n=15
f(){
	status 10
	if false
	then
		return
	fi
	status 5
}
f
echo $?
f(){
	status 10
	return $n
	status 5
}
f
echo $?
