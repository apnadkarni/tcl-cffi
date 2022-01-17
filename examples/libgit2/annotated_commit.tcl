# Based on libgit2/include/annotated_commit.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

libgit2 functions {
    git_annotated_commit_from_ref GIT_ERROR_CODE {
        pCommit {PANNOTATED_COMMIT out}
        pRepo   PREPOSITORY
        ref     PREFERENCE
    }
    git_annotated_commit_from_fetchhead GIT_ERROR_CODE {
        pCommit     {PANNOTATED_COMMIT out}
        pRepo       PREPOSITORY
        branch_name STRING
        url         STRING
        id          {struct.git_oid    byref}
    }
    git_annotated_commit_lookup GIT_ERROR_CODE {
        pCommit {PANNOTATED_COMMIT out}
        pRepo   PREPOSITORY
        id      {struct.git_oid    byref}
    }
    git_annotated_commit_from_revspec GIT_ERROR_CODE {
        pCommit {PANNOTATED_COMMIT out}
        pRepo   PREPOSITORY
        revspec STRING
    }
    git_annotated_commit_id {struct.git_oid byref} {
        pCommit PANNOTATED_COMMIT
    }
    git_annotated_commit_ref STRING {
        pCommit PANNOTATED_COMMIT
    }
    git_annotated_commit_free void {
        pCommit {PANNOTATED_COMMIT dispose}
    }
}
