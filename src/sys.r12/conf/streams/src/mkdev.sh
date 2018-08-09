# streams/mkdev - install System V-style streams.

. /usr/lib/shell_lib.sh
COMMAND_NAME=$0
source_path $0 "HOME_DIR="
parent_of $HOME_DIR "CONF_DIR="

while : ; do
	echo

	read_input "Enable STREAMS" ANS "n" require_yes_or_no || continue

	if is_yes $ANS; then
		EFLAG="-e"
		echo "STREAMS will be ENABLED."
	else
		EFLAG="-d"
		echo "STREAMS will be DISABLED."
	fi

	read_input "Is this correct" ANS "y" require_yes_or_no || continue
	is_yes $ANS && break
done

$CONF_DIR/bin/idenable $EFLAG streams
exit 0
