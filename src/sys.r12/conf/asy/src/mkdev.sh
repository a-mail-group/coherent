# asy/mkdev - install serial ports.
#
# This script only sets up com1-com4.
#
# Manual editing of /etc/default/async, and programs asypatch and asymkdev,
# are needed to configure other 8250-family multiport cards.

. /usr/lib/shell_lib.sh
COMMAND_NAME=$0
source_path $0 "HOME_DIR="
parent_of $HOME_DIR "CONF_DIR="

# For read_input, require the number of a valid port.

require_com_num () {

	case $1 in
	[1-4])
		eval is_equal \$COM$1 $NO_COM && {
			echo "Please enter a valid port number, 1-4."
			return 1
		}
		return 0
		;;
	*)
		echo "Please enter a valid port number, 1-4."
		return 1
		;;
	esac
}

# A constant - used by /etc/conf/bin/bports to indicate no device.

NO_COM=NONE

echo "
Configuring serial ports com1-com4.

See \"asy\" in the COHERENT Lexicon for configuring serial ports beyond
com1-com4.

If your computer system uses both ports COM1 and COM3, one of these must
be run in polled mode rather than interrupt-driven.  Similarly, if your
computer system uses both ports COM2 and COM4, one must be run in polled
mode rather than interrupt-driven."

# Get async port base addresses as seen by the BIOS.

set $($CONF_DIR/bin/bports -c)
COM1=$1
COM2=$2
COM3=$3
COM4=$4

while : ; do
	LPDEV=
	MODEM_PORT=
	GOT_COM_PORT=n

	echo

	# Detect if there are any valid serial ports.

	if [ $COM1 = $NO_COM -a $COM2 = $NO_COM -a \
	     $COM3 = $NO_COM -a $COM4 = $NO_COM ]; then

		echo "No serial ports were detected by the BIOS."
		USE_COM=n
	else
		# Ask if serial ports should be enabled.

		echo "Your BIOS identifies the following serial ports:"
echo "COM1: $COM1	COM2: $COM2	COM3: $COM3	COM4: $COM4\n"
		USE_COM=y
		GOT_COM_PORT=y
	fi
	read_input "Enable serial port device driver" USE_COM $USE_COM \
		require_yes_or_no

	is_yes $GOT_COM_PORT && {

		###############
		# Ask about serial printer.
		###############

		# Ask which, if any, should be the default.

		echo "
The default printer on COHERENT is /dev/lp.
This device may be linked to any valid serial or parallel port.
"
		read_input "Link /dev/lp to one of COM1 through COM4" \
		  ANS "n" require_yes_or_no

		is_yes $ANS && {

			read_input "Enter 1 to 4 for COM1 through COM4 for /dev/lp" \
			  com_num "" require_com_num

			read_input "Do you want to run COM${com_num} in polled mode" \
			  yesno "n" require_yes_or_no

			LPDEV="/dev/com${com_num}"
			if is_yes $yesno; then
				LPDEV="${LPDEV}pl"
			else
				LPDEV="${LPDEV}l"
			fi
		}

		if is_empty $LPDEV; then
			echo "\n/dev/lp will not be linked to any serial port."
		else
			echo "\n/dev/lp will be linked to $LPDEV"
		fi

		###############
		# Ask about modem.
		###############

		echo "
The default modem on COHERENT is /dev/modem.
This device may be linked to any valid serial port.
"
		read_input "Link /dev/modem to one of COM1 through COM4" \
		  ANS "y" require_yes_or_no

		is_yes $ANS && {

			read_input "Enter 1 to 4 for COM1 through COM4 for /dev/modem" \
			  com_num "" require_com_num

			read_input "Do you want to run COM${com_num} in polled mode" \
			  yesno "n" require_yes_or_no

			MODEM_PORT="/dev/com${com_num}"
			if is_yes $yesno; then
				MODEM_PORT="${MODEM_PORT}pl"
			else
				MODEM_PORT="${MODEM_PORT}l"
			fi
		}

		if is_empty $MODEM_PORT; then
			echo "\n/dev/modem will not be linked to any serial port."
		else
			echo "\n/dev/modem will be linked to $MODEM_PORT"
		fi
	}

	if is_yes $USE_COM ; then
		echo "Serial ports will be enabled."
	else
		echo "Serial ports will be disabled."
	fi

	read_input "Is this correct" ANS "y" require_yes_or_no || continue
	is_yes $ANS && break
done


# Enable/disable serial port driver.

if is_yes $USE_COM ; then
	$CONF_DIR/bin/idenable -e asy
else
	COM1=$NO_COM
	COM2=$NO_COM
	COM3=$NO_COM
	COM4=$NO_COM
	$CONF_DIR/bin/idenable -d asy
fi


# Make device nodes.

for i in 1 2 3 4; do
	eval is_equal \$COM$i $NO_COM || {
		/etc/mknod -f /dev/com${i}l c 5 $((128 + $i - 1))
		/etc/mknod -f /dev/com${i}r c 5 $(($i - 1))
		/bin/chmog 666 sys sys /dev/com${i}[lr]
	}
done


# Link default printer device.

is_empty $LPDEV || {
	/bin/echo "/bin/ln -f $LPDEV /dev/lp"
	/bin/ln -f $LPDEV /dev/lp
}

# Link default modem device.

is_empty $MODEM_PORT || {
	/bin/echo "/bin/ln -f $MODEM_PORT /dev/modem"
	/bin/ln -f $MODEM_PORT /dev/modem
}
