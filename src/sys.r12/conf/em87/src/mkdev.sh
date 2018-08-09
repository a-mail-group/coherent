# em87/mkdev - install floating point emulator.

. /usr/lib/shell_lib.sh
COMMAND_NAME=$0
source_path $0 "HOME_DIR="
parent_of $HOME_DIR "CONF_DIR="

FPHW=$($CONF_DIR/bin/ndptype)

while : ; do
	echo
	EFLAG=

	case $FPHW in

	FP_287 | FP_387)
		echo "Floating point hardware is in use on this system."
		echo "Software emulation is not needed."
		read_input "Configure without emulation" ANS "y" \
			require_yes_or_no || continue

		is_yes $ANS || EFLAG=emulate
		;;

	FP_NO)	echo "System is not configured for floating point support.
Floating point support is needed by by Word Perfect, 123, and gcc."

		read_input "Enable software emulation of floating point" \
			ANS "y" require_yes_or_no || continue
		is_yes $ANS && EFLAG=emulate
		;;

	FP_SW)	echo "System is configured for floating point emulation.
Floating point support is needed by by Word Perfect, 123, and gcc."

		read_input "Keep emulation enabled" ANS "y" \
			require_yes_or_no || continue
		is_yes $ANS && EFLAG=emulate
		;;

	*)	echo "em87/mkdev: invalid result from ndptype" 1>&2
		exit 1
		;;
	esac

	echo

	if is_empty $EFLAG ; then
		echo "Floating point emulation will be DISABLED."
	else
		echo "Floating point emulation will be ENABLED."
	fi

	read_input "Is this correct" ANS "y" require_yes_or_no || continue
	is_yes $ANS && break
done

if is_empty $EFLAG ; then
	$CONF_DIR/bin/idenable -d em87
else
	$CONF_DIR/bin/idenable -e em87
fi

exit 0
