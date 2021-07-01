# Copyright (c) 2021 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

package require cffi

namespace eval libzip {
    variable libzip

    proc InitTypes {} {
        cffi::Struct create zip_error {
            zip_err int
            sys_err int
            str {pointer unsafe}
        }

        cffi::enum define ZipFlags {
            ZIP_FL_ENC_GUESS   0
            ZIP_FL_NOCASE      1
            ZIP_FL_NODIR       2
            ZIP_FL_COMPRESSED  4
            ZIP_FL_UNCHANGED   8
            ZIP_FL_RECOMPRESS  16
            ZIP_FL_ENCRYPTED   32
            ZIP_FL_ENC_RAW     64
            ZIP_FL_ENC_STRICT  128
            ZIP_FL_LOCAL       256
            ZIP_FL_CENTRAL     512
            ZIP_FL_ENC_UTF_8   2048
            ZIP_FL_ENC_CP437   4096
            ZIP_FL_OVERWRITE   8192
        }
        cffi::enum define ZipOpenFlags {
            ZIP_CREATE     1
            ZIP_EXCL       2
            ZIP_CHECKCONS  4
            ZIP_TRUNCATE   8
            ZIP_RDONLY     16
        }

        cffi::alias load C
        cffi::alias define PZIP_T        pointer.zip_t
        cffi::alias define PZIP_SOURCE_T pointer.zip_source_t
        cffi::alias define PZIP_FILE_T   pointer.zip_file_t
        cffi::alias define ZIP_ERROR_REF {struct.zip_error byref}
        cffi::alias define ZIP_FLAGS_T   uint32_t
        cffi::alias define PZIP_FILE_ATTRIBUTES_T pointer.zip_file_attributes_t

        # LIBZIPSTATUS - integer error return that is 0 on success. Error
        # detail is to be retrieved with zip_get_error.
        cffi::alias define LIBZIPSTATUS {int zero {onerror ArchiveErrorHandler}}

    }

    # The error handlers expect the following parameter names in the
    # passed input/output dictionaries.
    #   pzip   - the PZIP_T handle to the archive
    #   psource - a PZIP_SOURCE_T handle
    #   pfile  - a PZIP_FILE_T handle to a file in the archive
    #   zcode  - integer error code
    #   zerr   - zip_error_t struct

    proc ThrowLibzipError {zerr} {
         set zmsg [zip_error_strerror $zerr]
         set zcode [zip_error_code_zip $zerr]
         set scode [zip_error_code_system $zerr]
         throw [list LIBZIP [list ZCODE $zcode SYSERROR $scode] $zmsg] $zmsg
    }

    proc ArchiveErrorHandler {fn_result inparams outparams} {
        set perr [zip_get_error [dict get $inparams pzip]]
        set zerr [zip_error fromnative $perr]
        # This is a static pointer, no freeing to be done. Just unregister
        cffi::pointer dispose $perr
        ThrowLibzipError $zerr
    }
    proc ArchiveOpenHandler {fn_result inparams outparams} {
         if {[dict exists $outparams zcode]} {
            zip_error_init_with_code zerr [dict get $outparams zcode]
            ThrowLibzipError $zerr
         }
         throw [list LIBZIP [list ZCODE -1 SYSERROR -1] "Unknown error"] "Unknown error"
    }
    # SourceErrorHander is needed because on error PZIP_SOURCE_T type
    # arguments need to be freed.
    proc SourceErrorHandler {fn_result inparams outparams} {
        if {[dict exists $inparams psource]} {
            zip_source_free [dict get $inparams psource]
        }
        ArchiveErrorHandler $fn_result $inparams $outparams
    }

