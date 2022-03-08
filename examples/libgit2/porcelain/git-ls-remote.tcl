# Demo of cffi libgit extension. Poor man's git ls-remote emulation from libgit2
# Translated to Tcl from libgit2/examples/ls-remote.c

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

proc parse_ls-remote_options {arguments} {
    parse_options opt arg $arguments {
        arglist {
            # [REMOTE]
            set rest $arg
        }
    }
    if {[llength $rest] > 1} {
        getopt::usage "Too many arguments."
    }
    return [lindex $rest 0]
}

proc git-ls-remote {arguments} {
    set remote [parse_ls-remote_options $arguments]
    if {$remote eq ""} {
        set remote origin
    }

    set pRemote NULL
    set pRepo [open_repository]
    try {
        set pRemote [git_remote_lookup $pRepo $remote]
        git_remote_connect $pRemote GIT_DIRECTION_FETCH {} {} NULL
        set pRefs [git_remote_ls nrefs $pRemote]
        # Note pRefs internal, unsafe. Not to be freed or disposed
        for {set i 0} {$i < $nrefs} {incr i} {
            # pRefs points to an array of pointers to git_remote_head structures
            # Get the pointer to the i'th structure
            set pRemoteHead [cffi::memory get! $pRefs pointer.${::GIT_NS}::git_remote_head $i]
            set remote_head [git_remote_head fromnative! $pRemoteHead]
            set oid [git_oid_tostr_s [dict get $remote_head oid]]
            set name [::cffi::memory tostring! [dict get $remote_head name]]
            puts "$oid\t$name"
        }
    } finally {
        git_remote_free $pRemote
        git_repository_free $pRepo
    }
}

source [file join [file dirname [info script]] porcelain-utils.tcl]
catch {git-ls-remote $::argv} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    puts [dict get $edict -errorinfo]
    exit 1
}
