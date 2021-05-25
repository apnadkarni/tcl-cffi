# (c) 2021 Ashok P. Nadkarni
# See LICENSE for license terms.
#
# Contains common definitions across test scripts

package require tcltest
eval tcltest::configure $argv

set NS cffi
if {$tcl_platform(platform) eq "windows"} {
    set BINDIR ../win/Release_AMD64_VC1916
} else {
    set BINDIR ../build
}
lappend auto_path $BINDIR
package require cffi

namespace eval cffi::test {
    namespace import ::tcltest::test
    variable testDllPath [file normalize $::cffi::dll_path]
    cffi::dyncall::Library create testDll $testDllPath
    testDll function getTestStructSize int {}
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

    variable paramAttrs {in out inout byref}
    variable pointerAttrs {unsafe dispose counted}
    variable errorAttrs {zero nonzero nonnegative positive errno}
    if {$::tcl_platform(platform) eq "windows"} {
        lappend errorAttrs winerror lasterror
    }
    variable typeAttrs [concat $paramAttrs $pointerAttrs $errorAttrs]

    # Error messages
    variable errorMessages
    array set errorMessages {
        unsupportedarraytype {The specified type is invalid or unsupported for array declarations.}
        typeparsemode {The specified type is not valid for the type declaration context.}
        attrparsemode {An attribute in the type declaration is not valid for the declaration context.}
        attrtype {An attribute in the type declaration is not valid for the data type.}
        attrconflict {Type declaration has repeated or conflicting attributes.}
        arraysize {Invalid array size or extra trailing characters.}
        scalarchars {Declarations of type chars, unichars and bytes must be arrays.}
        structbyref {Parameters of type struct must have the byref attribute.}
        arrayreturn {Function return type declaration must not specify an array.}
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

