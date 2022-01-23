# Set of utility routines used by the git porcelain examples
# To be sourced into the application's namespace

# Load the git package into the ::git namespace
set GIT_NS ::git
source [file join [file dirname [file dirname [info script]]] libgit2.tcl]
namespace path [linsert [namespace path] 0 ${GIT_NS}]

${GIT_NS}::init d:/src/vcpkg-export/libgit2/installed/x64-mingw-dynamic/bin/libgit2.dll

proc inform {message} {
    if {![option Quiet 0]} {
        puts stderr $message
    }
}

proc hexify_id {id} {
    # Return hex form of a libgit2 OID
    return [binary encode hex [dict get $id id]]
}

proc option {opt default} {
    variable Options
    if {[info exists Options($opt)]} {
        return $Options($opt)
    }
    return $default
}

proc option? {opt varname} {
    variable Options
    if {[info exists Options($opt)]} {
        upvar 1 $varname value
        set value $Options($opt)
        return 1
    }
    return 0
}

proc option_set {opt value} {
    variable Options
    set Options($opt) $value
}

# getopt
# Adapted from https://wiki.tcl-lang.org/page/alternative+getopt
# Alternatives in case this code is not satisfactory:
# - https://github.com/tcler/getopt.tcl
# - https://wiki.tcl-lang.org/page/basic+getopts
# - https://wiki.tcl-lang.org/page/GetOpt%2Dish
# Note any alternative must allow option repetition. Check if the ones
# above do that before use.

# Applications can redefine procs in the getopt::app namespace to get
# different behaviors.
namespace eval getopt {
    namespace export getopt
}

namespace eval getopt::app {
    if {[llength [info commands error_note]] == 0} {
        proc error_note {msg} {
            puts stderr $msg
        }
    }
    if {[llength [info commands abort]] == 0} {
        proc abort {exit_code} {
            exit $exit_code
        }
    }
    if {[llength [info commands program_name_prefix]] == 0} {
        proc program_name_prefix {} {
            set name "[file tail [info nameofexecutable]] [file tail $::argv0]"
        }
    }

