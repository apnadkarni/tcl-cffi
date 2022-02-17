# Demo of cffi libgit extension. Poor man's git commit emulation from libgit2
# Translated to Tcl from libgit2/examples/commit.c
# tclsh git-commit.tcl --help

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

proc parse_options {arguments} {
    getopt::getopt opt arg $arguments {
        -m: - --message:MESSAGE {
            # Use MESSAGE as commit message
            option_set Message $arg
        }
        --git-dir:GITDIR {
            # Specify the path to the repository
            option_set GitDir $arg
        }
    }
}

proc main {} {
    parse_options $::argv

    git_repository_open_ext pRepo [option GitDir .]
    try {
        try {
            git_revparse_ext pParent pRef $pRepo HEAD
        } trap {GIT -3} {} {
            inform "HEAD not found. Creating first commit."
        }

        git_repository_index pIndex $pRepo
        git_index_write_tree tree_oid $pIndex
        git_index_write $pIndex
        git_tree_lookup pTree $pRepo $tree_oid
        git_signature_default pSig $pRepo

        set sig [git_signature fromnative $pSig]

        # The message is in encoded form with encoding used passed as an argument
        set pMessage [::cffi::memory fromstring [option Message "Commit"] utf-8]

        # Commit parents are passed as a list of pointers
        if {[info exists pParent]} {
            set parents [list $pParent]
        } else {
            set parents {}
        }
        git_commit_create commit_id \
            $pRepo \
            HEAD \
            $sig \
            $sig \
            "UTF-8" \
            $pMessage \
            $pTree \
            [llength $parents] \
            $parents

    } finally {
        if {[info exists pMessage]} {
            ::cffi::memory free $pMessage
        }
        if {[info exists pIndex]} {
            git_index_free $pIndex
        }
        if {[info exists pSig]} {
            git_signature_free $pSig
        }
        if {[info exists pTree]} {
            git_tree_free $pTree
        }
        if {[info exists pParent]} {
            git_object_free $pParent
        }
        if {[info exists pRef]} {
            git_reference_free $pRef
        }
        git_repository_free $pRepo
    }

}
source [file join [file dirname [info script]] porcelain-utils.tcl]
catch {main} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    exit 1
}
