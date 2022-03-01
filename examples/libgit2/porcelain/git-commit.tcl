# Demo of cffi libgit extension. Poor man's git commit emulation from libgit2
# Translated to Tcl from libgit2/examples/commit.c

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

proc parse_commit_options {arguments} {
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

proc git-commit {} {
    parse_commit_options $::argv

    set pRepo [git_repository_open_ext [option GitDir .]]
    try {
        try {
            set pParent [git_revparse_ext pRef $pRepo HEAD]
        } trap {GIT -3} {} {
            inform "HEAD not found. Creating first commit."
        }

        set pIndex   [git_repository_index $pRepo]
        set tree_oid [git_index_write_tree $pIndex]
        git_index_write $pIndex
        set pTree    [git_tree_lookup $pRepo $tree_oid]
        set pSig     [git_signature_default $pRepo]

        set sig [git_signature fromnative $pSig]

        # The message is in encoded form with encoding used passed as an argument
        set pMessage [::cffi::memory fromstring [option Message "Commit"] utf-8]

        # Commit parents are passed as a list of pointers
        if {[info exists pParent]} {
            set parents [list $pParent]
        } else {
            set parents {}
        }
        set commit_id [git_commit_create \
            $pRepo \
            HEAD \
            $sig \
            $sig \
            "UTF-8" \
            $pMessage \
            $pTree \
            [llength $parents] \
                           $parents]

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
catch {git-commit} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    exit 1
}
