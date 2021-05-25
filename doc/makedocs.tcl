# (c) 2021 Ashok P. Nadkarni
# See LICENSE for license terms.

package require ruff

set NS cffi

source start.ruff
source reference.ruff
source dyncall.ruff
source memory.ruff
source struct.ruff
source type.ruff
source alias.ruff
source pointer.ruff

ruff::document [list Concepts $NS ${NS}::dyncall] \
    -output [file join [file dirname [info script]] $NS.html] \
    -sortnamespaces false \
    -preamble $start_page \
    -pagesplit namespace \
    -autopunctuate true \
    -navigation sticky \
    -hidenamespace $NS \
    -title "CFFI Reference"