    variable Functions
    array set Functions {
        zip_error_init_with_code { void   {zerr {ZIP_ERROR_REF out} zcode int} }
        zip_error_strerror       { string {zerr ZIP_ERROR_REF} }
        zip_error_code_zip       { int    {zerr ZIP_ERROR_REF} }
        zip_error_code_system    { int    {zerr ZIP_ERROR_REF} }
        zip_error_clear          { void   {pzip PZIP_T}}
        zip_get_error            { {pointer.libzip::zip_error} {pzip PZIP_T} }
        zip_strerror             { string {pzip PZIP_T} }
        zip_file_strerror        { string {pfile PZIP_FILE_T} }

        zip_open {
            {PZIP_T nonzero {onerror ::libzip::ArchiveOpenHandler}}
            {path string flags {int {enum ZipOpenFlags}} zcode {int out storeonerror}}
        }
        zip_close  { LIBZIPSTATUS  {pzip {PZIP_T dispose}} }

        zip_dir_add {
            {int64_t nonnegative {onerror libzip::ArchiveErrorHandler}}
            {pzip PZIP_T name string flags {ZIP_FLAGS_T {enum ZipFlags} {default ZIP_FL_ENC_GUESS}}}
        }
        zip_source_file {
            {PZIP_SOURCE_T nonzero {onerror ::libzip::ArchiveErrorHandler}}
            {pzip PZIP_T fname string start {uint64_t {default 0}} len { uint64_t {default 0}}}
        }
        zip_file_add {
            {int64_t nonnegative {onerror ::libzip::ArchiveErrorHandler}}
            {pzip PZIP_T fname string psource {PZIP_SOURCE_T dispose} flags ZIP_FLAGS_T}
        }
        zip_name_locate {int64_t {pzip PZIP_T fname string flags {ZIP_FLAGS_T bitmask {enum ZipFlags} {default 0}}}}
    }


