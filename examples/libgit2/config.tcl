# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

# Based on libgit2/include/config.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum define git_config_level_t {
    GIT_CONFIG_LEVEL_PROGRAMDATA 1
    GIT_CONFIG_LEVEL_SYSTEM      2
    GIT_CONFIG_LEVEL_XDG         3
    GIT_CONFIG_LEVEL_GLOBAL      4
    GIT_CONFIG_LEVEL_LOCAL       5
    GIT_CONFIG_LEVEL_APP         6
    GIT_CONFIG_HIGHEST_LEVEL     -1
}
::cffi::alias define GIT_CONFIG_LEVEL_T {int {enum git_config_level_t}}

::cffi::prototype function git_config_entry_free_cb int {
    entry pointer.git_config_entry
}

# name and value pointer, not string, because they point to internal libgit
# storage which is freed. Thus at script level we pass around as pointers
# and explicitly access.
::cffi::Struct create git_config_entry {
    name          {pointer unsafe}
    value         {pointer unsafe}
    include_depth uint
    level         GIT_CONFIG_LEVEL_T
    free          pointer.git_config_entry_free_cb
    payload       CB_PAYLOAD
}
::cffi::alias define PCONFIG_ENTRY pointer.git_config_entry

::cffi::prototype function git_config_foreach_cb int {
    entry {struct.git_config_entry byref}
    payload CB_PAYLOAD
}

::cffi::alias define PCONFIG_ITERATOR pointer.git_config_iterator

::cffi::enum sequence git_configmap_t {
    GIT_CONFIGMAP_FALSE
    GIT_CONFIGMAP_TRUE
    GIT_CONFIGMAP_INT32
    GIT_CONFIGMAP_STRING
}
::cffi::alias define GIT_CONFIGMAP_T {int {enum git_configmap_t}}

::cffi::Struct create git_configmap {
    type      GIT_CONFIGMAP_T
    str_match STRING
    map_value int
}

