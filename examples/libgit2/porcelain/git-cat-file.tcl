# Demo of cffi libgit extension. Poor man's git COMMAND emulation from libgit2
# Translated to Tcl from libgit2/examples/COMMAND.c
# tclsh git-COMMAND.tcl --help

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

proc parse_options {arguments} {
    getopt::getopt opt arg $arguments {
        -stp {
            # Show size / type / prettified output of object instead
            # of raw content
            option_set_once Action $opt
        }
        -e {
            # Check existence and validity of object without printing anything
            option_set_once Action $opt
        }
        -v {
            # Set verbose output mode
            option_set Verbose 1
        }
        --git-dir:GITDIR {
            # Specify the path to the repository
            option_set GitDir $arg
        }
        arglist {
            # [TYPE] OBJECT
            set rest $arg
        }
    }

    set nrest [llength $rest]
    if {![option? Action action]} {
        if {$nrest < 2} {
            getopt::usage "The TYPE argument must be present if one of -e, -p, -s or -t is not specified."
        }
        if {$nrest == 2} {
            lassign $rest type object
            set types {blob tree commit tag}
            if {$type ni $types} {
                getopt::usage "Invalid type \"$type\". Must be one of [join $types {, }]."
            }
            return [list $object $type]
        }
    } else {
        if {$nrest == 1} {
            return $rest
        }
        if {$nrest == 2} {
            getopt::usage "The TYPE argument must not be specified if one of -e, -p, -s or -t is present."
        }
    }

    getopt::usage "Wrong number of arguments"
}

proc show_size {pRepo pObj} {
    git_repository_odb pOdb $pRepo
    try {
        git_odb_read pOdbObj $pOdb [git_object_id $pObj]
        puts [git_odb_object_size $pOdbObj]
    } finally {
        if {[info exists pOdbObj]} {
            git_odb_object_free $pOdbObj
        }
        git_odb_free $pOdb
    }
}

proc pretty_print {pObj} {
    switch -exact -- [git_object_type $pObj] {
        GIT_OBJECT_COMMIT { show_commit $pObj }
        GIT_OBJECT_BLOB   { show_blob $pObj }
        GIT_OBJECT_TREE   { show_tree $pObj }
        GIT_OBJECT_TAG    { show_tag $pObj }
        default { error "Unknown type [git_object_type $pObj]"}
    }

}

proc main {} {
    lassign [parse_options $::argv] object expected_type

    option_set Verbose [option Verbose 0]; # Explicitly set if unset

    git_repository_open_ext pRepo [option GitDir .]
    set action [option Action ""]
    if {$action eq "-e"} {
        set ::SUPPRESS_OUTPUT 1
    }
    try {
        git_revparse_single pObj $pRepo $object
        if {[option! Verbose]} {
            set oid [git_oid_tostr_s [git_object_id $pObj]]
            set type [git_object_type2string [git_object_type $pObj]]
            puts "$type $oid\n--"
        }
        switch -exact -- [option Action ""] {
            -t {
                puts [git_object_type2string [git_object_type $pObj]]
            }
            -e {
                # Do not print anything. Just return. Any errors that
                # where generated will result in a non-zero exit code
            }
            -s {
                show_size $pRepo $pObj
            }
            -p {
                pretty_print $pObj
            }
            "" {
                show $pRepo $pObj $expected_type
            }
        }
    } finally {
        if {[info exists pObj]} {
            git_object_free $pObj
        }
        git_repository_free $pRepo
    }
}

set SUPPRESS_OUTPUT 0
source [file join [file dirname [info script]] porcelain-utils.tcl]
catch {main} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    if {! $SUPPRESS_OUTPUT} {
        puts stderr $result
    }
    exit 1
}
