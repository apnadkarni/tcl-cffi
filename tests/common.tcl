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
    cffi::Wrapper create testDll $testDllPath
    testDll function getTestStructSize int {}
    if {[cffi::pkgconfig get backend] eq "dyncall"} {
        cffi::dyncall::Symbols create testSyms $testDllPath
        tcltest::testConstraint dyncall 1
    } else {
        tcltest::testConstraint libffi 1
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
    variable intMin
    array set intMax {
        schar 127
        uchar 255
        short 32767
        ushort 65535
        int 2147483647
        uint 4294967295
        longlong   9223372036854775807
        ulonglong 18446744073709551615
    }
    array set intMin {
        schar -128
        uchar 0
        short -32768
        ushort 0
        int -2147483648
        uint 0
        longlong -9223372036854775808
        ulonglong 0
    }
    if {$::tcl_platform(wordSize) == 8} {
        set intMax(long)   $intMax(longlong)
        set intMax(ulong) $intMax(ulonglong)
        set intMin(long)   $intMin(longlong)
        set intMin(ulong) $intMin(ulonglong)
    } else {
        set intMax(long)   $intMax(int)
        set intMax(ulong) $intMax(uint)
        set intMin(long)   $intMin(int)
        set intMin(ulong) $intMin(uint)
    }

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

    # Test strings to be used for various encodings
    set testStrings(ascii) "abc"
    set testStrings(jis0208) \u543e
    set testStrings(unicode) \u00e0\u00e1\u00e2
    set testStrings(bytes) \x01\x02\x03

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

    cffi::Struct create ::StructWithStrings {
        s string
        utf8 string.utf-8
        jis string.jis0208
        uni unistring
    }
    proc makeStructWithStrings {} {
        variable testStrings
        return [list s $testStrings(ascii) utf8 $testStrings(unicode) jis $testStrings(jis0208) uni $testStrings(unicode)]
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

proc cffi::test::command_exists {cmd} {
    return [expr {[uplevel #0 namespace which $cmd] eq $cmd}]
}

proc nsqualify {name {ns {}}} {
    if {[string equal -length 2 :: $name]} {
        return $name
    }
    if {$ns eq {}} {
        set ns [uplevel 1 namespace current]
    }
    set ns [string trimright $ns :]
    return ${ns}::$name
}


proc cffi::test::makeptr {p {tag {}}} {
    set width [expr {$::tcl_platform(pointerSize) * 2}]
    return [format "0x%.${width}lx" $p]^$tag
}

proc cffi::test::scoped_ptr {p tag} {
    return [makeptr $p [nsqualify $tag [uplevel 1 namespace current]]]
}

proc cffi::test::lprefix {prefix args} {
    lmap e $args {
        return -level 0 $prefix$e
    }
}

proc cffi::test::lprefixns {args} {
    set ns [uplevel 1 namespace current]
    return [lmap e $args {
        nsqualify $e $ns
    }]
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

# Resets enums in ::, cffi::test and any other passed namespaces
proc cffi::test::reset_enums {args} {
    foreach ns [list :: [namespace current] {*}$args] {
        namespace eval $ns {cffi::enum delete *}
    }
}
# Resets aliases in ::, cffi::test and any other passed namespaces
proc cffi::test::reset_aliases {args} {
    foreach ns [list :: [namespace current] {*}$args] {
        namespace eval $ns {cffi::alias delete *}
    }
}