libgit2 functions {
    git_config_entry_free void {
        entry {PCONFIG_ENTRY nullok dispose}
    }
    git_config_find_global GIT_ERROR_CODE {
        buf PBUF
    }
    git_config_find_xdg GIT_ERROR_CODE {
        buf PBUF
    }
    git_config_find_system GIT_ERROR_CODE {
        buf PBUF
    }
    git_config_find_programdata GIT_ERROR_CODE {
        buf PBUF
    }
    git_config_open_default GIT_ERROR_CODE {
        cfg {PCONFIG retval}
    }
    git_config_new GIT_ERROR_CODE {
        cfg {PCONFIG retval}
    }
    git_config_add_file_ondisk GIT_ERROR_CODE {
        cfg   PCONFIG
        path  STRING
        level GIT_CONFIG_LEVEL_T
        pRepo PREPOSITORY
        force int
    }
    git_config_open_ondisk GIT_ERROR_CODE {
        cfg {PCONFIG retval}
        path STRING
    }
    git_config_open_level GIT_ERROR_CODE {
        cfg    {PCONFIG retval}
        parent PCONFIG
        level  GIT_CONFIG_LEVEL_T
    }
    git_config_open_global GIT_ERROR_CODE {
        global_cfg {PCONFIG retval}
        cfg        PCONFIG
    }
    git_config_snapshot GIT_ERROR_CODE {
        snapshot {PCONFIG retval}
        cfg      PCONFIG
    }
    git_config_free void {
        cfg {PCONFIG nullok dispose}
    }
    git_config_get_entry GIT_ERROR_CODE {
        entry {PCONFIG_ENTRY retval}
        cfg   PCONFIG
        name  STRING
    }
    git_config_get_int32 GIT_ERROR_CODE {
        value {int32_t retval}
        cfg   PCONFIG
        name  STRING
    }
    git_config_get_int64 GIT_ERROR_CODE {
        value {int64_t retval}
        cfg   PCONFIG
        name  STRING
    }
    git_config_get_bool GIT_ERROR_CODE {
        value {int retval}
        cfg   PCONFIG
        name  STRING
    }
    git_config_get_path GIT_ERROR_CODE {
        buf  {PBUF retval}
        cfg  PCONFIG
        name STRING
    }
    git_config_get_string GIT_ERROR_CODE {
        value {STRING retval}
        cfg   PCONFIG
        name  STRING
    }
    git_config_get_string_buf GIT_ERROR_CODE {
        buf  {PBUF retval}
        cfg  PCONFIG
        name STRING
    }
    git_config_get_multivar_foreach GIT_ERROR_CODE {
        cfg      PCONFIG
        name     STRING
        regexp   STRING
        callback pointer.git_config_foreach_cb
        payload  CB_PAYLOAD
    }
    git_config_multivar_iterator_new GIT_ERROR_CODE {
        iterator {PCONFIG_ITERATOR retval}
        cfg      PCONFIG
        name     STRING
        regexp   STRING
    }
    git_config_next GIT_ITER_ERROR_CODE {
        entry    {PCONFIG_ENTRY out}
        iterator PCONFIG_ITERATOR
    }
    git_config_iterator_free void {
        iterator {PCONFIG_ITERATOR nullok dispose}
    }
    git_config_set_int32 GIT_ERROR_CODE {
        cfg   PCONFIG
        name  STRING
        value int32_t
    }
    git_config_set_int64 GIT_ERROR_CODE {
        cfg   PCONFIG
        name  STRING
        value int64_t
    }
    git_config_set_bool GIT_ERROR_CODE {
        cfg   PCONFIG
        name  STRING
        value int
    }
    git_config_set_string GIT_ERROR_CODE {
        cfg   PCONFIG
        name  STRING
        value STRING
    }
    git_config_set_multivar GIT_ERROR_CODE {
        cfg     PCONFIG
        name    STRING
        regexpr STRING
        value   STRING
    }
    git_config_delete_entry GIT_ERROR_CODE {
        cfg  PCONFIG
        name STRING
    }
    git_config_delete_multivar GIT_ERROR_CODE {
        cfg    PCONFIG
        name   STRING
        regexp STRING
    }
    git_config_foreach GIT_ERROR_CODE {
        cfg      PCONFIG
        callback pointer.git_config_foreach_cb
        payload  CB_PAYLOAD
    }
    git_config_iterator_new GIT_ERROR_CODE {
        iterator {PCONFIG_ITERATOR retval}
        cfg      PCONFIG
    }
    git_config_iterator_glob_new GIT_ERROR_CODE {
        iterator {PCONFIG_ITERATOR retval}
        cfg      PCONFIG
        regexp   STRING
    }
    git_config_foreach_match GIT_ERROR_CODE {
        cfg      PCONFIG
        regexp   STRING
        callback pointer.git_config_foreach_cb
        payload  CB_PAYLOAD
    }
    git_config_get_mapped GIT_ERROR_CODE {
        result {int retval}
        cfg    PCONFIG
        name   STRING
        maps   struct.git_configmap[map_n]
        map_n  size_t
    }
    git_config_lookup_map_value GIT_ERROR_CODE {
        result {int retval}
        maps   struct.git_configmap[map_n]
        map_n  size_t
        value  STRING
    }
    git_config_parse_bool GIT_ERROR_CODE {
        result {int retval}
        value  STRING
    }
    git_config_parse_int32 GIT_ERROR_CODE {
        result {int32_t retval}
        value  STRING
    }
    git_config_parse_int64 GIT_ERROR_CODE {
        result {int64_t retval}
        value  STRING
    }
    git_config_parse_path GIT_ERROR_CODE {
        buf   PBUF
        value STRING
    }
    git_config_backend_foreach_match GIT_ERROR_CODE {
        backend  PCONFIG_BACKEND
        regexp   STRING
        callback pointer.git_config_foreach_cb
        payload  CB_PAYLOAD
    }
    git_config_lock GIT_ERROR_CODE {
        transaction {PTRANSACTION retval}
        cfg         PCONFIG
    }
}
