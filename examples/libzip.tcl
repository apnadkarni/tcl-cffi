# Copyright (c) 2021 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

# libzip::init {D:\src\vcpkg\installed\x64-windows\bin}

package require cffi

namespace eval libzip {
    variable libzip

    proc InitTypes {} {
        cffi::alias load C
        cffi::alias load posix

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
        cffi::enum define ZipEncodingFlags {
            ZIP_FL_ENC_GUESS   0
            ZIP_FL_ENC_RAW     64
            ZIP_FL_ENC_UTF_8   2048
            ZIP_FL_ENC_CP437   4096
        }
        cffi::enum define ZipOpenFlags {
            ZIP_CREATE     1
            ZIP_EXCL       2
            ZIP_CHECKCONS  4
            ZIP_TRUNCATE   8
            ZIP_RDONLY     16
        }
        cffi::enum define ZipStatFlags {
            ZIP_STAT_NAME              0x0001
            ZIP_STAT_INDEX             0x0002
            ZIP_STAT_SIZE              0x0004
            ZIP_STAT_COMP_SIZE         0x0008
            ZIP_STAT_MTIME             0x0010
            ZIP_STAT_CRC               0x0020
            ZIP_STAT_COMP_METHOD       0x0040
            ZIP_STAT_ENCRYPTION_METHOD 0x0080
            ZIP_STAT_FLAGS             0x0100
        }
        cffi::enum define ZipEncryptionMethod {
            ZIP_EM_NONE 0
            ZIP_EM_TRAD_PKWARE 1
            ZIP_EM_AES_128 0x0101
            ZIP_EM_AES_192 0x0102
            ZIP_EM_AES_256 0x0103
        }
        cffi::enum define ZipCompressionMethod {
            ZIP_CM_DEFAULT -1
            ZIP_CM_STORE 0
            ZIP_CM_BZIP2 12
            ZIP_CM_DEFLATE 8
            ZIP_CM_XZ 95
            ZIP_CM_ZSTD 93
        }
        cffi::Struct create zip_error_t {
            zip_err int
            sys_err int
            str     {pointer unsafe}
        }
        cffi::Struct create zip_stat_t {
            valid        uint64_t
            name         {pointer unsafe}
            index        uint64_t
            size         uint64_t
            comp_size    uint64_t
            mtime        time_t
            crc          uint32_t
            comp_method  uint16_t
            encryption_method  uint16_t
            flags        uint32_t
        }

        cffi::alias define PZIP_T        pointer.zip_t
        cffi::alias define PZIP_SOURCE_T pointer.zip_source_t
        cffi::alias define PZIP_FILE_T   pointer.zip_file_t
        cffi::alias define ZIP_ERROR_REF {struct.zip_error_t byref}
        cffi::alias define ZIP_FLAGS_T   {uint32_t {enum ZipFlags} bitmask {default 0}}
        cffi::alias define PZIP_FILE_ATTRIBUTES_T pointer.zip_file_attributes_t
        cffi::alias define UTF8          string.utf-8
        cffi::alias define ZIP_ENCODING  {
            uint32_t {enum ZipEncodingFlags} {default ZIP_FL_ENC_GUESS}
        }
        cffi::alias define ZIP_ENCRYPTION_METHOD {uint16_t {enum ZipEncryptionFlags} {default ZIP_EM_NONE}}
        cffi::alias define ZIP_COMPRESSION_METHOD {uint32_t {enum ZipCompressionMethod} {default ZIP_CM_DEFAULT}}
        # LIBZIPSTATUS - integer error return that is 0 on success. Error
        # detail is to be retrieved with zip_get_error.
        cffi::alias define LIBZIPSTATUS {int zero {onerror ::libzip::ArchiveErrorHandler}}
        # LIBZIPCODE - error code
        cffi::alias define LIBZIPCODE {int zero {onerror ::libzip::ErrorCodeHandler}}


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
    proc ErrorCodeHandler {fn_result args} {
        # fn_result is the error code
        zip_error_init_with_code zerr $fn_result
        ThrowLibzipError $zerr
    }
    proc ArchiveErrorHandler {fn_result inparams outparams} {
        # fn_result is any value that did not meet requirements
        # Error to be retrieved from archive (pzip parameter)
        set perr [zip_get_error [dict get $inparams pzip]]
        set zerr [zip_error_t fromnative $perr]
        # This is a static pointer, no freeing to be done. Just unregister
        cffi::pointer dispose $perr
        ThrowLibzipError $zerr
    }
    proc ArchiveOpenHandler {fn_result inparams outparams} {
        # fn_result is any value that did not meet requirements
        # Error given by zcode output parameter
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
        zip_libzip_version { string {} }

        # {Error handling functions}
        zip_error_init_with_code { void   {zerr {ZIP_ERROR_REF out} zcode int} }
        zip_error_strerror       { string {zerr ZIP_ERROR_REF} }
        zip_error_code_zip       { int    {zerr ZIP_ERROR_REF} }
        zip_error_code_system    { int    {zerr ZIP_ERROR_REF} }
        zip_error_clear          { void   {pzip PZIP_T}}
        zip_strerror             { string {pzip PZIP_T} }
        zip_get_error            { {pointer.libzip::zip_error_t} {pzip PZIP_T} }
        zip_file_strerror        { string {pfile PZIP_FILE_T} }
        zip_file_get_error       { {pointer.libzip::zip_error_t} {pfile PZIP_FILE_T} }

        # {Open/close archive}
        zip_open {
            {PZIP_T nonzero {onerror ::libzip::ArchiveOpenHandler}}
            {path string flags {int {enum ZipOpenFlags}} zcode {int out storeonerror}}
        }
        zip_close   { LIBZIPSTATUS {pzip {PZIP_T dispose}} }
        zip_discard { void         {pzip {PZIP_T dispose}} }

        # {Archive properties}
        zip_set_default_password {
            LIBZIPSTATUS
            {pzip PZIP_T password {UTF8 nullifempty}}
        }
        zip_get_archive_comment {
            string
            {pzip PZIP_T len {int out} flags ZIP_FLAGS_T}
        }
        zip_set_archive_comment {
            LIBZIPSTATUS
            {pzip PZIP_T comment UTF8 len uint16_t}
        }
        zip_get_archive_flag {
            {int nonnegative}
            {pzip PZIP_T flag_to_check ZIP_FLAGS_T flags ZIP_FLAGS_T}
        }
        
        # {Directory related functions}
        zip_dir_add {
            {int64_t nonnegative {onerror libzip::ArchiveErrorHandler}}
            {pzip PZIP_T name UTF8 flags ZIP_ENCODING}
        }
        zip_get_num_entries {
            {int64_t nonnegative}
            {pzip PZIP_T flags ZIP_FLAGS_T}
        }
        zip_name_locate {
            int64_t
            {pzip PZIP_T fname UTF8 flags ZIP_FLAGS_T }
        }
        zip_get_name {
            {UTF8 nonzero {onerror ArchiveErrorHandler}}
            {pzip PZIP_T index uint64_t flags ZIP_FLAGS_T}
        }
        zip_file_set_mtime {
            LIBZIPSTATUS
            {pzip PZIP_T index uint64_t mtime time_t flags ZIP_FLAGS_T}
        }

        # {Get / set file properties}
        zip_stat {
            LIBZIPSTATUS
            {pzip PZIP_T fname UTF8 flags ZIP_FLAGS_T stat {struct.zip_stat_t out}}
        }
        zip_stat_index {
            LIBZIPSTATUS
            {pzip PZIP_T index uint64_t flags ZIP_FLAGS_T stat {struct.zip_stat_t out}}
        }
        zip_file_get_comment {
            UTF8
            {pzip PZIP_T index uint64_t len {uint32_t out} flags ZIP_ENCODING}
        }
        zip_file_set_comment {
            LIBZIPSTATUS
            {pzip PZIP_T index uint64_t comment UTF8 len uint16_t flags ZIP_ENCODING}
        }
        zip_file_get_external_attributes {
            LIBZIPSTATUS
            {pzip PZIP_T index uint64_t flags ZIP_FLAGS_T opsys {uint8_t out} attrs {uint32_t out}}
        }
        zip_file_set_external_attributes {
            LIBZIPSTATUS
            {pzip PZIP_T index uint64_t flags ZIP_FLAGS_T opsys uint8_t attrs uint32_t}
        }
        zip_file_set_encryption {
            LIBZIPSTATUS
            {pzip PZIP_T index uint64_t enc_method ZIP_ENCRYPTION_METHOD password {UTF8 nullifempty}}
        }
        zip_set_file_compression {
            LIBZIPSTATUS
            {pzip PZIP_T index uint64_t comp ZIP_COMPRESSION_METHOD level {uint32_t {default 0}}}
        }

        # {Add / remove files}
        zip_source_file {
            {PZIP_SOURCE_T nonzero {onerror ::libzip::ArchiveErrorHandler}}
            {pzip PZIP_T fsname string start {uint64_t {default 0}} len { uint64_t {default 0}}}
        }
        zip_file_add {
            {int64_t nonnegative {onerror ::libzip::ArchiveErrorHandler}}
            {pzip PZIP_T utfname UTF8 psource {PZIP_SOURCE_T dispose} flags ZIP_FLAGS_T}
        }
        zip_file_replace {
            LIBZIPSTATUS
            {pzip PZIP_T index uint64_t psource {PZIP_SOURCE_T dispose} flags ZIP_FLAGS_T}
        }
        zip_file_rename {
            LIBZIPSTATUS
            {pzip PZIP_T index uint64_t newname string flags ZIP_ENCODING}
        }
        zip_delete           { LIBZIPSTATUS {pzip PZIP_T index int64_t} }
        zip_unchange         { LIBZIPSTATUS {pzip PZIP_T index uint64_t}}
        zip_unchange_all     { LIBZIPSTATUS {pzip PZIP_T}}
        zip_unchange_archive { LIBZIPSTATUS {pzip PZIP_T}}
        
        
        # {File operations}
        zip_fopen {
            {PZIP_FILE_T nonzero {onerror ::libzip::ArchiveErrorHandler}}
            {pzip PZIP_T utfname UTF8 flags ZIP_FLAGS_T}
        }
        zip_fopen_index {
            {PZIP_FILE_T nonzero {onerror ::libzip::ArchiveErrorHandler}}
            {pzip PZIP_T index uint64_t flags ZIP_FLAGS_T}
        }
        zip_fopen_encrypted {
            {PZIP_FILE_T nonzero {onerror ::libzip::ArchiveErrorHandler}}
            {pzip PZIP_T utfname UTF8 flags ZIP_FLAGS_T password {UTF8 nullifempty}}
        }
        zip_fopen_index_encrypted {
            {PZIP_FILE_T nonzero {onerror ::libzip::ArchiveErrorHandler}}
            {pzip PZIP_T index uint64_t flags ZIP_FLAGS_T password {UTF8 nullifempty}}
        }
        zip_fclose { LIBZIPCODE {pfile {PZIP_FILE_T dispose}} }
        zip_fread  { int64_t {pfile PZIP_FILE_T buf {bytes[nbytes] out} nbytes uint64_t} }
        zip_fseek  { int8_t {zfile PZIP_FILE_T offset int64_t whence int} }
        zip_ftell   { int64_t {zfile PZIP_FILE_T} }

    }



    # zip_error_fini {void {perr pzip_error_t}}
    # zip_error_init {void {perr {struct.zip_error_t out}}}
    # zip_error_set {void {perr pzip_error_t zerr int syserr int}}
    # zip_error_system_type {int {perr pzip_error_t}}
    # zip_error_to_data {{int64_t positive} {# perr pzip_error_t # data {bytes[len] out} # len int64_t # }}
        # zip_fdopen {{pzip_t nonzero {onerror PArchiveOpenHandler}} {fd int flags int pcode {int out storeonerror}}}
        # zip_fdopen pzip_t {int, int, int *_Nullable}
        # zip_file_attributes_init void {pzip_file_attributes_t _Nonnull}
        # zip_file_error_clear void {zfile PZIP_FILE_T}
        # zip_file_extra_field_delete int {pzip PZIP_T uint64_t, uint16_t, flags ZIP_FLAGS_T}
        # zip_file_extra_field_delete_by_id int {pzip PZIP_T uint64_t, uint16_t, uint16_t, flags ZIP_FLAGS_T}
        # zip_file_extra_field_set int {pzip PZIP_T uint64_t, uint16_t, uint16_t, const uint8_t *_Nullable, uint16_t, flags ZIP_FLAGS_T}
        # zip_file_extra_fields_count zip_int16_t {pzip PZIP_T uint64_t, flags ZIP_FLAGS_T}
        # zip_file_extra_fields_count_by_id zip_int16_t {pzip PZIP_T uint64_t, uint16_t, flags ZIP_FLAGS_T}
# const uint8_t *_Nullable zip_file_extra_field_get {pzip PZIP_T uint64_t, uint16_t, uint16_t *_Nullable, uint16_t *_Nullable, flags ZIP_FLAGS_T}
# const uint8_t *_Nullable zip_file_extra_field_get_by_id {pzip PZIP_T uint64_t, uint16_t, uint16_t, uint16_t *_Nullable, flags ZIP_FLAGS_T}
# zip_file_set_dostime int {pzip PZIP_T uint64_t, uint16_t, uint16_t, flags ZIP_FLAGS_T}
# zip_open_from_source pzip_t {pzip_source_t _Nonnull, int, pzip_error_t _Nullable}
# zip_register_progress_callback_with_state int {pzip PZIP_T double, zip_progress_callback _Nullable, void  {*_Nullable} {void *_Nullable}, void *_Nullable}
# zip_register_cancel_callback_with_state int {pzip PZIP_T zip_cancel_callback _Nullable, void  {*_Nullable} {void *_Nullable}, void *_Nullable}
# zip_set_pArchive_comment int {pzip PZIP_T string _Nullable, uint16_t}
# zip_set_pArchive_flag int {pzip PZIP_T flags ZIP_FLAGS_T, int}
# zip_source_begin_write int {pzip_source_t _Nonnull}
# zip_source_begin_write_cloning int {pzip_source_t _Nonnull, uint64_t}
# zip_source_buffer pzip_source_t {pzip PZIP_T const void *_Nullable, uint64_t, int}
# zip_source_buffer_create pzip_source_t {const void *_Nullable, uint64_t, int, pzip_error_t _Nullable}
# zip_source_buffer_fragment pzip_source_t {pzip PZIP_T const zip_buffer_fragment_t *_Nonnull, uint64_t, int}
# zip_source_buffer_fragment_create pzip_source_t {const zip_buffer_fragment_t *_Nullable, uint64_t, int, pzip_error_t _Nullable}
# zip_source_close int {pzip_source_t _Nonnull}
# zip_source_commit_write int {pzip_source_t _Nonnull}
# zip_source_error pzip_error_t {pzip_source_t _Nonnull}
# zip_source_file pzip_source_t {pzip PZIP_T string _Nonnull, uint64_t, int64_t}
# zip_source_file_create pzip_source_t {string _Nonnull, uint64_t, int64_t, pzip_error_t _Nullable}
# zip_source_filep pzip_source_t {pzip PZIP_T FILE *_Nonnull, uint64_t, int64_t}
# zip_source_filep_create pzip_source_t {FILE *_Nonnull, uint64_t, int64_t, pzip_error_t _Nullable}
# zip_source_free void {pzip_source_t _Nullable}
# zip_source_function pzip_source_t {pzip PZIP_T zip_source_callback _Nonnull, void *_Nullable}
# zip_source_function_create pzip_source_t {zip_source_callback _Nonnull, void *_Nullable, pzip_error_t _Nullable}
# zip_source_get_file_attributes int {pzip_source_t _Nonnull, pzip_file_attributes_t _Nonnull}
# zip_source_is_deleted int {pzip_source_t _Nonnull}
# zip_source_keep void {pzip_source_t _Nonnull}
# zip_source_make_command_bitmap int64_t {zip_source_cmd_t, ...}
# zip_source_open int {pzip_source_t _Nonnull}
# zip_source_read int64_t {pzip_source_t _Nonnull, void *_Nonnull, uint64_t}
# zip_source_rollback_write void {pzip_source_t _Nonnull}
# zip_source_seek int {pzip_source_t _Nonnull, int64_t, int}
# zip_source_seek_compute_offset int64_t {uint64_t, uint64_t, void *_Nonnull, uint64_t, pzip_error_t _Nullable}
# zip_source_seek_write int {pzip_source_t _Nonnull, int64_t, int}
# zip_source_stat int {pzip_source_t _Nonnull, zip_stat_t *_Nonnull}
# zip_source_tell int64_t {pzip_source_t _Nonnull}
# int64_t zip_source_tell_write {pzip_source_t _Nonnull}
# #ifdef _WIN32
# zip_source_win32a pzip_source_t {pzip_t , string , uint64_t, int64_t}
# zip_source_win32a_create pzip_source_t {string , uint64_t, int64_t, pzip_error_t }
# zip_source_win32handle pzip_source_t {pzip_t , void *, uint64_t, int64_t}
# zip_source_win32handle_create pzip_source_t {void *, uint64_t, int64_t, pzip_error_t }
# zip_source_win32w pzip_source_t {pzip_t , const wchar_t *, uint64_t, int64_t}
# zip_source_win32w_create pzip_source_t {const wchar_t *, uint64_t, int64_t, pzip_error_t }
# #endif
# zip_source_window_create pzip_source_t {pzip_source_t _Nonnull, uint64_t, int64_t, pzip_error_t _Nullable}
# zip_source_write int64_t {pzip_source_t _Nonnull, const void *_Nullable, uint64_t}
# zip_source_zip pzip_source_t {pzip PZIP_T pzip PZIP_T uint64_t, flags ZIP_FLAGS_T, uint64_t, int64_t}
# zip_source_zip_create pzip_source_t {pzip PZIP_T uint64_t, flags ZIP_FLAGS_T, uint64_t, int64_t, pzip_error_t _Nullable}
# zip_stat_init void {zip_stat_t *_Nonnull}
# zip_strerror string {pArchive pzip_t}
# zip_unchange int {pzip PZIP_T uint64_t}
# zip_unchange_all int {pArchive pzip_t}
# zip_unchange_pArchive int {pArchive pzip_t}
# zip_compression_method_supported int {zip_int32_t method, int compress}
# zip_encryption_method_supported int {uint16_t method, int encode}
    
    proc InitFunctions {{lazy 1}} {
        variable Functions
        foreach {fn_name prototype} [array get Functions] {
            if {$fn_name eq "#"} continue
            if {$lazy} {
                proc $fn_name args "libzip function $fn_name [list {*}$prototype]; tailcall $fn_name {*}\$args"
            } else {
                libzip function $fn_name {*}$prototype
            }
        }
    }

    proc init [list [list path "libzip[info sharedlibextension]"] [list lazy 1]] {
        variable Functions

        ::cffi::dyncall::Library create libzip $path
        InitTypes
        InitFunctions $lazy

        # Redefine to no-op
        proc init args {}
    }

}
