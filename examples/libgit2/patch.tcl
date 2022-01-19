# Based on libgit2/include/patch.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::alias define PPATCH pointer.git_patch

libgit2 functions {
    git_patch_owner {PREPOSITORY nullok} {
        patch PPATCH
    }
    git_patch_from_diff GIT_ERROR_CODE {
        patch {PPATCH out}
        diff  PDIFF
        idx   size_t
    }
    git_patch_from_blobs GIT_ERROR_CODE {
        patch       {PPATCH out}
        old_blob    {PBLOB  nullok}
        old_as_path {STRING nullifempty}
        new_blob    {PBLOB  nullok}
        new_as_path {STRING nullifempty}
        opts        {struct.git_diff_options byref nullifempty}
    }
    git_patch_from_blob_and_buffer GIT_ERROR_CODE {
        patch          {PPATCH out}
        old_blob       {PBLOB  nullok}
        old_as_path    {STRING nullifempty}
        buffer         {binary nullifempty}
        buffer_len     size_t
        buffer_as_path {STRING nullifempty}
        opts           {struct.git_diff_options byref nullifempty}
    }
    git_patch_from_buffers GIT_ERROR_CODE {
        patch       {PPATCH out}
        old_buffer  {binary nullifempty}
        old_len     size_t
        old_as_path {STRING nullifempty}
        new_buffer  {binary nullifempty}
        new_len     size_t
        new_as_path {STRING nullifempty}
        opts        {struct.git_diff_options byref nullifempty}
    }
    git_patch_free void {
        patch PPATCH
    }
    git_patch_get_delta {struct.git_diff_delta byref} {
        patch PPATCH
    }
    git_patch_num_hunks size_t {
        patch PPATCH
    }
    git_patch_line_stats GIT_ERROR_CODE {
        total_context   {size_t out}
        total_additions {size_t out}
        total_deletions {size_t out}
        patch           PPATCH
    }
    git_patch_get_hunk GIT_ERROR_CODE {
        hunk          {pointer.git_diff_hunk unsafe out}
        lines_in_hunk {size_t out}
        patch         PPATCH
        hunk_idx      size_t
    }
    git_patch_num_lines_in_hunk GIT_ERROR_CODE {
        patch    PPATCH
        hunk_idx size_t
    }
    git_patch_get_line_in_hunk GIT_ERROR_CODE {
        line         {pointer.git_diff_line out}
        patch        PPATCH
        hunk_idx     size_t
        line_of_hunk size_t
    }
    git_patch_size GIT_ERROR_CODE {
        patch                PPATCH
        include_context      int
        include_hunk_headers int
        include_file_headers int
    }
    git_patch_print GIT_ERROR_CODE {
        patch    PPATCH
        print_cb pointer.git_diff_line_cb
        payload  CB_PAYLOAD
    }
    git_patch_to_buf GIT_ERROR_CODE {
        buf PBUF
        patch PPATCH
    }
}
