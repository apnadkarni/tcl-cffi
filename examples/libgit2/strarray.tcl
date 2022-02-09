# Based on libgit2/include/strarray.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

# Handling of git_strarray is a little tricky because the contained pointers
# point to memory that may be owned either by libgit2 or the application. Thus
# care has to be taken when calling git_strarray_dispose, git_strarray_copy etc.
# as these should not be called if the memory is owned by the application.
# To protect against this, we define a parallel struct cffi_strarray. Libgit2
# functions that take a const git_strarray (i.e. only as input) are defined
# using pointers to cffi_strarray where the strings passed in are allocated
# by the application (us). Functions that use git_strarray as output are
# defined as taking pointer to git_strarray. This distinction offers some level
# of protection against errors.
#
# Correspondingly, we define two functions - cffi_strarray_new to allocate and
# initialize a cffi_strarray and a second function cffi_strarray_free which will
# accept a pointer to either git_strarray or cffi_strarray and do the appropriate
# calls for releasing memory.
#
# Note this potential confusion with respect strarray memory management has nothing
# to do with cffi bindings. It is also present at the C level libgit2 API. We
# just try to mitigate it at the script level.

# Note pStrings is unsafe pointer as libgit2 allocates under the covers
::cffi::Struct create git_strarray {
    pStrings {pointer unsafe nullok}
    count   size_t
} -clear

# Note pStrings is safe pointer as we will allocate
::cffi::Struct create cffi_strarray {
    pStrings {pointer nullok}
    count size_t
} -clear

libgit2 functions {
    git_strarray_dispose void {
        array PSTRARRAY
    }
    git_strarray_copy GIT_ERROR_CODE {
        pTarget PSTRARRAY
        pSource PSTRARRAY
    }
}

# Allocate a cffi_strarray initialized with a list of strings
proc cffi_strarray_new {strings} {
    set count [llength $strings]
    if {$count} {
        # pStrings is a pointer to an allocated array of pointers to allocated strings
        set pStrings [::cffi::memory new pointer\[$count\] [lmap s $strings {
            ::cffi::memory fromstring $s utf-8
        }]]
    } else {
        set pStrings NULL
    }
    set pStrArray [cffi_strarray new [list pStrings $pStrings count $count]]
    return $pStrArray
}

# Free memory for a git_strarray OR a cffi_strarray
proc cffi_strarray_free {pStrArray} {
    set tag [::cffi::pointer tag $pStrArray]
    if {$tag eq [cffi_strarray name]} {
        # We own the whole boondoggle
        # Free all strings
        # Free array holding pointers to strings
        # Free the structure
        set strArray [cffi_strarray fromnative $pStrArray]
        set count [dict get $strArray count]
        set pStrings [dict get $strArray pStrings]
        if {![::cffi::pointer isnull $pStrings]} {
            for {set i 0} {$i < $count} {incr i} {
                ::cffi::memory free [::cffi::memory get $pStrings pointer $i]
            }
            ::cffi::memory free $pStrings
        }
        cffi_strarray free $pStrArray
    } elseif {$tag eq [git_strarray name]} {
        # All content owned by libgit2 except for the struct space itself
        git_strarray_dispose $pStrArray
        git_strarray free $pStrArray
    } else {
        error "Invalid pointer tag $tag."
    }
}

# Get the strings from a {git,cffi}_strarray
proc cffi_strarray_strings {pStrArray} {
    set o [::cffi::pointer tag $pStrArray]
    set count [$o get $pStrArray count]
    set pStrings [$o get $pStrArray pStrings]

    if {$count == 0} {
        return {}
    }
    set ptrs [::cffi::memory get $pStrings pointer\[$count\]]
    set strings {}
    return [lmap ptr $ptrs {
        ::cffi::memory tostring! $ptr utf-8
    }]
}
