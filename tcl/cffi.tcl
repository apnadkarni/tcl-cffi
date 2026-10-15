# Script component for cffi extension
#
# Copyright (c) Ashok P. Nadkarni
# All rights reserved.
#
# See file LICENSE for licensing information.

namespace eval cffi {}

proc cffi::help {args} {
    set nargs [llength $args]
    set types [list alias enum function interface struct union]
    if {$nargs == 2} {
        lassign $args type name
        set types [list $type]
    } elseif {$nargs == 1} {
        lassign $args name
        if {$name in $types} {
            error "wrong # args: should be \"cffi::help $name NAME\""
        }
    } else {
        lassign $args name
        if {$name in $types} {
            error "wrong # args: should be \"cffi::help $name NAME\""
        } else {
            error "wrong # args: should \"cffi::help ?TYPE? NAME\""
        }
    }
    set result {}
    foreach type $types {
        set text ""
        switch $type {
            alias {
                catch {
                    set info [uplevel 1 [list cffi::rtti alias $name]]
                    set name [dict get $info Name]
                    lappend text "Alias: $name"
                    lappend text "  Body: [dict get $info Alias]"
                    lappend result [join $text \n]
                }
            }
            enum {
                catch {
                    set info [uplevel 1 [list cffi::rtti enum $name]]
                    set name [dict get $info Name]
                    lappend text "Enum: $name"
                    lappend text "Members:"
                    foreach {enum_name enum_value} [dict get $info Members] {
                        lappend text [string cat "    " $enum_name " = " $enum_value]
                    }
                    lappend result [join $text \n]
                }
            }
            function {
                catch {
                    set info [uplevel 1 [list cffi::rtti function $name]]
                    set name [dict get $info Name]
                    set param_names [lmap param [dict get $info Params] {
                        dict get $param Name
                    }]
                    set sep " "
                    if {[llength $param_names] == 0} {
                        set sep ""
                    }
                    if {[dict exists $info Return]} {
                        lappend text "Syntax: $name [join $param_names { }]$sep-> [dict get $info Return]"
                    } else {
                        lappend text "Syntax: $name$sep[join $param_names { }]"
                    }
                    foreach param [dict get $info Params] {
                        lappend text "  [dict get $param Name]: [dict get $param Type]"
                    }
                    if {[dict exists $info Doc]} {
                        lappend text "[dict get $info Doc]"
                    }
                    lappend result [join $text \n]
                }
            }
            interface {
                catch {
                    set info [uplevel 1 [list cffi::rtti interface $name]]
                    set name [dict get $info Name]
                    lappend text "Interface: $name"
                    if {[dict exists $info Super]} {
                        lappend text "  Superclass: [dict get $info Super]"
                    }
                    lappend text "  Methods:"
                    foreach ifc_methods [dict get $info Methods] {
                        lassign $ifc_methods ifc_name methods
                        set method_text [lsort -dictionary [lmap method $methods {
                            dict get $method Name
                        }]]
                        lappend text [string cat {    } [join $method_text { }] " " ($ifc_name)]
                    }
                    if {[dict exists $info Doc]} {
                        lappend text "[dict get $info Doc]"
                    }
                    lappend result [join $text \n]
                }
            }
            struct -
            union {
                catch {
                    set info [uplevel 1 [list cffi::rtti $type $name]]
                    set name [dict get $info Name]
                    lappend text "[string totitle $type]: $name"
                    lappend text "  Fields:"
                    foreach field [dict get $info Fields] {
                        lappend text [string cat "    " [dict get $field Name] " : " [dict get $field Type]]
                    }
                    if {[dict exists $info Doc]} {
                        lappend text "[dict get $info Doc]"
                    }
                    lappend result [join $text \n]
                }
            }
            default {
                error "Unknown rtti type \"$type\""
            }
        }
    }
    if {[llength $result] == 0} {
        error "Program element named \"$name\" not found."
    }
    return [join $result \n\n]
}
