#!/bin/bash

# Replace TABs with sequences of 8 spaces in all given files.
replace_tabs() {
    local _file _tmp

    for _file in $*; do
        _tmp=$_file.tabs
        cp $_file $_tmp && \
            cat $_tmp | \
                sed 's/\t/        /g' > $_file && \
        rm -f $_tmp
    done
}


# Replaces lines containing only spaces with empty lines in all given files.
strip_empty_lines() {
    local _file _tmp

    for _file in $*; do
        _tmp=$_file.spaces
        cp $_file $_tmp && \
            cat $_tmp | \
                sed 's/^ [ ]*$//g' > $_file && \
        rm -f $_tmp
    done
}


# Strip trailing white space from all the given files.
strip_trailing_ws() {
    local _file _tmp

    for _file in $*; do
        _tmp=$_file.spaces
        cp $_file $_tmp && \
            cat $_tmp | \
                sed 's/ *$//g' > $_file && \
        rm -f $_tmp
    done
}


# Clean up TABS and empty lines in all given or found files.
if [ -n "$*" ]; then
    replace_tabs $* && \
        strip_empty_lines $* && \
            strip_trailing_ws $*
else
    files=$(find . -name \*.h -o -name \*.c)
    replace_tabs $files && \
        strip_empty_lines $files && \
            strip_trailing_ws $files
fi
