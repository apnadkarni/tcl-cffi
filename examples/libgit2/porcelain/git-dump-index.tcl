# Demo of cffi libgit extension. Poor man's git show-index emulation from libgit2
# Translated to Tcl from libgit2/examples/show-index.c
# Note this NOT same as git's show-index hence name change.

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

proc parse_dump-index_options {arguments} {
    parse_options opt arg $arguments {
        -v - --verbose {
            # Verbose mode
            option_set Verbose $arg
        }
        arglist {
            # [INDEXFILE]
            set rest $arg
        }
    }

    if {[llength $rest] > 1} {
        getopt::usage "Too many arguments."
    }

    return [lindex $rest 0]; # May be empty
}

proc git-dump-index {arguments} {
    set index_file [parse_dump-index_options $arguments]
    if {$index_file ne ""} {
        set pIndex [git_index_open $index_file]
    } else {
        set pRepo [open_repository]
        try {
            set pIndex [git_repository_index $pRepo]
        } finally {
            git_repository_free $pRepo
        }
    }

    try {
        git_index_read $pIndex 0
        set count [git_index_entrycount $pIndex]
        for {set i 0} {$i < $count} {incr i} {
            set entry [git_index_get_byindex $pIndex $i]
            dict with entry {
                set oid_str [git_oid_tostr_s $id]
                if {[option Verbose 0]} {
                    puts "File Path: $path"
                    puts "    Stage: [git_index_entry_stage $entry]"
                    puts " Blob SHA: $oid_str"
                    puts "File Mode: [format %07o $mode]"
                    puts "File Size: $file_size bytes"
                    puts "Dev/Inode: $dev/$ino"
                    puts "  UID/GID: $uid/$gid"
                    puts "    ctime: [clock format [dict get $ctime seconds] -gmt 1]"
                    puts "    mtime: [clock format [dict get $mtime seconds] -gmt 1]"
                    puts ""
                } else {
                    puts "$oid_str $path\t$file_size"
                }
            }
        }
    } finally {
        git_index_free $pIndex
    }
}

source [file join [file dirname [info script]] porcelain-utils.tcl]
catch {git-dump-index $::argv} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    puts stderr [dict get $edict -errorinfo]
    exit 1
}
