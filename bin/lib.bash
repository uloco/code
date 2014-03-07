#!bash
# lib.bash - a few very basic functions for bash cripts

if [[ $__LIBROOT ]]; then
	return
else
	__LIBROOT=${BASH_SOURCE[0]%/*}
fi

# $LVL is like $SHLVL, but zero for programs ran interactively;
# it is used to decide when to prefix errors with program name.

_lvl=$(( LVL++ )); export LVL

## Variable defaults

: ${XDG_CACHE_HOME:=$HOME/.cache}
: ${XDG_CONFIG_HOME:=$HOME/.config}
: ${XDG_DATA_HOME:=$HOME/.local/share}
: ${XDG_DATA_DIRS:=/usr/local/share:/usr/share}

## Logging

progname=${0##*/}
progname_prefix=-1

print_msg() {
	local prefix=$1 msg=$2 color reset
	if [[ -t 1 ]]; then
		color=$3 reset=${color:+'\e[m'}
	fi
	if [[ $DEBUG ]]; then
		local progname="$progname[$$]"
	fi
	if [[ $DEBUG || $progname_prefix -gt 0 ||
	      ( $progname_prefix -le 0 && $_lvl -gt 0 ) ]]; then
		printf "%s: ${color}%s:${reset} %s\n" "$progname" "$prefix" "$msg"
	else
		printf "${color}%s:${reset} %s\n" "$prefix" "$msg"
	fi
}

print_fmsg() {
	local level=$1 msg=$2 color=$3 fprefix=$4 fcolor=$5 reset
	if [[ $DEBUG ]]; then
		print_msg "$level" "$msg" "$color"
		return
	fi
	if [[ -t 1 ]]; then
		color="$fcolor" reset='\e[m'
	fi
	if [[ $progname_prefix -gt 0 ||
	      ( $progname_prefix -le 0 && $_lvl -gt 0 ) ]]; then
		printf -- "%s: ${color}%s${reset} %s\n" "$progname" "$fprefix" "$msg"
	else
		printf -- "${color}%s${reset} %s\n" "$fprefix" "$msg"
	fi
}

debug() {
	local color reset
	if [[ -t 1 ]]; then
		color='\e[36m' reset='\e[m'
	fi
	if [[ $DEBUG ]]; then
		printf "%s[%s]: ${color}debug @ %s:${reset} %s\n" \
			"$progname" "$$" "${FUNCNAME[1]}" "$*"
	fi
	return 0
} >&2

say() {
	if [[ $DEBUG ]]; then
		print_msg 'info' "$*" '\e[1;34m'
	elif [[ $VERBOSE ]]; then
		printf "%s\n" "$*"
	fi
	return 0
}

log() {
	print_fmsg 'log' "$*" '\e[32m' '--' '\e[32m'
}

status() {
	log "$*"
	settitle "$progname: $*"
}

log2() {
	print_fmsg 'log2' "$*" '\e[1;35m' '==' '\e[35m'
}

notice() {
	print_fmsg 'notice' "$*" '\e[1;35m' '!' '\e[1;35m'
} >&2

warn() {
	print_msg 'warning' "$*" '\e[1;33m'
	if (( DEBUG > 1 )); then backtrace; fi
	(( ++warnings ))
} >&2

err() {
	print_msg 'error' "$*" '\e[1;31m'
	if (( DEBUG > 1 )); then backtrace; fi
	! (( ++errors ))
} >&2

die() {
	print_msg 'fatal' "$*" '\e[1;31m'
	if (( DEBUG > 1 )); then backtrace; fi
	exit 1
} >&2

xwarn() {
	printf '%s\n' "$*"
	(( ++warnings ))
} >&2

xerr() {
	printf '%s\n' "$*"
	! (( ++errors ))
} >&2

xdie() {
	printf '%s\n' "$*"
	exit 1
} >&2

confirm() {
	local text=$1 prefix color reset=$'\e[m' si=$'\001' so=$'\002'
	case $text in
	    "error: "*)
		prefix="(!)"
		color=$'\e[1;31m';;
	    "warning: "*)
		prefix="(!)"
		color=$'\e[1;33m';;
	    *)
		prefix="(?)"
		color=$'\e[1;36m';;
	esac
	local prompt=${si}${color}${so}${prefix}${si}${reset}${so}" "${text}" "
	local answer="n"
	read -e -p "$prompt" answer <> /dev/tty && [[ $answer == y ]]
}

backtrace() {
	local -i i=${1:-1}
	printf "%s[%s]: call stack:\n" "$progname" "$$"
	for (( 1; i < ${#BASH_SOURCE[@]}; i++ )); do
		printf "... %s:%s: %s -> %s\n" \
			"${BASH_SOURCE[i]}" "${BASH_LINENO[i-1]}" \
			"${FUNCNAME[i]:-?}" "${FUNCNAME[i-1]}"
	done
} >&2

## Various

use() {
	local lib file
	for lib; do
		file="lib$lib.bash"
		if have "$file"; then
			debug "loading $file from path"
		else
			debug "loading $file from libroot"
			file="$__LIBROOT/$file"
		fi
		. "$__LIBROOT/$file" || die "failed to load $file"
	done
}

have() {
	command -v "$1" >&/dev/null
}

now() {
	date +%s "$@"
}

older_than() {
	local file=$1 date=$2 filets datets
	filets=$(stat -c %y "$file")
	datets=$(date +%s -d "$date ago")
	(( filets < datets ))
}

## Final

debug "lib.bash loaded by $0 from $__LIBROOT"
