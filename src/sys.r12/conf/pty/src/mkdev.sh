# pty/mkdev - install pseudoterminals

# Get current setting of a tunable parameter into a shell variable, or die

get_tunable () {
	set "$2" $($CONF_DIR/bin/idtune -p $1)

	is_empty $3 && {
		echo "pty/mkdev: can't find number of pty's configured"
		exit 100
	}

	eval $1=$3
}

# Verify that argument is between 0 and 128, inclusive.

require_0_to_128 () {
	if [ $1 -ge 0 -a $1 -le 128 ]
	then
		return 0
	else
		echo "Enter a number from 0 to 128"
		return 1
	fi
}

. /usr/lib/shell_lib.sh
COMMAND_NAME=$0
source_path $0 "HOME_DIR="
parent_of $HOME_DIR "CONF_DIR="

# Default number of ptys is whatever is already configured.

case $($CONF_DIR/bin/idenable -p pty) in
pty*Y)	
	get_tunable NUPTY_SPEC NUM_PTY
	;;

pty*N)	NUM_PTY=0
	;;

*)	echo "pty/mkdev: can't tell if pty device is enabled"
	exit 1
	;;
esac


while : ; do
	echo
	echo "To use the X windowing system or the screens package"
	echo "you must have the pty driver installed."
	echo
	echo "The more pty devices you have, the more windows"
	echo "you can open, but each pty device uses memory."
	echo
	echo "You may specify from 0 to 128 pty devices."
	echo "Your system is currently configured for $NUM_PTY ptys."
	echo

	read_input "How many pty devices would you like" NUM_PTY $NUM_PTY \
		require_0_to_128 || continue

	echo

	if [ $NUM_PTY -eq 0 ]
	then
		echo "Pty driver will be disabled - no ptys specified."
	else
		echo "Pty driver will be enabled with $NUM_PTY ptys."
	fi

	read_input "Is this correct" ANS "y" require_yes_or_no || continue
	is_yes $ANS && break
done


if [ $NUM_PTY -eq 0 ]
then
# Disable the device
	$CONF_DIR/bin/idenable -d pty
else
# Enable and tune the device
	$CONF_DIR/bin/idenable -e pty
	$CONF_DIR/bin/idtune -f NUPTY_SPEC $NUM_PTY

# Make device nodes.

	PTY_NAME_END=$(($NUM_PTY - 1))

	for i in `from 0 to $PTY_NAME_END`
	do
		/etc/mknod -f /dev/ptyp$i c 9 $(($i + 128))
		/etc/mknod -f /dev/ttyp$i c 9 $i
		/bin/chmog 666 bin bin /dev/ttyp$i
		/bin/chmog 666 bin bin /dev/ptyp$i
	done
fi

exit 0
