# ft/mkdev - get floppy tape device info

DEVDIR=/dev

# Things to do:

#	Ask if the driver should be enabled.
#
#	Configure tape drive.
#	Get/set number of segment buffers.
#	Get/set soft select option.
#	If hard select, get/set unit number.


. /usr/lib/shell_lib.sh
COMMAND_NAME=$0
source_path $0 "HOME_DIR="
parent_of $HOME_DIR "CONF_DIR="

. $CONF_DIR/bin/conf_lib.sh

#################
# require_0_to_3
#
# Verify that argument is between 0 and 128, inclusive.
#################

require_0_to_3 () {
	if [ $1 -ge 0 -a $1 -le 3 ]
	then
		return 0
	else
		echo "Enter a number from 0 to 3"
		return 1
	fi
}


###############
# Main logic.
#
# Configure driver.
# If driver enabled
#   configure hard drive devices
###############

# Enable/disable ft device driver.
echo "\nQIC-80/QIC-40 Tape Drive Attached to Diskette Controller (Floppy Tape)"
set_enable_status ft

# Get current unit numbers for floppy tape device, if any
TAPE_DEV=/dev/ft
if [ -c $TAPE_DEV ];then
	ft_unit=$(expr $(set $(/bin/ls -l $TAPE_DEV); echo $6) % 4)
fi

echo "Current ft unit is ${ft_unit-NONE}"

# Hardware configuration of tape drive.

read_input "Configure floppy tape" ANS "n" require_yes_or_no
if is_yes $ANS;then

	while : ; do

		tune FT_NBUF_SPEC "Number of 32 Kbyte tape buffers"

		tune FT_AUTO_CONFIG_SPEC "
0 - manual configuration (if you know unit number or soft select mode)
1 - auto configuration of tape drive select

Your choice"

		get_tunable FT_AUTO_CONFIG_SPEC ft_auto_config

		if [ $ft_auto_config -eq 0 ];then

			tune FT_SOFT_SELECT_SPEC "
0 - hard select
1 - soft select, Archive/Conner/Mountain/Summit
2 - soft select, Colorado Memory Systems

Your choice"

			get_tunable FT_SOFT_SELECT_SPEC ft_soft_select

			# Unit number matters for hard select and CMS softsel.
			if [ $ft_soft_select -ne 1 ];then
				read_input \
"Which unit number (0..3) is the tape drive assigned to" \
				  ft_unit ${ft_unit-1} \
				  require_0_to_3
			else
				ft_unit=0
			fi

		else
			echo \
"Tape drive select method will be automatically determined."
			ft_unit=0
		fi

		read_input "Is this correct" ANS "y" require_yes_or_no || continue
		is_yes $ANS && break

	done
fi


# Make device nodes.
#
# Major number is 4.
# Minor number is
#   0110 1000 + unit (104) for rewind on close
#   0111 1000 + unit (120) for no rewind on close

(
set -x

/etc/mknod -f $DEVDIR/ft  c  4 $((104 + $ft_unit))
/etc/mknod -f $DEVDIR/nft c  4 $((120 + $ft_unit))
)

/bin/chmog 666 sys sys $DEVDIR/nft $DEVDIR/ft

read_input "Assign default tape device /dev/tape to this device" \
  ANS "y" require_yes_or_no

if is_yes $ANS; then
	ln -f /dev/nft /dev/tape
	echo "/dev/tape linked to /dev/nft"
fi

exit 0
