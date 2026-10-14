#!/usr/bin/env tclsh
#
# Generate a C array based on file content.

proc usage {} {
    puts stderr "Usage: [file tail [info nameofexecutable]] $::argv0 ?options? ?inputfile ...?"
    puts stderr "Options:"
    puts stderr "-b\t\tOutput C array as an array of hex bytes with no"
    puts stderr "\t\tnul terminator. By default the array is generated"
    puts stderr "\t\tin string form with a nul terminator."
    puts stderr "-c\t\tStore gzip-compressed content in array. Implies -b."
    puts stderr "-h HFILE\tWrite a C header to file HFILE."
    puts stderr "-f\t\tOverwrite existing output files."
    puts stderr "-o output\tOutput file. By default, writes to stdout."
    puts stderr "-p PREFIX\tPrefix to use for the C identifier."
    puts stderr ""
    puts stderr "Generates a C array definition containing the contents"
    puts stderr "of each file specified on the command line. If no input"
    puts stderr "files are specified, reads from stdin."
}

proc main {} {
    set args $::argv
    array default set opts ""
    set opts(-b) 0
    set opts(-f) 0
    set opts(-c) 0
    set opts(-p) "arr_"
    while {[llength $args]} {
        set opt [lpop args 0]
        switch -glob $opt {
            -- {break}
            -b - -c - -f {set opts($opt) 1}
            -h - -o - -p {
                if {[llength $args] == 0} {
                    puts stderr "No value specified with option \"$opt\""
                    usage
                    exit 1
                }
                set opts($opt) [lpop args 0]
            }
            -* {
                puts stderr "Unknown option \"$opt\""
                usage
                exit 1
            }
            default {
                set args [linsert $args 0 $opt]
                break
            }
        }
    }
    if {$opts(-o) eq ""} {
        set outfd stdout
    } else {
        if {[file exists $opts(-o)] && !$opts(-f)} {
            puts stderr "File $opts(-o) exists. Use option -f to force overwrite."
            exit 1
        }
        set outfd [open $opts(-o) w]
    }
    if {[file exists $opts(-h)] && !$opts(-f)} {
        puts stderr "File $opts(-h) exists. Use option -f to force overwrite."
        exit 1
    }

    if {[llength $args] == 0} {
        fconfigure stdin -translation binary
        set inputs [list "" [read stdin]]
    } else {
        set inputs [list ]
        foreach fn $args {
            set baseName [regsub -all {[^0-9a-zA-Z]} [file tail $fn] _]
            lappend inputs $baseName [readFile $fn binary]
        }
    }

    set headers [list ]
    puts $outfd "/*"
    puts $outfd " * This file was generated using the command"
    puts $outfd " *     [file tail [info nameofexecutable]] $::argv0 $::argv"
    puts $outfd " */"
    puts $outfd ""
    foreach {baseName content} $inputs {
        set arrayName [string cat $opts(-p) $baseName]
        if {$opts(-c)} {
            set content [zlib gzip $content]
        }
        if {$opts(-b) || $opts(-c)} {
            puts $outfd [toHexArray $arrayName $content]
            lappend headers "extern const unsigned char $arrayName\[];"
        } else {
            puts $outfd [toStringLiteral $arrayName $content]
            lappend headers "extern const unsigned char $arrayName\[];"
        }
        lappend headers "extern const size_t ${arrayName}_len;"
    }
    if {$opts(-h) ne ""} {
        set hfd [open $opts(-h) w]
        set guard [string toupper [regsub -all {[^0-9a-zA-Z]} [file tail $opts(-h)] _]]
        puts $hfd "#ifndef $guard"
        puts $hfd "#define $guard"
        puts $hfd "/*"
        puts $hfd " * This file was generated using the command"
        puts $hfd " *     [file tail [info nameofexecutable]] $::argv0 $::argv"
        puts $hfd " */"
        puts $hfd ""
        puts $hfd [join $headers \n]
        puts $hfd ""
        puts $hfd "#endif /* $guard */"
    }
}

proc toHexArray {arrayName content} {
    set len [string length $content]
    set rows {}
    set completed 0
    # Do 16 bytes at a time
    const row_size 8
    while {$completed < $len} {
        set piece [string range $content $completed [expr {$completed+$row_size-1}]]
        incr completed $row_size
        binary scan $piece H* hex
        lappend rows [regsub -all {..} $hex {0x&, }]
    }
    append def "const unsigned char $arrayName\[] = \{\n"
    append def [regsub -all -lineanchor {^} [join $rows \n] "    "]
    append def "\n    0x0 /* Additional terminating nul */\n"
    append def "\};\n"
    append def "/* Length not including terminating nul */\n"
    append def "size_t ${arrayName}_len = $len;\n"
}

proc toStringLiteral {arrayName content} {
    set len [string length $content]
    set lines [lmap line [split $content \n] {
        set line [mapAsString $line]
        string cat \" $line \\n \"
    }]
    append def "const char $arrayName\[] = \{\n"
    append def [regsub -all -lineanchor {^} [join $lines \n] "    "]
    append def "\n\};\n"
    append def "/* Length not including terminating nul */\n"
    append def "size_t ${arrayName}_len = $len;\n"
}

proc mapAsString {line} {
    # Set up a map that transforms ASCII control and non-ASCII.
    # CR, LF -> \r \n respectively although $line should
    # really not contain LF's
    # quotes and backslashes -> add preceding backslash
    # Other control chars and non-ASCII -> octal escapes
    #
    # We could also escape ? here to avoid misinterpretation as
    # a ?? trigraph sequence but that is done below as a separate
    # step to avoid unnecessary escaping.
    set map [list \\ \\\\ \" \\\" \n \\n \r \\r]
    for {set ch 0} {$ch < 32} {incr ch} {
        # Skip tabs, cr and lf
        if {$ch != 9 && $ch != 10 && $ch != 13} {
            lappend map [format %c $ch] [format {\%03o} $ch]
        }
    }
    for {set ch 127} {$ch < 256} {incr ch} {
        lappend map [format %c $ch] [format {\%03o} $ch]
    }
    # Consecutive ?'s can be misinterpreted as trigraphs. Escape them
    # in the final proc definition.
    proc mapAsString line "regsub -all {\\?\\?} \[string map [list $map] \$line\] ?\\\\?"
    tailcall mapAsString $line
}

if {[file normalize $argv0] eq [file dirname [file normalize [info script]/...]]} {
    main
}

