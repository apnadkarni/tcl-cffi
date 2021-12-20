# (c) 2021 Ashok P. Nadkarni
# See LICENSE for license terms.
#
# Contains common definitions across test scripts

package require tcltest
eval tcltest::configure $argv

set NS cffi
if {[catch {package require $NS}]} {
    if {$tcl_platform(platform) eq "windows"} {
        if {$tcl_platform(pointerSize) == 8} {
            set BINDIR ../win/Release_AMD64_VC1916
        } else {
            set BINDIR ../win/Release_VC1916
        }
    } else {
        set BINDIR ../build
    }
    lappend auto_path $BINDIR
    package require $NS
}

namespace eval cffi::test {
    namespace import ::tcltest::test
    variable testDllPath [file normalize [file join [file dirname $::cffi::dll_path] cffitest[info sharedlibextension]]]
    cffi::dyncall::Library create testDll $testDllPath
    testDll function getTestStructSize int {}
    if {[cffi::pkgconfig get backend] eq "dyncall"} {
        cffi::dyncall::Symbols create testSyms $testDllPath
        tcltest::testConstraint dyncall 1
    }
    variable testStructSize
    set testStructSize [getTestStructSize]

    namespace path [namespace parent]

    variable voidTypes {void};  # Currently only one but that might change
    variable signedIntTypes {schar short int long longlong}
    variable unsignedIntTypes {uchar ushort uint ulong ulonglong}
    variable realTypes {float double}
    variable intTypes [concat $signedIntTypes $unsignedIntTypes]
    variable numericTypes [concat $intTypes $realTypes]
    variable stringTypes {string unistring binary}
    variable charArrayTypes {chars unichars bytes}
    variable pointerTypes {pointer}

    variable baseTypes [concat $voidTypes $numericTypes $pointerTypes $stringTypes $charArrayTypes]

    variable intMax
    array set intMax {
        schar 0x7f uchar 0xff short 0x7fff ushort 0xffff
        int 0x7fffffff
        uint 0xffffffff
    }
    if {$::tcl_platform(wordSize) == 8} {
        set intMax(long) 0x7fffffffffffffff
        set intMax(ulong) 0xffffffffffffffff
    } else {
        set intMax(long) 0x7fffffff
        set intMax(ulong) 0xffffffff
    }
    set intMax(longlong) 0x7fffffffffffffff
    set intMax(ulonglong) 0xffffffffffffffff

    variable paramAttrs {in out inout byref}
    variable pointerAttrs {nullok unsafe dispose counted}
    variable stringAttrs {nullok nullifempty}
    variable requirementAttrs {zero nonzero nonnegative positive}
    variable errorHandlerAttrs {errno}
    if {$::tcl_platform(platform) eq "windows"} {
        lappend errorAttrs winerror lasterror
        lappend errorHandlerAttrs winerror lasterror
    }
    variable errorAttrs [concat $requirementAttrs $errorHandlerAttrs]
    variable typeAttrs [concat $paramAttrs $pointerAttrs $errorAttrs]

    # Error messages
    variable errorMessages
    array set errorMessages {
        unsupportedarraytype {The specified type is invalid or unsupported for array declarations.}
        typeparsemode {The specified type is not valid for the type declaration context.}
        attrparsemode {A type annotation is not valid for the declaration context.}
        attrtype {A type annotation is not valid for the data type.}
        attrconflict {Unknown, repeated or conflicting type annotations specified.}
        arraysize {Invalid array size or extra trailing characters.}
        scalarchars {Declarations of type chars, unichars and bytes must be arrays.}
        arrayreturn {Function return type must not be an array.}
        namesyntax {Invalid name syntax.}
        attrparamdir {One or more annotations are invalid for the parameter direction.}
        defaultdisallowed {Defaults are not allowed in this declaration context.}
        structbyref {Passing of structs by value is not supported. Annotate with "byref" to pass by reference if function expects a pointer.}
        store {Annotations "storeonerror" and "storealways" not allowed for "in" parameters.}
        fieldvararray {Fields cannot be arrays of variable size.}
    }

