x64:
	load "bin/loader.x64.o"        # read the loader COFF
		make pic +gofirst          # turn it into PIC and ensure the go function is at the start
		dfr "resolve" "ror13"      # use ror13 with the resolve method for resolving dfr functions
		mergelib "libtcg.x64.zip"  # merge the shared library

		# read the dll being provided
		push $DLL
		# link it to the "dll" section in the loader
		link "dll"

		# export the final pic
		export
