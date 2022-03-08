# Demo of cffi libgit extension. Poor man's git ls-files emulation from libgit2
# Translated to Tcl from libgit2/examples/ls-files.c

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

proc parse_ls-files_options {arguments} {
    parse_options opt arg $arguments {
    }
    return
}

proc git-ls-files {arguments} {
    parse_ls-files_options $arguments

    set pIndex NULL
    set pRepo [open_repository]
    try {
        set pIndex [git_repository_index $pRepo]
        set count [git_index_entrycount $pIndex]
        for {set i 0} {$i < $count} {incr i} {
            set entry [git_index_get_byindex $pIndex $i]
            puts [dict get $entry path]
        }
    } finally {
        git_repository_free $pRepo
    }
}

source [file join [file dirname [info script]] porcelain-utils.tcl]
catch {git-ls-files $::argv} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    exit 1
}
