# (c) 2021 Ashok P. Nadkarni
# See LICENSE for license terms.

package require ruff


set NS cffi

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

set title "Tcl CFFI package"

if {[llength $argv] == 0 || "html" in $argv} {
    ruff::document [list Concepts $NS] \
        -format html \
        -outfile $NS.html \
        -outdir [file join [file dirname [info script]] html] \
        -title $title \
        -sortnamespaces false \
        -preamble $start_page \
        -pagesplit namespace \
        -autopunctuate true \
        -hidenamespace $NS \
        -product cffi \
        -version 1.0b3 \
        -copyright "Ashok P. Nadkarni" {*}$::argv
}

if {[llength $argv] == 0 || "nroff" in $argv} {
    ruff::document [list Concepts $NS] \
        -format nroff \
        -outfile $NS.man \
        -outdir [file join [file dirname [info script]] man] \
        -title $title \
        -sortnamespaces false \
        -preamble $start_page \
        -pagesplit namespace \
        -autopunctuate true \
        -hidenamespace $NS \
        -product cffi \
        -version 1.0b3 \
        -copyright "Ashok P. Nadkarni" {*}$::argv
}