    # zip_delete { LIBZIPSTATUS  {pzip PZIP_T index int64_t} }
    # zip_discard {void {pArchive pzip_t}}
    # zip_error_fini {void {perr pzip_error_t}}
    # zip_error_init {void {perr {struct.zip_error_t out}}}
    # zip_error_set {void {perr pzip_error_t zerr int syserr int}}
    # zip_error_system_type {int {perr pzip_error_t}}
    # zip_error_to_data {{int64_t positive} {# perr pzip_error_t # data {bytes[len] out} # len int64_t # }}
        # zip_fclose {LIBZIPSTATUS {zfile pzip_file_t}}
        # zip_fdopen {{pzip_t nonzero {onerror PArchiveOpenHandler}} {fd int flags int pcode {int out storeonerror}}}
        # zip_fdopen pzip_t {int, int, int *_Nullable}
        # zip_file_attributes_init void {pzip_file_attributes_t _Nonnull}
        # zip_file_error_clear void {zfile pzip_file_t}
        # zip_file_extra_field_delete int {pArchive pzip_t, uint64_t, uint16_t, flags zip_flags_t}
        # zip_file_extra_field_delete_by_id int {pArchive pzip_t, uint64_t, uint16_t, uint16_t, flags zip_flags_t}
        # zip_file_extra_field_set int {pArchive pzip_t, uint64_t, uint16_t, uint16_t, const uint8_t *_Nullable, uint16_t, flags zip_flags_t}
        # zip_file_extra_fields_count zip_int16_t {pArchive pzip_t, uint64_t, flags zip_flags_t}
        # zip_file_extra_fields_count_by_id zip_int16_t {pArchive pzip_t, uint64_t, uint16_t, flags zip_flags_t}
# const uint8_t *_Nullable zip_file_extra_field_get {pArchive pzip_t, uint64_t, uint16_t, uint16_t *_Nullable, uint16_t *_Nullable, flags zip_flags_t}
# const uint8_t *_Nullable zip_file_extra_field_get_by_id {pArchive pzip_t, uint64_t, uint16_t, uint16_t, uint16_t *_Nullable, flags zip_flags_t}
# zip_file_get_comment string {pArchive pzip_t, uint64_t, uint32_t *_Nullable, flags zip_flags_t}
# zip_file_get_error pzip_error_t {zfile pzip_file_t}
# zip_file_get_external_attributes int {pArchive pzip_t, uint64_t, flags zip_flags_t, uint8_t *_Nullable, uint32_t *_Nullable}
# zip_file_rename int {pArchive pzip_t, uint64_t, string _Nonnull, flags zip_flags_t}
# zip_file_replace int {pArchive pzip_t, uint64_t, pzip_source_t _Nonnull, flags zip_flags_t}
# zip_file_set_comment int {pArchive pzip_t, uint64_t, string _Nullable, uint16_t, flags zip_flags_t}
# zip_file_set_dostime int {pArchive pzip_t, uint64_t, uint16_t, uint16_t, flags zip_flags_t}
# zip_file_set_encryption int {pArchive pzip_t, uint64_t, uint16_t, string _Nullable}
# zip_file_set_external_attributes int {pArchive pzip_t, uint64_t, flags zip_flags_t, uint8_t, uint32_t}
# zip_file_set_mtime int {pArchive pzip_t, uint64_t, time_t, flags zip_flags_t}
# zip_fopen pzip_file_t {pArchive pzip_t, string _Nonnull, flags zip_flags_t}
# zip_fopen_encrypted pzip_file_t {pArchive pzip_t, string _Nonnull, flags zip_flags_t, string _Nullable}
# zip_fopen_index pzip_file_t {pArchive pzip_t, uint64_t, flags zip_flags_t}
# zip_fopen_index_encrypted pzip_file_t {pArchive pzip_t, uint64_t, flags zip_flags_t, string _Nullable}
# zip_fread zip_int64_t {zfile pzip_file_t, void *_Nonnull, uint64_t}
# zip_fseek zip_int8_t {zfile pzip_file_t, zip_int64_t, int}
# zip_ftell zip_int64_t {zfile pzip_file_t}
# zip_get_pArchive_comment string {pArchive pzip_t, int *_Nullable, flags zip_flags_t}
# zip_get_pArchive_flag int {pArchive pzip_t, flags zip_flags_t, zip_flags_t}
# zip_get_name string {pArchive pzip_t, uint64_t, flags zip_flags_t}
# zip_get_num_entries zip_int64_t {pArchive pzip_t, flags zip_flags_t}
# zip_libzip_version string {void}
# zip_open pzip_t {string _Nonnull, int, int *_Nullable}
# zip_open_from_source pzip_t {pzip_source_t _Nonnull, int, pzip_error_t _Nullable}
# zip_register_progress_callback_with_state int {pArchive pzip_t, double, zip_progress_callback _Nullable, void  {*_Nullable} {void *_Nullable}, void *_Nullable}
# zip_register_cancel_callback_with_state int {pArchive pzip_t, zip_cancel_callback _Nullable, void  {*_Nullable} {void *_Nullable}, void *_Nullable}
# zip_set_pArchive_comment int {pArchive pzip_t, string _Nullable, uint16_t}
# zip_set_pArchive_flag int {pArchive pzip_t, flags zip_flags_t, int}
# zip_set_default_password int {pArchive pzip_t, string _Nullable}
# zip_set_file_compression int {pArchive pzip_t, uint64_t, zip_int32_t, uint32_t}
# zip_source_begin_write int {pzip_source_t _Nonnull}
# zip_source_begin_write_cloning int {pzip_source_t _Nonnull, uint64_t}
# zip_source_buffer pzip_source_t {pArchive pzip_t, const void *_Nullable, uint64_t, int}
# zip_source_buffer_create pzip_source_t {const void *_Nullable, uint64_t, int, pzip_error_t _Nullable}
# zip_source_buffer_fragment pzip_source_t {pArchive pzip_t, const zip_buffer_fragment_t *_Nonnull, uint64_t, int}
# zip_source_buffer_fragment_create pzip_source_t {const zip_buffer_fragment_t *_Nullable, uint64_t, int, pzip_error_t _Nullable}
# zip_source_close int {pzip_source_t _Nonnull}
# zip_source_commit_write int {pzip_source_t _Nonnull}
# zip_source_error pzip_error_t {pzip_source_t _Nonnull}
# zip_source_file pzip_source_t {pArchive pzip_t, string _Nonnull, uint64_t, zip_int64_t}
# zip_source_file_create pzip_source_t {string _Nonnull, uint64_t, zip_int64_t, pzip_error_t _Nullable}
# zip_source_filep pzip_source_t {pArchive pzip_t, FILE *_Nonnull, uint64_t, zip_int64_t}
# zip_source_filep_create pzip_source_t {FILE *_Nonnull, uint64_t, zip_int64_t, pzip_error_t _Nullable}
# zip_source_free void {pzip_source_t _Nullable}
# zip_source_function pzip_source_t {pArchive pzip_t, zip_source_callback _Nonnull, void *_Nullable}
# zip_source_function_create pzip_source_t {zip_source_callback _Nonnull, void *_Nullable, pzip_error_t _Nullable}
# zip_source_get_file_attributes int {pzip_source_t _Nonnull, pzip_file_attributes_t _Nonnull}
# zip_source_is_deleted int {pzip_source_t _Nonnull}
# zip_source_keep void {pzip_source_t _Nonnull}
# zip_source_make_command_bitmap zip_int64_t {zip_source_cmd_t, ...}
# zip_source_open int {pzip_source_t _Nonnull}
# zip_source_read zip_int64_t {pzip_source_t _Nonnull, void *_Nonnull, uint64_t}
# zip_source_rollback_write void {pzip_source_t _Nonnull}
# zip_source_seek int {pzip_source_t _Nonnull, zip_int64_t, int}
# zip_source_seek_compute_offset zip_int64_t {uint64_t, uint64_t, void *_Nonnull, uint64_t, pzip_error_t _Nullable}
# zip_source_seek_write int {pzip_source_t _Nonnull, zip_int64_t, int}
# zip_source_stat int {pzip_source_t _Nonnull, zip_stat_t *_Nonnull}
# zip_source_tell zip_int64_t {pzip_source_t _Nonnull}
# zip_int64_t zip_source_tell_write {pzip_source_t _Nonnull}
# #ifdef _WIN32
# zip_source_win32a pzip_source_t {pzip_t , string , uint64_t, zip_int64_t}
# zip_source_win32a_create pzip_source_t {string , uint64_t, zip_int64_t, pzip_error_t }
# zip_source_win32handle pzip_source_t {pzip_t , void *, uint64_t, zip_int64_t}
# zip_source_win32handle_create pzip_source_t {void *, uint64_t, zip_int64_t, pzip_error_t }
# zip_source_win32w pzip_source_t {pzip_t , const wchar_t *, uint64_t, zip_int64_t}
# zip_source_win32w_create pzip_source_t {const wchar_t *, uint64_t, zip_int64_t, pzip_error_t }
# #endif
# zip_source_window_create pzip_source_t {pzip_source_t _Nonnull, uint64_t, zip_int64_t, pzip_error_t _Nullable}
# zip_source_write zip_int64_t {pzip_source_t _Nonnull, const void *_Nullable, uint64_t}
# zip_source_zip pzip_source_t {pArchive pzip_t, pArchive pzip_t, uint64_t, flags zip_flags_t, uint64_t, zip_int64_t}
# zip_source_zip_create pzip_source_t {pArchive pzip_t, uint64_t, flags zip_flags_t, uint64_t, zip_int64_t, pzip_error_t _Nullable}
# zip_stat int {pArchive pzip_t, string _Nonnull, flags zip_flags_t, zip_stat_t *_Nonnull}
# zip_stat_index int {pArchive pzip_t, uint64_t, flags zip_flags_t, zip_stat_t *_Nonnull}
# zip_stat_init void {zip_stat_t *_Nonnull}
# zip_strerror string {pArchive pzip_t}
# zip_unchange int {pArchive pzip_t, uint64_t}
# zip_unchange_all int {pArchive pzip_t}
# zip_unchange_pArchive int {pArchive pzip_t}
# zip_compression_method_supported int {zip_int32_t method, int compress}
# zip_encryption_method_supported int {uint16_t method, int encode}
    
    proc InitFunctions {} {
        variable Functions
        foreach {fn_name prototype} [array get Functions] {
            proc $fn_name args "libzip function $fn_name [list {*}$prototype]; tailcall $fn_name {*}\$args"
        }
    }

    proc init [list [list path "libzip[info sharedlibextension]"]] {
        variable Functions

        ::cffi::dyncall::Library create libzip $path
        InitTypes
        InitFunctions

        # Redefine to no-op
        proc init args {}
    }

}
