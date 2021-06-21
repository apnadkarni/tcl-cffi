# (c) 2021 Ashok P. Nadkarni
# See LICENSE for license terms.

package require ruff

set NS cffi

source cffi.ruff
source start.ruff
source concepts.ruff
source dyncall.ruff
source memory.ruff
source struct.ruff
source type.ruff
source alias.ruff
source pointer.ruff
source prototype.ruff

ruff::document [list Concepts $NS ${NS}::dyncall] \
    -output [file join [file dirname [info script]] html $NS.html] \
    -sortnamespaces false \
    -preamble $start_page \
    -pagesplit namespace \
    -autopunctuate true \
    -hidenamespace $NS \
    -title "CFFI Reference"
