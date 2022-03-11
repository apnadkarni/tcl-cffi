# (c) 2021 Ashok P. Nadkarni
# See LICENSE for license terms.

package require ruff


set NS cffi
set version 1.0b4

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
                -diagrammer ditaa \
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
        -outfile $NS.man \
        -outdir [file join [file dirname [info script]] man] \
        {*}$common
}
