x64:
	# generate an 128-byte XOR key
	generate $KEY 128

	# Load the runner PIC.
	load "bin/loader.x64.o"
		make pic +optimize +gofirst
	
		# Merge in LibTCG.
		mergelib "libtcg.x64.zip"
	
		# Opt into dynamic function resolution using the resolve() function.
		dfr "resolve" "ror13" "KERNEL32, NTDLL"
		dfr "resolve_unloaded" "strings"

	# Load the PICO
	load "bin/execute_assembly.x64.o"
		make object +optimize
		# Export as bytes and link as "my_pico".
		export
		link "my_pico"
	
	# Load the .NET assembly and link as "my_assembly".
	resolve "%ASSEMBLY_PATH"
	load %ASSEMBLY_PATH
		xor $KEY
		preplen
		link "my_assembly"

	# load our XOR key and link it in as my_key
	push $KEY
		preplen
		link "my_key"
	
	# Patch in arguments to the .NET assembly.
	pack $CMDLINE_BYTES "Z" %CMDLINE
	patch "__CMDLINE__" $CMDLINE_BYTES

	# Export the resulting PIC.
	export