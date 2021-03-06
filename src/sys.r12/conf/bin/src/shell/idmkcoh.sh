# (-lgl
#	Coherent 386 release 4.2-Beta
#	Copyright (c) 1982, 1993 by Mark Williams Company.
#	All rights reserved. May not be copied without permission.
#	For copying permission and licensing info, write licensing@mwc.com
# -lgl)

# idmkcoh - link a COHERENT kernel

##################### FUNCTION DEFINITIONS #######################

# Output an error message to standard error, prefixed by the command name.

report_error () {
	basename $COMMAND_NAME "" "echo " ": $*" 1>&2
}


# Report a usage message to standard error.

usage () {
	report_error Make new COHERENT system kernel.
	cat <<END 1>&2
usage:	[ -o kernel-file ] [ -r conf-dir ]
	kernel-file defaults to "$TARGET"
	conf-dir defaults to "$CONFDIR"
END
}

. /usr/lib/shell_lib.sh
COMMAND_NAME=$0
source_path $0 "HOME_DIR="
parent_of $HOME_DIR "CONFDIR="
CONF_PATH="$(pwd):$CONFDIR:$CONFDIR/install_conf:$CONF_PATH"
K386LIB=$CONFDIR/lib

# Parse command line.

TARGET=${CONFDIR}/coherent
FORCE_KERNEL=force_kernel

while getopts o:r: SWITCH ; do
	case $SWITCH in
	o)	: source_path $OPTARG/tmp "TARGET="
		TARGET=$OPTARG
		;;

	r)	: source_path $OPTARG/tmp "CONFDIR="
		CONFDIR=$OPTARG
		CONF_PATH=$CONFDIR:$CONF_PATH
		;;

	*)	usage
		exit 100
		;;
	esac
done

shift $(($OPTIND - 1))

is_equal $# 0 || {
	echo Bad trailing arguments: $*
	usage
	exit 100
}


# Locate all of the individual configuration files

find_and_set () {
	find_file $1 $CONF_PATH "$2=" \
		"report_error File \'$1\' is not found in $CONF_PATH; exit 126"
}

find_and_set mtune MTUNE_FILE
find_and_set stune STUNE_FILE
find_and_set mdevice MDEV_FILE
find_and_set sdevice SDEV_FILE
find_and_set Makefile MAKE_FILE
find_and_set template.mak TEMPL_FILE
find_and_set keeplist KEEP_FILE

/bin/echo "Configuring $TARGET kernel.  Pardon our dust..."

(source_path $MAKE_FILE "cd "
 make TARGET=$TARGET CONFDIR=$CONFDIR K386LIB=$K386LIB \
	MTUNE_FILE=$MTUNE_FILE STUNE_FILE=$STUNE_FILE \
	MDEV_FILE=$MDEV_FILE SDEV_FILE=$SDEV_FILE \
	TEMPL_FILE=$TEMPL_FILE KEEP_FILE=$KEEP_FILE \
	FORCE_KERNEL="$FORCE_KERNEL") || exit 1
