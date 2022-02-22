# This file should be sourced into whatever namespace commands should
# be created in.

# Utility commands useful for applications using libgit2. Not part of libgit2 API.
# See the README.


proc lg2_throw_last_error {code} {
    set codename [::cffi::enum name git_error_code $code $code]
    set p [git_error_last]
    if {[::cffi::pointer isnull $p]} {
        throw [list GIT $code $codename UNKNOWN] "libgit2 error: Error code $code"
    } else {
        set last_error [git_error fromnative! $p]
        dict with last_error {} ; # klass, message
        throw [list GIT $code $codename $klass] "libgit2 error: $message"
    }
}

variable iana_to_tcl_encoding
array set iana_to_tcl_encoding {
    437 cp437
    850 cp850
    852 cp852
    855 cp855
    857 cp857
    860 cp860
    861 cp861
    862 cp862
    863 cp863
    865 cp865
    866 cp866
    869 cp869
    ansi_x3.4-1968 ascii
    ansi_x3.4-1986 ascii
    ascii ascii
    big5 big5
    cp-gr cp869
    cp-is cp861
    cp1250 cp1250
    cp1251 cp1251
    cp1252 cp1252
    cp1253 cp1253
    cp1254 cp1254
    cp1255 cp1255
    cp1256 cp1256
    cp1257 cp1257
    cp1258 cp1258
    cp367 ascii
    cp437 cp437
    cp737 cp737
    cp775 cp775
    cp850 cp850
    cp852 cp852
    cp855 cp855
    cp857 cp857
    cp860 cp860
    cp861 cp861
    cp862 cp862
    cp863 cp863
    cp864 cp864
    cp865 cp865
    cp866 cp866
    cp869 cp869
    cp874 cp874
    cp932 cp932
    cp936 cp936
    cp949 cp949
    cp950 cp950
    csascii ascii
    csbig5 big5
    cseuckr euc-kr
    cseucpkdfmtjapanese euc-jp
    csgb2312 gb2312
    csibm855 cp855
    csibm857 cp857
    csibm860 cp860
    csibm861 cp861
    csibm863 cp863
    csibm864 cp864
    csibm865 cp865
    csibm866 cp866
    csibm869 cp869
    cskoi8r koi8-r
    cspc775baltic       cp775
    cspc850multilingual cp850
    cspc862latinhebrew  cp862
    cspc8codepage437    cp437
    cspcp852            cp852
    csshiftjis shiftjis
    dingbats dingbats
    ebcdic ebcdic
    euc-cn euc-cn
    euc-jp euc-jp
    euc-kr euc-kr
    extended_unix_code_packed_format_for_japanese euc-jp
    gb12345 gb12345
    gb1988 gb1988
    gb2312 gb2312
    gb2312-raw gb2312-raw
    gbk cp936
    ibm367 ascii
    ibm437 cp437
    ibm775 cp775
    ibm850 cp850
    ibm852 cp852
    ibm855 cp855
    ibm857 cp857
    ibm860 cp860
    ibm861 cp861
    ibm862 cp862
    ibm863 cp863
    ibm864 cp864
    ibm865 cp865
    ibm866 cp866
    ibm869 cp869
    identity identity
    iso-ir-6   ascii
    iso2022    iso2022
    iso2022-jp iso2022-jp
    iso2022-kr iso2022-kr
    iso646-us  ascii
    iso8859-1  iso8859-1
    iso8859-10 iso8859-10
    iso8859-13 iso8859-13
    iso8859-14 iso8859-14
    iso8859-15 iso8859-15
    iso8859-16 iso8859-16
    iso8859-2  iso8859-2
    iso8859-3  iso8859-3
    iso8859-4  iso8859-4
    iso8859-5  iso8859-5
    iso8859-6  iso8859-6
    iso8859-7  iso8859-7
    iso8859-8  iso8859-8
    iso8859-9  iso8859-9
    iso_646.irv:1991 ascii
    jis0201 jis0201
    jis0208 jis0208
    jis0212 jis0212
    koi8-r koi8-r
    koi8-u koi8-u
    ksc5601 ksc5601
    maccenteuro macCentEuro
    maccroatian macCroatian
    maccyrillic macCyrillic
    macdingbats macDingbats
    macgreek    macGreek
    maciceland  macIceland
    macjapan    macJapan
    macroman    macRoman
    macromania  macRomania
    macthai     macThai
    macturkish  macTurkish
    macukraine  macUkraine
    ms936 cp936
    ms_kanji  shiftjis
    shift-jis shiftjis
    shift_jis shiftjis
    shiftjis shiftjis
    symbol symbol
    tis-620 tis-620
    unicode unicode
    us       ascii
    us-ascii ascii
    utf-8 utf-8
    windows-936 cp936
}

proc lg2_iana_to_tcl_encoding {iana} {
    variable iana_to_tcl_encoding
    if {$iana eq ""} {
        # libgit2 null encoding is utf-8
        return utf-8
    }
    set iana [string tolower $iana]
    if {[info exists iana_to_tcl_encoding($iana)]} {
        return $iana_to_tcl_encoding($iana)
    }
    set iana [string map {- "" _ "" : ""} $iana]
    if {[info exists iana_to_tcl_encoding($iana)]} {
        return $iana_to_tcl_encoding($iana)
    }
    return utf-8
}

namespace eval lg2_encoding {
    # Like Tcl encoding but uses IANA names and defaults to utf-8, not system
    proc convertto {args} {
        if {[llength $args] == 2} {
            lassign $args enc text
            set enc [lg2_iana_to_tcl_encoding $enc]
        } else {
            set text [lindex $args 0]
            set enc utf-8
        }
        encoding convertto $enc $text
    }
    proc convertfrom {args} {
        if {[llength $args] == 2} {
            lassign $args enc text
            set enc [lg2_iana_to_tcl_encoding $enc]
        } else {
            set text [lindex $args 0]
            set enc utf-8
        }
        encoding convertfrom $enc $text
    }
    namespace export convertto convertfrom
    namespace ensemble create
}
