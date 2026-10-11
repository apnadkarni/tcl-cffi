# Script component for cffi extension
#
# Copyright (c) Ashok P. Nadkarni
# All rights reserved.
#
# See file LICENSE for licensing information.

namespace eval cffi {}

proc cffi::help {args} {
    set nargs [llength $args]
    if {$nargs == 2} {
        lassign $args type name
        set types [list $type]
    } elseif {$nargs == 1} {
        lassign $args name
        set types [list alias enum function interface struct union]
    } else {
        error "wrong # args: should \"help ?TYPE? NAME\""
    }
    set result {}
    foreach type $types {
        set text ""
        switch $type {
            alias {
                catch {
                    set info [rtti alias $name]
                    set name [dict get $info Name]
                    lappend text "Alias: $name"
                    lappend text "Body: [dict get $info Alias]"
                    lappend result [join $text \n]
                }
            }
            enum {
                catch {
                    set info [rtti enum $name]
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
                    set info [rtti function $name]
                    set name [dict get $info Name]
                    set param_names [lmap param [dict get $info Params] {
                        dict get $param Name
                    }]
                    lappend text "Command: $name [join $param_names { }]"
                    if {[dict exists $info Return]} {
                        lappend text "Returns: [dict get $info Return]"
                    }
                    if {[dict exists $info Doc]} {
                        lappend text "[dict get $info Doc]"
                    }
                    lappend result [join $text \n]
                }
            }
            interface {
                catch {
                    set info [rtti interface $name]
                    set name [dict get $info Name]
                    lappend text "Interface: $name"
                    lappend text "Methods:"
                    foreach ifc_methods [dict get $info Methods] {
                        lassign $ifc_methods ifc_name methods
                        set method_text [lsort -dictionary [lmap method $methods {
                            dict get $method Name
                        }]]
                        lappend text [string cat {    } [join $method_text { }] " " ($ifc_name)]
                    }
                    lappend result [join $text \n]
                }
            }
            struct -
            union {
                catch {
                    set info [rtti $type $name]
                    set name [dict get $info Name]
                    lappend text "[string totitle $type]: $name"
                    lappend text "Fields:"
                    foreach field [dict get $info Fields] {
                        lappend text [string cat "    " [dict get $field Name] " : " [dict get $field Type]]
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