    if {[llength [info commands help_prelude]] == 0} {
        proc help_prelude {} {
            set prelude {}
            if {![catch {open [uplevel 1 info script]} f]} {
                while {[gets $f line] > 0} {
                    if {[string match "#\[#!]*" $line]} continue
                    if {[string match "#*" $line]} {
                        lappend prelude [regsub {^#\s*} $line {}]
                    } else {
                        break
                    }
                }
                close $f
            }
            return [join $prelude \n]
        }
    }
}

proc getopt::getopt {args} {
    if {[llength $args] == 3} {
        lassign $args argvar list body
    } elseif {[llength $args] == 4} {
        lassign $args optvar argvar list body
        upvar 1 $optvar option
    } else {
        return -code error -errorcode {TCL WRONGARGS} \
          {wrong # args: should be "getopt ?optvar? argvar list body"}
    }
    upvar 1 $argvar value
    set arg(missing) [dict create pattern missing argument 0]
    set arg(unknown) [dict create pattern unknown argument 0]
    set arg(argument) [dict create pattern argument argument 0]
    if {[llength [info commands ::help]] == 0} {
        interp alias {} ::help {} return -code 99 -level 0
    }
    lappend defaults --help "# display this help and exit\nhelp" \
      arglist #\n[format {getopt::noargs ${%s}} $argvar] \
      missing [format {getopt::missing ${%s}} $argvar] \
      argument [format {getopt::argoop ${%s}} $argvar] \
      unknown [format {getopt::nfound %s ${%s}} [list $body] $argvar]
    # Can't use dict merge as that could mess up the order of the patterns
    foreach {pat code} $defaults {
        if {![dict exists $body $pat]} {dict set body $pat $code}
    }
    dict for {pat code} $body {
        switch -glob -- $pat {
            -- {# end-of-options option}
            --?*:* {# long option requiring an argument
                set arg([lindex [split $pat :] 0]) \
                  [dict create pattern $pat argument 1]
            }
            --?* {# long option without an argument
                set arg($pat) [dict create pattern $pat argument 0]
            }
            -?* {# short options
                set last ""; foreach c [split [string range $pat 1 end] ""] {
                    if {$c eq ":" && $last ne ""} {
                        dict set arg($last) argument 1
                        set last ""
                    } else {
                        set arg(-$c) [dict create pattern $pat argument 0]
                        set last -$c
                    }
                }
            }
        }
    }
    while {[llength $list]} {
        set rest [lassign $list opt]
        # Does it look like an option?
        if {$opt eq "-" || [string index $opt 0] ne "-"} break
        # Is it the end-of-options option?
        if {$opt eq "--"} {set list $rest; break}
        set option [string range $opt 0 1]
        set value 1
        if {$option eq "--"} {
            # Long format option
            set argument [regexp {(--[^=]+)=(.*)} $opt -> opt value]
            if {[info exists arg($opt)]} {
                set option $opt
            } elseif {[llength [set match [array names arg $opt*]]] == 1} {
                set option [lindex $match 0]
            } else {
                # Unknown or ambiguous option
                set value $opt
                set option unknown
            }
            if {[dict get $arg($option) argument]} {
                if {$argument} {
                } elseif {[llength $rest]} {
                    set rest [lassign $rest value]
                } else {
                    set value $option
                    set option missing
                }
            } elseif {$argument} {
                set value $option
                set option argument
            }
        } elseif {![info exists arg($option)]} {
            set value $option
            set option unknown
            if {[string length $opt] > 2} {
                set rest [lreplace $list 0 0 [string replace $opt 1 1]]
            }
        } elseif {[dict get $arg($option) argument]} {
            if {[string length $opt] > 2} {
                set value [string range $opt 2 end]
            } elseif {[llength $rest]} {
                set rest [lassign $rest value]
            } else {
                set value $option
                set option missing
            }
        } elseif {[string length $opt] > 2} {
            set rest [lreplace $list 0 0 [string replace $opt 1 1]]
        }
        invoke [dict get $arg($option) pattern] $body
        set list $rest
    }
    set option arglist
    set value $list
    invoke arglist $body
}

proc getopt::invoke {pat body} {
    set rc [catch {uplevel 2 [list switch -- $pat $body]} msg]
    if {$rc == 1} {usage $msg}
    if {$rc == 99} {help $body}
}

proc getopt::usage {msg} {
    set name [app::program_name_prefix]
    app::error_note "$name: $msg"
    app::error_note "Try `$name --help' for more information."
    app::abort 2
}

proc getopt::noargs {list} {
    if {[llength $list] > 0} {usage "too many arguments"}
}

proc getopt::missing {option} {
    usage "option '$option' requires an argument"
}

proc getopt::argoop {option} {
    usage "option '$option' doesn't allow an argument"
}

proc getopt::nfound {body option} {
    if {[string match --?* $option]} {
        set map [list * \\* ? \\? \[ \\\[ \] \\\]]
        set possible [dict keys $body [string map $map $option]*]
    } else {
        usage "invalid option -- '$option'"
    }
    if {[llength $possible] == 0} {
        usage "unrecognized option '$option'"
    }
    set msg "option '$option' is ambiguous; possibilities:"
    foreach n $possible {
        if {[string match *: $n]} {set n [string range $n 0 end-1]}
        append msg " '$n'"
    }
    usage $msg
}

proc getopt::comment {code} {
    set lines [split $code \n]
    if {[set x1 [lsearch -regexp -not $lines {^\s*$}]] < 0} {set x1 0}
    if {[set x2 [lsearch -start $x1 -regexp -not $lines {^\s*#}]] < 0} {
        set x2 [llength $lines]
    }
    for {set rc "";set i $x1} {$i < $x2} {incr i} {
        lappend rc [regsub {^\s*#\s?} [lindex $lines $i] {}]
    }
    return $rc
}

proc getopt::help {body} {
    set max 28
    set tab 8
    set arg ""
    set opts {}
    dict for {pat code} $body {
        switch -glob -- $pat {
            -- {}
            --?*: {lappend opts [string range $pat 0 end-1]=WORD}
            --?*:* {
                set x [string first : $pat]
                lappend opts [string replace $pat $x $x =]
            }
            --?* {lappend opts $pat}
            -?* {
                foreach c [split [string range $pat 1 end] {}] {
                    if {$c ne ":"} {lappend opts -$c}
                }
            }
            arglist {
                set lines [comment $code]
                if {[llength $lines] > 0} {
                    set arg [lindex $lines 0]
                } else {
                    set arg {[FILE]...}
                }
                continue
            }
        }
        if {$code eq "-"} continue
        set lines [comment $code]
        if {[llength $lines] == 0} {
            # Hidden option
            set opts {}
            continue
        }
        set short [lsearch -glob -all -inline $opts {-?}]
        set long [lsearch -glob -all -inline $opts {--?*}]
        if {[llength $short]} {
            set str "  [join $short {, }]"
            if {[llength $long]} {append str ", "}
        } else {
            set str "      "
        }
        append str [join $long {, }] " "
        set tab [expr {max($tab, [string length $str])}]
        foreach line $lines {
            lappend out $str $line
            set str ""
        }
        set opts {}
    }
    set name [app::program_name_prefix]
    app::error_note [format {Usage: %s [OPTION]... %s} $name $arg]
    app::error_note [app::help_prelude]
    app::error_note "\nMandatory arguments to long options\
      are mandatory for short options too."
    foreach {s1 s2} $out {
        if {[string length $s1] > $tab} {
            app::error_note $s1
            set s1 ""
        }
        app::error_note [format {%-*s %s} $tab $s1 $s2]
    }
    app::abort 1
}

namespace import getopt::*
