x64:
	load "bin/loader-postex.x64.o"        # read the loader COFF
		make pic +gofirst          # turn it into PIC and ensure the go function is at the start
		dfr "resolve" "ror13"      # use ror13 with the resolve method for resolving dfr functions
		mergelib "../libtcg.x64.zip"  # merge the shared library

		# DLL bytes
		push $DLL
			link "dll"

		# DLL Args from File
		load %ARGFILE
			preplen
			link "dll_args"

		# export the final pic
		export
