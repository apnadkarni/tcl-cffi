# Demo of cffi libgit extension. Poor man's git fetch emulation from libgit2
# Translated to Tcl from libgit2/examples/fetch.c

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

proc parse_fetch_options {arguments} {
    parse_options opt arg $arguments {
        arglist {
            # REMOTE
            set rest $arg
        }
    }
    if {[llength $rest] == 1} {
        return [lindex $rest 0]
    } else {
        getopt::usage "Wrong number of arguments."
    }
}

proc update_tips_progress {refname oid_old oid_new payload} {
    if {[catch {
        set str_new [git_oid_tostr_s $oid_new]
        if {[git_oid_is_zero $oid_old]} {
            puts "\[new\]     $str_new $refname"
        } else {
            set str_old [git_oid_tostr_s $oid_old]
            puts "\[updated\] [string range $str_old 0 9]..[string range $str_new 0 9] $refname"
        }
    } result]} {
        puts $result
        return -1
    }
    return 0
}

proc transfer_progress {stats payload} {
    if {[catch {
        dict with stats {
            if {$received_objects == $total_objects} {
                puts -nonewline "Resolving deltas $indexed_deltas/$total_deltas\r"
            } elseif {$total_objects > 0} {
                puts -nonewline "Received $received_objects/$total_objects ($indexed_objects) in $received_bytes\r"
            }
        }
    } result]} {
        puts $result
        return -1
    }
    return 0
}

proc sideband_progress {str len payload} {
    if {[catch {
        # Cannot use tostring! because docs do not say nul-terminated
        set bin [::cffi::memory tobinary! $str $len]

        # We want to overwrite current line so terminate with \r.
        # When the next phase begins, the remote end will send the newline itself
        puts -nonewline "remote: [encoding convertfrom utf-8 $bin]\r"
    } result]} {
        puts stderr $result
        return -1
    }
    return 0
}

proc git-fetch {arguments} {
    set remote [parse_fetch_options $arguments]

    set fetch_opts [git_fetch_options_init]
    set translation [fconfigure stdout -translation]
    set pRepo [open_repository]
    set pRemote NULL
    try {
        fconfigure stdout -translation lf

        # Try as a remote name first, then a URL
        if {[catch {
            set pRemote [git_remote_lookup $pRepo $remote]
        }]} {
            set pRemote [git_remote_create_anonymous $pRepo $remote]
        }

        # Define the callbacks to show progress
        set update_tips_cb \
            [::cffi::callback new ${::GIT_NS}::git_remote_update_tips_cb \
             update_tips_progress -1]
        dict set fetch_opts callbacks update_tips $update_tips_cb

        set sideband_cb \
            [::cffi::callback new ${::GIT_NS}::git_transport_message_cb \
                 sideband_progress -1]
        dict set fetch_opts callbacks sideband_progress $sideband_cb

        set transfer_cb \
            [::cffi::callback new ${::GIT_NS}::git_indexer_progress_cb \
                 transfer_progress -1]
        dict set fetch_opts callbacks transfer_progress $transfer_cb

        set credentials_cb \
            [::cffi::callback new ${::GIT_NS}::git_credential_acquire_cb \
                 cred_acquire_cb -1]
        dict set fetch_opts callbacks credentials $credentials_cb

        # Do the fetch with the refspecs configured in the config
        git_remote_fetch $pRemote NULL $fetch_opts "fetch"

        # Print final stats
        set stats [git_remote_stats $pRemote]
        dict with stats {}; # stats -> local_objects etc.
        if {$local_objects > 0} {
            puts "\rReceived $indexed_objects/$total_objects in $received_bytes bytes (used $local_objects local objects)"
        } else {
            puts "\rReceived $indexed_objects/$total_objects in $received_bytes bytes"
        }

    } finally {
        git_remote_free $pRemote
        git_repository_free $pRepo
        fconfigure stdout -translation $translation
    }
}

source [file join [file dirname [info script]] porcelain-utils.tcl]
catch {git-fetch $::argv} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    exit 1
}