    cffi::Struct create ::cffi::test::InnerTestStruct {
        c chars[15]
    }
    cffi::Struct create ::TestStruct {
        c  schar
        i  int
        shrt short
        ui uint
        ushrt ushort
        l  long
        uc uchar
        ul ulong
        c3 chars[13]
        ll longlong
        unic unichars[13]
        ull ulonglong
        b   bytes[3]
        f float
        s struct.cffi::test::InnerTestStruct
        d double
    }
    proc checkTestStruct {sdict} {
        # Returns list of mismatched fields assuming as initialzed
        # by getTestStruct in tclCffiTest.c
        # Each field is set in C to its offset
        set sinfo [::TestStruct info]
        set mismatches {}
        foreach fld {c i shrt ui ushrt l uc ul c3 ll unic ull f d} {
            if {[expr {[dict get $sdict $fld] != [dict get $sinfo fields $fld offset]}]} {
                puts "$fld: [dict get $sdict $fld] != [dict get $sinfo fields $fld offset]"
                lappend mismatches $fld
            }
        }
        binary scan [dict get $sdict b] cucucu byte0 byte1 byte2
        set off [dict get $sinfo fields b offset]
        if {$byte0 != $off || $byte1 != [incr off] || $byte2 != [incr off]} {
            lappend mismatches b
        }
        if {[dict get [dict get $sdict s] c] != [dict get $sinfo fields s offset]} {
            lappend mismatches s
        }
        return $mismatches
    }
}

proc cffi::test::makeptr {p {tag {}}} {
    set width [expr {$::tcl_platform(pointerSize) * 2}]
    return [format "0x%.${width}lx" $p]^$tag
}

# step better be > 0
proc seq {start count {step 1}} {
    set seq {}
    for {set i 0} {$i < $count} {incr i} {
        lappend seq $start
        incr start $step
    }
    return $seq
}

# Verify two dicts are equal
proc cffi::test::dict_equal {l1 l2} {
    if {[dict size $l1] != [dict size $l2]} {
        return 0
    }
    dict for {k val} $l1 {
        if {![dict exists $l2 $k] || $val != [dict get $l2 $k]} {
            return 0
        }
        dict unset l2 $k
    }
    return [expr {[dict size $l2] == 0}]
}
tcltest::customMatch dict ::cffi::test::dict_equal

proc cffi::test::testsubcmd {cmd} {
    test [namespace tail $cmd]-missing-subcmd-0 "Missing subcommand" -body {
        $cmd
    } -result "wrong # args: should be \"$cmd subcommand ?arg ...?\"" -returnCodes error
}

proc cffi::test::testnumargs {label cmd {fixed {}} {optional {}} args} {
    set minargs [llength $fixed]
    set maxargs [expr {$minargs + [llength $optional]}]
    if {[lindex $optional end] eq "..."} {
        unset maxargs
    }
    set message "wrong # args: should be \"$cmd"
    if {[llength $fixed]} {
        append message " $fixed"
    }
    if {[llength $optional]} {
        append message " $optional"
    }
    if {[llength $fixed] == 0 && [llength $optional] == 0} {
        append message " \""
    } else {
        append message "\""
    }
    if {$minargs > 0} {
        set arguments [lrepeat [expr {$minargs-1}] x]
        test $label-minargs-0 "$label no arguments" \
            -body "$cmd" \
            -result $message -returnCodes error \
            {*}$args
        if {$minargs > 1} {
            test $label-minargs-1 "$label missing arguments" \
                -body "$cmd $arguments" \
                -result $message -returnCodes error \
            {*}$args
        }
    }
    if {[info exists maxargs]} {
        set arguments [lrepeat [expr {$maxargs+1}] x]
        test $label-maxargs-0 "$label extra arguments" \
            -body "$cmd $arguments" \
            -result $message -returnCodes error \
            {*}$args
    }
}

proc cffi::test::purge_pointers {} {
    while {[llength [cffi::pointer list]]} {
        foreach ptr [cffi::pointer list] {
            cffi::pointer dispose $ptr
        }
    }
}
proc cffi::test::make_counted_pointers args {
    lmap addr $args {
        set p [makeptr $addr]
        cffi::pointer counted $p
        set p
    }
}
proc cffi::test::make_safe_pointers args {
    lmap addr $args {
        set p [makeptr $addr]
        cffi::pointer safe $p
        set p
    }
}
proc cffi::test::make_unsafe_pointers args {
    lmap addr $args {
        makeptr $addr
    }
}
