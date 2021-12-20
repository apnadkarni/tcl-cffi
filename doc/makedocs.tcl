# (c) 2021 Ashok P. Nadkarni
# See LICENSE for license terms.

package require ruff

set NS cffi

source cffi.ruff
source start.ruff
source concepts.ruff
source wrapper.ruff
source dyncall.ruff
source memory.ruff
source struct.ruff
source type.ruff
source alias.ruff
source enum.ruff
source pointer.ruff
source prototype.ruff

ruff::document [list Concepts $NS ${NS}::dyncall] \
    -outfile $NS.html \
    -outdir [file join [file dirname [info script]] html] \
    -sortnamespaces false \
    -preamble $start_page \
    -pagesplit namespace \
    -autopunctuate true \
    -hidenamespace $NS \
    -product cffi \
    -version 1.0b1 \
    -copyright "Ashok P. Nadkarni" {*}$::argv
