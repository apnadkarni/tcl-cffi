# Demo of cffi libgit extension. Poor man's git clone emulation from libgit2
# Translated to Tcl from libgit2/examples/clone.c

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

proc parse_clone_options {arguments} {

    # NOTE: getopt uses comments below to generate help. Careful about changing them.
    getopt::getopt opt arg $arguments {
        arglist {
            # URL [DIRECTORY]
            set remain $arg
        }
    }

    if {[llength $remain] == 0 || [llength $remain] > 2} {
        getopt::usage "Wrong number of arguments"
    }
    return $remain
}


proc print_progress {stats progress_state} {
    # stats is a dictionary updated by the various callbacks

    # explode $stats -> completed_steps, total_steps, path, fetch_progress
    dict with stats {}

    # explode $fetch_progress -> total_objects indexed_objects received_objects
    #                   local_objects total_deltas indexed_deltas received_bytes
    dict with fetch_progress {}

    if {$total_objects > 0} {
        set network_percent [expr {(100* $received_objects) / $total_objects}]
        set index_percent [expr {(100 *$indexed_objects) / $total_objects}]
    } else {
        set network_percent 0
        set index_percent 0
    }
    set checkout_percent [expr {$total_steps == 0 ? 0 : (100*$completed_steps)/$total_steps}]
    set kbytes [expr {$received_bytes / 1024}]

    # The whole dance with print_state is because we want to overwrite lines
    # if last print state was the same but go to a new line if the last print
    # state was different. This is to accomodate callbacks of different types
    # coming in intermixed.
    if {$progress_state eq "checkout"} {
        if {[dict get $stats print_state] eq "printing_netstats"} {
            puts ""
        }
        puts -nonewline "Resolving deltas $indexed_deltas/$total_deltas\r"
        return printing_deltas
    } else {
        if {[dict get $stats print_state] eq "printing_deltas"} {
            puts ""
        }
        puts -nonewline [format "Net: %3d%% (%4lu kb, %5u/%5u)  /  idx %3d%% (%5u/%5u)  /  chk %3d%% (%4lu/%4lu)%s\r" \
                  $network_percent $kbytes $received_objects $total_objects \
                  $index_percent $indexed_objects $total_objects \
                  $checkout_percent $completed_steps $total_steps $path]
        return printing_netstats
    }
}

proc checkout_progress {path sofar total payload} {
    # Must match git_checkout_progress_cb prototype
    upvar 1 progress_stats stats
    dict set stats completed_steps $sofar
    dict set stats total_steps $total
    dict set stats print_state [print_progress $stats checkout]
    # void callback so no need for specific return value
}

proc sideband_progress {str len payload} {
    # Must match git_transport_message_cb prototype
    # str is modeled as a pointer since docs are not clear on this

    upvar 1 progress_stats stats

    if {[dict get $stats print_state] ni {{} printing_sideband}} {
        puts ""
    }
    dict set stats print_state printing_sideband

    # Cannot use tostring! because docs do not say nul-terminated
    set bin [::cffi::memory tobinary! $str $len]

    # We want to overwrite current line so terminate with \r.
    # When the next phase begins, the remote end will send the newline itself
    puts -nonewline "remote: [encoding convertfrom utf-8 $bin]\r"
    return 0
}

proc fetch_progress {indexer_stats payload} {
    upvar 1 progress_stats stats
    dict set stats fetch_progress $indexer_stats
    dict set stats print_state [print_progress $stats fetch]
    return 0
}

proc git-clone {arguments} {

    lassign [parse_clone_options $arguments] url path
    if {$path eq ""} {
        set path [file tail $url]
        if {[file extension $path] eq ".git"} {
            set path [file rootname $path]
        }
    }
    puts "Cloning into '$path' ..."

    set clone_opts [git_clone_options_init]
    dict set clone_opts checkout_opts checkout_strategy GIT_CHECKOUT_SAFE

    set translation [fconfigure stdout -translation]
    try {
        fconfigure stdout -translation lf

        # Set up statistics that will be accessed by callbacks via upvar
        set progress_stats {
            completed_steps 0
            total_steps 0
            path ""
            fetch_progress {}
            print_state {}
        }
        set callback_context {
            tried_credential_types {}
        }

        # Set up various progress callbacks
        set checkout_cb \
            [::cffi::callback ${::GIT_NS}::git_checkout_progress_cb \
                 checkout_progress]
        dict set clone_opts checkout_opts progress_cb $checkout_cb

        set sideband_cb \
            [::cffi::callback ${::GIT_NS}::git_transport_message_cb \
                 sideband_progress -1]
        dict set clone_opts fetch_opts callbacks sideband_progress $sideband_cb

        set transfer_cb \
            [::cffi::callback ${::GIT_NS}::git_indexer_progress_cb \
                 fetch_progress -1]
        dict set clone_opts fetch_opts callbacks transfer_progress $transfer_cb

        set credentials_cb \
            [::cffi::callback ${::GIT_NS}::git_credential_acquire_cb \
                 cred_acquire_cb -1]
        dict set clone_opts fetch_opts callbacks credentials $credentials_cb

        # Then clone the repository
        set pRepo [git_clone $url $path $clone_opts]
        puts ""
        git_repository_free $pRepo

    } finally {
        foreach cb {checkout_cb sideband_cb transfer_cb credentials_cb} {
            if {[info exists $cb]} {
                ::cffi::callback_free [set $cb]
            }
        }
        fconfigure stdout -translation $translation
    }


}


source [file join [file dirname [info script]] porcelain-utils.tcl]
catch {git-clone $::argv} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    puts stderr [dict get $edict -errorinfo]
    exit 1
}
