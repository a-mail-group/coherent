# hai/mkdev - get SCSI device info
# Revised: Tue Mar  1 14:36:52 1994 CST

DEVDIR=/dev

# Things to do:

#	Ask if the driver should be enabled.
#
#	Configure host adapter.
#
#	For SCSI disk devices:
#		Report current SCSI id's.
#		Ask for changes.
#		Tune kernel variable.
#		Make device nodes.

#	For SCSI tape devices:
#		Report current SCSI id's.
#		Ask for changes.
#		Tune kernel variable.
#		Make device nodes.


. /usr/lib/shell_lib.sh
COMMAND_NAME=$0
source_path $0 "HOME_DIR="
parent_of $HOME_DIR "CONF_DIR="

. $CONF_DIR/bin/conf_lib.sh


###############
# Main logic.
#
# Configure driver.
# If driver enabled
#   configure hard drive devices
#   configure tape drive devices
###############

# Enable/disable hai device driver.
echo "\nAdaptec 154x MAIN driver, SCSI hard disk and/or SCSI tape."
set_enable_status hai

# Hardware configuration of HBA.

read_input "Change host adapter configuration" ANS "n" require_yes_or_no
if is_yes $ANS; then
	echo "
If you are not sure how to set any of the following parameters, just hit
<Enter> to keep the current setting.  Further information can be found
in your host adapter documentation, as well as in COHERENT files
/etc/conf/mtune and /etc/conf/hai/Space.c.
"
	while : ; do

		tune HAI_AHABASE_SPEC	"Host adapter base i/o port address"
		tune HAI_AHAINTR_SPEC	"Host adapter interrupt number"
		tune HAI_AHADMA_SPEC	"Host adapter DMA channel"

		tune HAI_SD_HDS_SPEC	"# of heads on hard drive (translation mode)"
		tune HAI_SD_SPT_SPEC	"sectors per track on hard drive (translation mode)"

		tune HAI_AHAXFERSPEED	"Host adapter transfer speed (0=>5Mb/s)"
		tune HAI_AHABUSOFFTIME	"Host adapter bus off time (microseconds)"
		tune HAI_AHABUSONTIME	"Host adapter bus on time (microseconds)"

		read_input "\nIs host adapter configuration correct" ANS "y" \
		  require_yes_or_no
		is_yes $ANS && break
	done
fi


# Get SCSI id's for attached hard drives.

while : ; do
	get_tunable HAI_DISK_SPEC DISKS
	if val $(($DISKS == 0)) ; then
		echo "No SCSI hard drives are configured."
	else
		echo "Hard drives are configured at the following SCSI id numbers:"
		for i in 0 1 2 3 4 5 6 7 ; do
			val $((($DISKS & (1 << $i)) != 0)) && echo -n $i " "
		done
		echo
	fi
	read_input "Is this correct" ANS "y" require_yes_or_no || continue
	is_yes $ANS && break

	echo Enter all hard drive SCSI id\'s, separated by spaces.
	read_input "Disk ID's or <Enter>" DISKLIST ""

	DISKS=0
	for i in $DISKLIST; do
		is_numeric $i || {
			echo $i is not a number. Please try again.
			continue 2
		}
		val $(($i < 0 || $i > 7)) && {
			echo $i is not a valid drive ID. Only 0 through 7 are valid ID\'s
			continue 2
		}

		DISKS=$(($DISKS | (1 << ($i))))
	done

	$CONF_DIR/bin/idtune -f HAI_DISK_SPEC $DISKS
done

for i in 0 1 2 3 4 5 6 7; do
	val $((($DISKS & (1 << $i)) == 0)) && continue

	MINOR=$(($i * 16))

	/etc/mknod -f $DEVDIR/sd${i}x b 13 $(($MINOR + 128))
	/etc/mknod -f $DEVDIR/rsd${i}x c 13 $(($MINOR + 128))
	/etc/mknod -f $DEVDIR/sd${i}a b 13 $MINOR
	/etc/mknod -f $DEVDIR/rsd${i}a c 13 $MINOR
	/etc/mknod -f $DEVDIR/sd${i}b b 13 $(($MINOR + 1))
	/etc/mknod -f $DEVDIR/rsd${i}b c 13 $(($MINOR + 1))
	/etc/mknod -f $DEVDIR/sd${i}c b 13 $(($MINOR + 2))
	/etc/mknod -f $DEVDIR/rsd${i}c c 13 $(($MINOR + 2))
	/etc/mknod -f $DEVDIR/sd${i}d b 13 $(($MINOR + 3))
	/etc/mknod -f $DEVDIR/rsd${i}d c 13 $(($MINOR + 3))
	/bin/chmog 600 sys sys $DEVDIR/*sd${i}*
done

# Get SCSI id's for attached tape drives.
while : ; do
	get_tunable HAI_TAPE_SPEC TAPES

	if val $(($TAPES == 0)) ; then
		echo "No SCSI tape drives are configured."
	else
		echo "Tape drives are configured at the following SCSI id numbers:"
		for i in 0 1 2 3 4 5 6 7 ; do
			val $((($TAPES & (1 << $i)) != 0)) && echo -n $i " "
		done
		echo
	fi

	read_input "Is this correct" ANS "y" require_yes_or_no || continue
	is_yes $ANS && break

	echo Enter all tape drive SCSI id\'s, separated by spaces.
	read_input "Tape ID's or <Enter>" TAPELIST ""

	TAPES=0
	for i in $TAPELIST; do
		is_numeric $i || {
			echo $i is not a number in the range 0 through 7. Please try again.
			continue 2
		}
		val $(($i < 0 || $i > 7)) && {
			echo $i is not a valid tape ID. Only 0 through 7 are valid ID\'s
			continue 2
		}

		TAPES=$(($TAPES | (1 << ($i))))
	done

	$CONF_DIR/bin/idtune -f HAI_TAPE_SPEC $TAPES
done


DFLT_TAPE=
for i in 0 1 2 3 4 5 6 7; do
	val $((($TAPES & (1 << $i)) == 0)) && continue

	MINOR=$((16 * $i + 129))
	/etc/mknod -f $DEVDIR/rStp$i c 13 $(($MINOR + 2))
	/etc/mknod -f $DEVDIR/nrStp$i c 13 $MINOR
	/bin/chmog 600 sys sys $DEVDIR/*Stp$i

	if [ -z "$DFLT_TAPE" ]; then
		read_input \
		"Assign default tape device /dev/tape to SCSI ID $i" \
		ANS "y" require_yes_or_no

		if is_yes $ANS; then
			DFLT_TAPE=$i
			ln -f /dev/nrStp$i /dev/tape
			echo "/dev/tape linked to /dev/nrStp$i"
		fi
	fi
done

exit 0
