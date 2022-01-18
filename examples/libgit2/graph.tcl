# Based on libgit2/include/graph.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

libgit2 functions {
    git_graph_ahead_behind GIT_ERROR_CODE {
        ahead    {size_t         out}
        behind   {size_t         out}
        pRepo    PREPOSITORY
        local    {struct.git_oid byref}
        upstream {struct.git_oid byref}
    }
    git_graph_descendant_of int {
        pRepo    PREPOSITORY
        commit   {struct.git_oid byref}
        ancestor {struct.git_oid byref}
    }
    git_graph_reachable_from_any int {
        pRepo            PREPOSITORY
        commit           {struct.git_oid byref}
        descendant_array {struct.git_oid[length] byref}
        length           size_t
    }
}
