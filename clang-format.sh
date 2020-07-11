#!/bin/bash
parse_args() {
	while [[ $# -gt 0 ]]
	do
	key="$1"
		case $key in
		    -f)
			echo "Clang formatting the files specified"
		    clang-format -style=file -i -fallback-style=none $2
		    shift # past argument
		    shift # past value
		    ;;
		    -d)
			echo "Clang formatting all files in the directory"
			find $2 -iname *.h -o -iname *.c -o -iname *.h | xargs clang-format -style=file -i -fallback-style=none
		    shift # past argument
		    shift # past value
		    ;;
		    -g)
			echo "Clang formatting only git diff'ed output"
			clang-format -style=file -fallback-style=none -i `git ls-files -om "*.[ch]"`
		    shift # past argument
		    ;;
		    -h|--help)
			echo "-f: Pass list of files to be clang formatted"
			echo "-a: Clang format all files in the project"
			echo "-d: Clang format all files in the directory passed after this option"
			echo "-g: Clang formatting only git diff'ed output"
		    exit 0
		    ;;
		    *)    # unknown option
		    echo "Unknown option $key"
		    exit 1
		    ;;
		esac
	done
}

parse_args $@
