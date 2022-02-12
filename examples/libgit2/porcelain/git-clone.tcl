proc pdict d {
    dict for {k v} $d {
        puts "$k: $v"
    }
}

proc print_progress {stats} {
    pdict $stats
}

proc checkout_progress {path sofar total payload} {
    # Must match git_checkout_progress_cb prototype
    upvar 1 progress_stats stats
    dict set stats completed_steps $sofar
    dict set stats total_steps $total
    print_progress $stats
    # void callback so no need for specific return value
}

proc sideband_progress {str len payload} {
    # Must match git_transport_message_cb prototype
    # str is modeled as a pointer since docs are not clear on this

    # Cannot use tostring! because docs do not say nul-terminated
    set bin [::cffi::memory tobinary! $str $len]
    puts [encoding convertfrom utf-8 $bin]
    return 0
}

proc fetch_progress {indexer_stats payload} {
    upvar 1 progress_stats stats
    puts $indexer_stats
    dict set stats fetch_progress $indexer_stats
    print_progress $stats
    return 0
}

proc main {} {
    if {[llength [lassign $::argv url path]]} {
        error "USAGE: git-clone.tcl URL \[PATH\]"
    }
    if {$path eq ""} {
        set path [file tail $url]
    }
    if {[file exists $path]} {
        error "File or directory $path already exists."
    }

    git_clone_options_init clone_opts
    dict set clone_opts checkout_opts checkout_strategy GIT_CHECKOUT_SAFE

    try {
        # Set up statistics that will be accessed by callbacks via upvar
        set progress_stats {
            completed_steps 0
            total_steps 0
            path ""
            fetch_progress {}
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
        git_clone pRepo $url $path $clone_opts
        git_repository_free $pRepo

    } finally {
        foreach cb {checkout_cb sideband_cb transfer_cb credentials_cb} {
            if {[info exists $cb]} {
                ::cffi::callback_free [set $cb]
            }
        }
    }


}


source [file join [file dirname [info script]] porcelain-utils.tcl]
catch {main} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    puts stderr [dict get $edict -errorinfo]
    exit 1
}
