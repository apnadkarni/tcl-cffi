# Demo of cffi libgit extension. Poor man's git push emulation from libgit2
# Translated to Tcl from libgit2/examples/push.c

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

proc parse_push_options {arguments} {
    parse_options opt arg $arguments {
        arglist {
            # [REMOTE [REFSPEC ...]]
            set rest $arg
        }
    }

    return $rest
}

proc git-push {arguments} {
    set arguments [parse_push_options $arguments]
    if {[llength $arguments] == 0} {
        set remote "origin"
    } else {
        set arguments [lassign $arguments remote]
    }

    set push_opts [git_push_options_init]
    set pRefSpecs NULL
    set pRemote NULL
    set pRepo [open_repository]
    try {
        set pRemote [git_remote_lookup $pRepo $remote]
        set pRefSpecs [lg2_strarray_new $arguments]
        git_remote_push $pRemote $pRefSpecs $push_opts
        puts "pushed to $remote"
    } finally {
        lg2_strarray_free $pRefSpecs
        git_repository_free $pRepo
    }
}

source [file join [file dirname [info script]] porcelain-utils.tcl]
catch {git-push $::argv} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    exit 1
}
