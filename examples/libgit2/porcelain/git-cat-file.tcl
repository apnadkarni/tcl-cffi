# Demo of cffi libgit extension. Poor man's git COMMAND emulation from libgit2
# Translated to Tcl from libgit2/examples/COMMAND.c

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

proc parse_cat-file_options {arguments} {
    parse_options opt arg $arguments {
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
    set pOdb [git_repository_odb $pRepo]
    try {
        set pOdbObj [git_odb_read $pOdb [git_object_id $pObj]]
        puts [git_odb_object_size $pOdbObj]
    } finally {
        if {[info exists pOdbObj]} {
            git_odb_object_free $pOdbObj
        }
        git_odb_free $pOdb
    }
}

proc show_commit {pObj} {
    set pCommit [::cffi::pointer cast $pObj ${::GIT_NS}::git_commit]
    puts "tree [git_oid_tostr_s [git_commit_tree_id $pCommit]]"

    set count [git_commit_parentcount $pCommit]
    for {set i 0} {$i < $count} {incr i} {
        puts "parent [git_oid_tostr_s [git_commit_parent_id $pCommit $i]]"
    }

    print_signature author [git_commit_author $pCommit]
    print_signature committer [git_commit_committer $pCommit]

    set encoded_message [git_commit_message $pCommit]

    # Get the git encoding which uses IANA names and map to Tcl encoding name
    set encoding [lg2_iana_to_tcl_encoding [git_commit_message_encoding $pCommit]]
    set message [cffi::memory tostring! $encoded_message $encoding]
    if {$message ne ""} {
        puts \n$message
    }
}

proc show_blob {pObj} {
    set pBlob [::cffi::pointer cast $pObj ${::GIT_NS}::git_blob]
    set pContent [git_blob_rawcontent $pBlob]
    set nbytes [git_blob_rawsize $pBlob]
    puts [cffi::memory tobinary! $pContent $nbytes]
}

proc show_tree {pObj} {
    set pTree [::cffi::pointer cast $pObj ${::GIT_NS}::git_tree]
    set count [git_tree_entrycount $pTree]
    for {set i 0} {$i < $count} {incr i} {
        set pEntry [git_tree_entry_byindex $pTree $i]
        set oid [git_oid_tostr_s [git_tree_entry_id $pEntry]]
        set name [git_tree_entry_name $pEntry]
        set type [git_object_type2string [git_tree_entry_type $pEntry]]
        set mode [git_tree_entry_filemode_raw $pEntry]
        puts [format "%06o $type $oid\t$name" $mode]
    }
}

proc show_tag {pObj} {
    set pTag [::cffi::pointer cast $pObj ${::GIT_NS}::git_tag]
    set oid [git_tag_target_id $pTag]
    puts "object [git_oid_tostr_s $oid]"
    set type [git_object_type2string [git_tag_target_type $pTag]]
    puts "type $type"
    puts "tag [git_tag_name $pTag]"
    print_signature tagger [git_tag_tagger $pTag]

    # Unlike commits, tags do not seem to have an associated function to
    # get the encoding.
    # Or is this basically a commit so we use git_commit_message_encoding? TBD
    set message [git_tag_message $pTag]

    if {$message ne ""} {
        puts \n$message
    }
}

proc show {pRepo pObj expected_type} {
    set expected_enum [git_object_string2type $expected_type]
    set type [git_object_type $pObj]
    if {$type eq $expected_enum} {
        show_$expected_type $pObj
        return
    }

    # The type is not what is wanted. Peel off layers 
    set pPeeled [git_object_peel $pObj $expected_enum]
    try {
        if {[git_object_type $pPeeled] eq $expected_enum} {
            show_$expected_type $pPeeled
        } else {
            # Should not happen since git_object_peel would have errored but still...
            error "Object was not converted to $expected_type."
        }
    } finally {
        git_object_free $pPeeled
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

proc git-cat-file {arguments} {
    lassign [parse_cat-file_options $::argv] object expected_type

    option_set Verbose [option Verbose 0]; # Explicitly set if unset

    set pRepo [git_repository_open_ext [option GitDir .]]
    set action [option Action ""]
    if {$action eq "-e"} {
        set ::SUPPRESS_OUTPUT 1
    }
    try {
        set pObj [git_revparse_single $pRepo $object]
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
catch {git-cat-file $::argv} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    if {! $SUPPRESS_OUTPUT} {
        puts stderr $result
    }
    exit 1
}
