# bash completion support for Crystal Palace
#
# (c) 2026 Raphael Mudge, Adversary Fan Fiction Writers Guild
# Distributed under the BSD license. See LICENSE with Crystal Palace
#
# Created with the help of LLM AI. Welcome robotic hacker overlords

_cpl_completions() {
	local cur="${COMP_WORDS[COMP_CWORD]}"
	local prev="${COMP_WORDS[COMP_CWORD-1]}"

	# 1. Complete subcommands if it's the very first word
	if [[ ${COMP_CWORD} -eq 1 ]]; then
		COMPREPLY=( $(compgen -W "build coffparse disassemble help link server" -- "$cur") )
		return 0
	fi

	# 2. If the user typed 'help', complete the available commands
	if [[ ${COMP_CWORD} -eq 2 && "$prev" == "help" ]]; then
		COMPREPLY=( $(compgen -W "build coffparse disassemble help link server" -- "$cur") )
		return 0
	fi

	# 3. If the current word starts with '@', complete files with the prefix
	if [[ "$cur" == @* ]]; then
		# Strip the '@' off what they typed so we can search real filenames
		local search_file="${cur#@}"

		# -f completes files, -P "@" prepends the @ back onto the suggestions
		COMPREPLY=( $(compgen -P "@" -f -- "$search_file") )
		return 0
	fi

	# 4. If it's anything else, COMPREPLY stays empty,
	# and '-o default' takes over to suggest normal files.
}

complete -o default -F _cpl_completions cpl
