# (c) 2021 Ashok P. Nadkarni
# See LICENSE for license terms.

package require ruff

# Extract version from configure
proc get_version {ac_path} {
    set fd [open $ac_path]
    regexp {AC_INIT\(\[cffi\],\[(\d+\.\d+[.ab]\d+)\]\)} [read $fd] -> ver
    close $fd
    return $ver
}

set NS cffi
set version [get_version [file join [file dirname [info script]] .. configure.ac]]

source cffi.ruff
source start.ruff
source concepts.ruff
source wrapper.ruff
# source dyncall.ruff
source memory.ruff
source struct.ruff
source type.ruff
source alias.ruff
source enum.ruff
source pointer.ruff
source prototype.ruff
source callback.ruff
source cookbook.ruff

set title "Tcl CFFI package"

set common [list \
                -title $title \
                -sortnamespaces false \
                -preamble $start_page \
                -pagesplit namespace \
                -autopunctuate true \
                -hidenamespace $NS \
                -product cffi \
                -diagrammer "ditaa --border-width 1" \
                -version $version \
                -copyright "Ashok P. Nadkarni" {*}$::argv
               ]

if {[llength $argv] == 0 || "html" in $argv} {
    ruff::document [list Concepts Cookbook $NS] \
        -format html \
        -outfile $NS.html \
        -outdir [file join [file dirname [info script]] html] \
        {*}$common
}

if {[llength $argv] == 0 || "nroff" in $argv} {
    ruff::document [list Concepts Cookbook $NS] \
        -format nroff \
        -outfile $NS.3tcl \
        -outdir [file join [file dirname [info script]] man man3] \
        {*}$common
}
