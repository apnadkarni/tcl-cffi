# Demo of cffi libgit extension. Poor man's git tag emulation from libgit2
# Translated to Tcl from libgit2/examples/tag.c

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

proc parse_tag_options {arguments} {
    parse_options opt arg $arguments {
        -a - --annotate {
            # Make a annotated tag
            option_set_once Action tag_annotate {-a/--annotate -l/--list -d/--delete}
        }
        -f - --force {
            # Force replacement of existing tag
            option_set Force $arg
        }
        -l - --list {
            # List tags
            option_set_once Action tag_list {-a/--annotate -l/--list -d/--delete}
        }
        -d - --delete {
            # Delete the tag
            option_set_once Action tag_delete {-a/--annotate -l/--list -d/--delete}
        }
        -m - --message:MESSAGE {
            # Use given tag message. May be specified multiple times.
            option_lappend Message $arg
        }
        arglist {
            # [TAG] [COMMIT]
            set rest $arg
        }
    }

    if {[llength $rest] > 2} {
        getopt::usage "Too many arguments."
    } else {
        return $rest
    }
}

proc get_target {pRepo args} {
    if {[llength $args] == 0} {
        getopt::usage "Tag not specified."
    }
    set tag [lindex $args 0]
    if {[llength $args] == 1} {
        set target HEAD
    } else {
        set target [lindex $args 1]
        puts target:$target
    }
    set target [option Target $target]
    return [list $tag [git_revparse_single $pRepo $target]]
}

proc tag {pRepo args} {
    lassign [get_target $pRepo {*}$args] tag pTarget
    try {
        git_tag_create_lightweight $pRepo $tag $pTarget [option Force 0]
    } finally {
        git_object_free $pTarget
    }
}

proc tag_annotate {pRepo args} {
    if {![option? Message message]} {
        puts -nonewline "Tag message: "
        flush stdout
        set message [gets stdin]
    }
    set pSig NULL
    lassign [get_target $pRepo {*}$args] tag pTarget
    try {
        set pSig [git_signature_default $pRepo]
        git_tag_create $pRepo $tag $pTarget [git_signature fromnative $pSig] $message [option Force 0]
    } finally {
        git_signature_free $pSig
        git_object_free $pTarget
    }
}

proc tag_list {pRepo args} {
    if {[llength $args] > 1} {
        getop::usage "Too many arguments."
    }
    if {[llength $args] == 0} {
        set pattern *
    } else {
        set pattern [lindex $args 0]
    }
    set pStrArray [git_strarray new]
    try {
        git_tag_list_match $pStrArray $pattern $pRepo
        foreach tag [lg2_strarray_strings $pStrArray] {
            puts $tag
        }

    } finally {
        git_strarray_dispose $pStrArray
        git_strarray free $pStrArray
    }
}

proc tag_delete {pRepo args} {
    if {[llength $args] == 0} {
        getopt::usage "Tag not specified."
    } elseif {[llength $args] > 1} {
        getopt::usage "Too many arguments."
    }
    set tag [lindex $args 0]
    set pObj [git_revparse_single $pRepo $tag]
    try {
        set pOidBuf [git_buf new]
        git_object_short_id $pOidBuf $pObj
        git_tag_delete $pRepo $tag
        set oid [::cffi::memory tostring! [git_buf getnative $pOidBuf ptr]]
        puts "Deleted tag '$tag' (was $oid)"
    } finally {
        git_buf_dispose $pOidBuf
        git_buf free $pOidBuf
        git_object_free $pObj
    }
}

proc git-tag {arguments} {
    set arguments [parse_tag_options $arguments]

    set pRepo [open_repository]
    try {
        if {[llength $arguments] == 0} {
            set action [option Action tag_list]
        } else {
            set action [option Action tag]
        }
        $action $pRepo {*}$arguments
    } finally {
        git_repository_free $pRepo
    }
}

source [file join [file dirname [info script]] porcelain-utils.tcl]
catch {git-tag $::argv} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    exit 1
}
