/*
 * a set of functions to test arguments and returns in dyncall.
 */

/*
 * IMPORTANT: we need tcl.h so we can get Tcl_UniChar type BUT we should not
 * be making use of any Tcl functions as this DLL should be loadable and
 * unloadable on the fly during testing (e.g. bug #59)
 */
#include <tcl.h>
#include <stddef.h>
#include <stdint.h>		/* uintptr_t */
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>

#ifdef _WIN32
#include <windows.h>
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT
#define CFFI_STDCALL __stdcall
#else
#define CFFI_STDCALL
#endif

#ifdef _WIN32
typedef UUID uuid_t;
#else
#include <uuid/uuid.h>
typedef struct UUID {
    uuid_t bytes;
} UUID;
#endif

#define FNSTR2NUM(name_, fmt_, type_)           \
    EXTERN type_ name_ (char *s) {              \
        type_ val;                              \
        sscanf(s, "%" #fmt_, &val);             \
        return val;                             \
    }

#define FNINOUT(token_, type_)                       \
    EXTERN type_ token_##_out(type_ in, type_ *out)  \
    {                                                \
        if (out)                                     \
            *out = in + 1;                           \
        return in + 2;                               \
    }                                                \
    EXTERN int token_##_retval(type_ in, type_ *out) \
    {                                                \
        if (out)                                     \
            *out = in + 1;                           \
        return 1;                                    \
    }                                                \
    EXTERN type_ token_##_byref(type_ *inP)          \
    {                                                \
        return *inP + 2;                             \
    }                                                \
    EXTERN type_ token_##_inout(type_ *inoutP)       \
    {                                                \
        if (inoutP) {                                \
            *inoutP = *inoutP + 1;                   \
            return *inoutP + 1;                      \
        }                                            \
        else                                         \
            return 0;                                \
    }                                                \
    EXTERN type_ *token_##_ret_byref(type_ in)       \
    {                                                \
        static type_ static_val;                     \
        static_val = in + 1;                         \
        return &static_val;                          \
    }

#define FNDYNAMICARRAY(token_, type_)                            \
    EXTERN void token_##_array_dynamic_copy(                     \
        type_ *n_out, type_ *arr_out, type_ n_in, type_ *arr_in) \
    {                                                            \
        type_ i;                                                 \
        if (n_in < *n_out)                                       \
            *n_out = n_in;                                       \
        for (i = 0; i < *n_out; ++i) {                           \
            arr_out[i] = arr_in[i];                              \
        }                                                        \
    }

#define FNNUMERICARRAY(token_, type_)                             \
    EXTERN void token_##_array_count_copy(                        \
        const type_ *arr_in, int n_in, type_ *arr_out, int n_out) \
    {                                                             \
        int i, n;                                                 \
        n = n_out;                                                \
        if (n > n_in)                                             \
            n = n_in;                                             \
        for (i = 0; i < n; ++i)                                   \
            arr_out[i] = arr_in[i];                               \
        while (i < n_out)                                         \
            arr_out[i++] = 0;                                     \
    }                                                             \
    EXTERN void token_##_count_array_copy(                        \
        int n_out, type_ *arr_out, int n_in, const type_ *arr_in) \
    {                                                             \
        int i, n;                                                 \
        n = n_out;                                                \
        if (n > n_in)                                             \
            n = n_in;                                             \
        for (i = 0; i < n; ++i)                                   \
            arr_out[i] = arr_in[i];                               \
        while (i < n_out)                                         \
            arr_out[i++] = 0;                                     \
    }                                                             \
    EXTERN type_ token_##_array_count_in(const type_ *arr, int n) \
    {                                                             \
        type_ sum;                                                \
        int i;                                                    \
        if (arr == NULL)                                          \
            return (type_)-1;                                     \
        for (i = 0, sum = 0; i < n; ++i)                          \
            sum += arr[i];                                        \
        return sum;                                               \
    }                                                             \
    EXTERN type_ token_##_array_in(int n, const type_ *arr)       \
    {                                                             \
        type_ sum;                                                \
        int i;                                                    \
        if (arr == NULL)                                          \
            return (type_)-1;                                     \
        for (i = 0, sum = 0; i < n; ++i)                          \
            sum += arr[i];                                        \
        return sum;                                               \
    }                                                             \
    EXTERN void token_##_array_out(int n, type_ *arr)             \
    {                                                             \
        int i;                                                    \
        for (i = 0; i < n; ++i)                                   \
            arr[i] = (type_)i;                                    \
    }                                                             \
    EXTERN void token_##_array_inout(int n, type_ *arr)           \
    {                                                             \
        int i;                                                    \
        for (i = 0; i < n; ++i)                                   \
            arr[i] = arr[i] + 1;                                  \
    }

#define FNSTRINGS(token_, type_)                     \
    EXTERN int token_##_out(type_ *in, type_ *out)   \
    {                                                \
        int i;                                       \
        if (out == NULL)                             \
            return 0;                                \
        for (i = 0; in[i]; ++i)                      \
            out[i] = in[i];                          \
        out[i] = in[i];                              \
        return i;                                    \
    }                                                \
    EXTERN int token_##_reverse_inout(type_ *inout)  \
    {                                                \
        int i, len;                                  \
        type_ ch;                                    \
        if (inout == NULL)                           \
            return 0;                                \
        for (len = 0; inout[len]; ++len)             \
            ;                                        \
        for (i = 0; i < len / 2; ++i) {              \
            ch                 = inout[i];           \
            inout[i]           = inout[len - i - 1]; \
            inout[len - i - 1] = ch;                 \
        }                                            \
        return len;                                  \
    }                                                \
    EXTERN int token_##_len(type_ *in)               \
    {                                                \
        int len = 0;                                 \
        if (in)                                      \
            while (*in++)                            \
                ++len;                               \
        return len;                                  \
    }                                                \
    EXTERN int token_##_inbyref_len(type_ **inP)     \
    {                                                \
        type_ *in = *inP;                            \
        int len   = 0;                               \
        while (*in++)                                \
            ++len;                                   \
        return len;                                  \
    }

typedef struct InnerTestStruct {
    char c[15];
} InnerTestStruct;

typedef struct TestStruct {
    signed char c;
    int i;
    short shrt;
    unsigned int uint;
    unsigned short ushrt;
    long l;
    unsigned char uc;
    unsigned long ul;
    char chars[11];
    long long ll;
    Tcl_UniChar unic[7];
    unsigned long long ull;
    unsigned char b[3];
    float f;
    InnerTestStruct s;
    double d;
#ifdef _WIN32
    WCHAR wchars[13];
#endif
} TestStruct;

EXTERN int noargs () { return 42; }
EXTERN int onearg (int arga) { return -arga; }
EXTERN int twoargs (int arga, int argb) { return arga + argb; }
EXTERN int threeargs (int arga, int argb, int argc) { return arga + argb + argc; }

DLLEXPORT double CFFI_STDCALL stdcalltest(double arga, double argb) {
  /* Division so order of argument errors will be caught */
    return arga / argb;
}

EXTERN void schar_to_void(signed char a) { return ; }
EXTERN void uchar_to_void(unsigned char a) { return ; }
EXTERN void short_to_void(signed short a) { return ; }
EXTERN void ushort_to_void(unsigned short a) { return ; }
EXTERN void int_to_void(signed int a) { return ; }
EXTERN void uint_to_void(unsigned int a) { return ; }
EXTERN void long_to_void(signed long a) { return ; }
EXTERN void ulong_to_void(unsigned long a) { return ; }
EXTERN void longlong_to_void(signed long long a) { return ; }
EXTERN void ulonglong_to_void(unsigned long long a) { return ; }
EXTERN void float_to_void(float a) { return ; }
EXTERN void double_to_void(double a) { return ; }
EXTERN void pointer_to_void(void *a) { return ; }
EXTERN void string_to_void(char *s) { return; }
EXTERN void unistring_to_void(Tcl_UniChar *s) { return; }
#ifdef _WIN32
EXTERN void winstring_to_void(WCHAR *s) { return; }
#endif
EXTERN void chars_to_void(char s[]) { return; }
EXTERN void unichars_to_void(Tcl_UniChar s[]) { return; }
EXTERN void winchars_to_void(Tcl_UniChar s[]) { return; }
EXTERN void binary_to_void(unsigned char *b) { return; }
EXTERN void bytes_to_void(unsigned char *b[]) { return; }


EXTERN signed char schar_to_schar(signed char a) { return a; }
EXTERN signed char uchar_to_schar(unsigned char a) { return (signed char) a; }
EXTERN signed char short_to_schar(signed short a) { return (signed char) a; }
EXTERN signed char ushort_to_schar(unsigned short a) { return (signed char) a; }
EXTERN signed char int_to_schar(signed int a) { return (signed char) a; }
EXTERN signed char uint_to_schar(unsigned int a) { return (signed char) a; }
EXTERN signed char long_to_schar(signed long a) { return (signed char) a; }
EXTERN signed char ulong_to_schar(unsigned long a) { return (signed char) a; }
EXTERN signed char longlong_to_schar(signed long long a) { return (signed char) a; }
EXTERN signed char ulonglong_to_schar(unsigned long long a) { return (signed char) a; }
EXTERN signed char float_to_schar(float a) { return (signed char) a; }
EXTERN signed char double_to_schar(double a) { return (signed char) a; }
EXTERN signed char pointer_to_schar(void *a) { return (signed char)(uintptr_t)a; }
FNSTR2NUM(string_to_schar, hhd, signed char)
FNINOUT(schar, signed char)
FNNUMERICARRAY(schar, signed char)
FNDYNAMICARRAY(schar, signed char)


EXTERN unsigned char schar_to_uchar(signed char a) { return (unsigned char) a; }
EXTERN unsigned char uchar_to_uchar(unsigned char a) { return a; }
EXTERN unsigned char short_to_uchar(signed short a) { return (unsigned char) a; }
EXTERN unsigned char ushort_to_uchar(unsigned short a) { return (unsigned char) a; }
EXTERN unsigned char int_to_uchar(signed int a) { return (unsigned char) a; }
EXTERN unsigned char uint_to_uchar(unsigned int a) { return (unsigned char) a; }
EXTERN unsigned char long_to_uchar(signed long a) { return (unsigned char) a; }
EXTERN unsigned char ulong_to_uchar(unsigned long a) { return (unsigned char) a; }
EXTERN unsigned char longlong_to_uchar(signed long long a) { return (unsigned char) a; }
EXTERN unsigned char ulonglong_to_uchar(unsigned long long a) { return (unsigned char) a; }
EXTERN unsigned char float_to_uchar(float a) { return (unsigned char) a; }
EXTERN unsigned char double_to_uchar(double a) { return (unsigned char) a; }
EXTERN unsigned char pointer_to_uchar(void *a) { return (unsigned char)(uintptr_t)a; }
FNSTR2NUM(string_to_uchar, hhu, unsigned char)
FNINOUT(uchar, unsigned char)
FNNUMERICARRAY(uchar, unsigned char)
FNDYNAMICARRAY(uchar, unsigned char)

EXTERN signed short schar_to_short(signed char a) { return (signed short) a; }
EXTERN signed short uchar_to_short(unsigned char a) { return (signed short) a; }
EXTERN signed short short_to_short(signed short a) { return a; }
EXTERN signed short ushort_to_short(unsigned short a) { return (signed short) a; }
EXTERN signed short int_to_short(signed int a) { return (signed short) a; }
EXTERN signed short uint_to_short(unsigned int a) { return (signed short) a; }
EXTERN signed short long_to_short(signed long a) { return (signed short) a; }
EXTERN signed short ulong_to_short(unsigned long a) { return (signed short) a; }
EXTERN signed short longlong_to_short(signed long long a) { return (signed short) a; }
EXTERN signed short ulonglong_to_short(unsigned long long a) { return (signed short) a; }
EXTERN signed short float_to_short(float a) { return (signed short) a; }
EXTERN signed short double_to_short(double a) { return (signed short) a; }
EXTERN signed short pointer_to_short(void *a) { return (signed short)(uintptr_t)a; }
FNSTR2NUM(string_to_short, hd, short)
FNINOUT(short, short)
FNNUMERICARRAY(short, short)
FNDYNAMICARRAY(short, short)

EXTERN unsigned short schar_to_ushort(signed char a) { return (unsigned short) a; }
EXTERN unsigned short uchar_to_ushort(unsigned char a) { return (unsigned short) a; }
EXTERN unsigned short short_to_ushort(signed short a) { return (unsigned short) a; }
EXTERN unsigned short ushort_to_ushort(unsigned short a) { return a; }
EXTERN unsigned short int_to_ushort(signed int a) { return (unsigned short) a; }
EXTERN unsigned short uint_to_ushort(unsigned int a) { return (unsigned short) a; }
EXTERN unsigned short long_to_ushort(signed long a) { return (unsigned short) a; }
EXTERN unsigned short ulong_to_ushort(unsigned long a) { return (unsigned short) a; }
EXTERN unsigned short longlong_to_ushort(signed long long a) { return (unsigned short) a; }
EXTERN unsigned short ulonglong_to_ushort(unsigned long long a) { return (unsigned short) a; }
EXTERN unsigned short float_to_ushort(float a) { return (unsigned short) a; }
EXTERN unsigned short double_to_ushort(double a) { return (unsigned short) a; }
EXTERN unsigned short pointer_to_ushort(void *a) { return (unsigned short)(uintptr_t)a; }
FNSTR2NUM(string_to_ushort, hu, unsigned short)
FNINOUT(ushort, unsigned short)
FNNUMERICARRAY(ushort, unsigned short)
FNDYNAMICARRAY(ushort, unsigned short)

EXTERN signed int schar_to_int(signed char a) { return (signed int) a; }
EXTERN signed int uchar_to_int(unsigned char a) { return (signed int) a; }
EXTERN signed int short_to_int(signed short a) { return (signed int) a; }
EXTERN signed int ushort_to_int(unsigned short a) { return (signed int) a; }
EXTERN signed int int_to_int(signed int a) { return a; }
EXTERN signed int uint_to_int(unsigned int a) { return (signed int) a; }
EXTERN signed int long_to_int(signed long a) { return (signed int) a; }
EXTERN signed int ulong_to_int(unsigned long a) { return (signed int) a; }
EXTERN signed int longlong_to_int(signed long long a) { return (signed int) a; }
EXTERN signed int ulonglong_to_int(unsigned long long a) { return (signed int) a; }
EXTERN signed int float_to_int(float a) { return (signed int) a; }
EXTERN signed int double_to_int(double a) { return (signed int) a; }
EXTERN signed int pointer_to_int(void *a) { return (signed int)(uintptr_t)a; }
FNSTR2NUM(string_to_int, d, int)
FNINOUT(int, int)
FNNUMERICARRAY(int, int)
FNDYNAMICARRAY(int, int)

EXTERN unsigned int schar_to_uint(signed char a) { return (unsigned int) a; }
EXTERN unsigned int uchar_to_uint(unsigned char a) { return (unsigned int) a; }
EXTERN unsigned int short_to_uint(signed short a) { return (unsigned int) a; }
EXTERN unsigned int ushort_to_uint(unsigned short a) { return (unsigned int) a; }
EXTERN unsigned int int_to_uint(signed int a) { return (unsigned int) a; }
EXTERN unsigned int uint_to_uint(unsigned int a) { return a; }
EXTERN unsigned int long_to_uint(signed long a) { return (unsigned int) a; }
EXTERN unsigned int ulong_to_uint(unsigned long a) { return (unsigned int) a; }
EXTERN unsigned int longlong_to_uint(signed long long a) { return (unsigned int) a; }
EXTERN unsigned int ulonglong_to_uint(unsigned long long a) { return (unsigned int) a; }
EXTERN unsigned int float_to_uint(float a) { return (unsigned int) a; }
EXTERN unsigned int double_to_uint(double a) { return (unsigned int) a; }
EXTERN unsigned int pointer_to_uint(void *a) { return (unsigned int)(uintptr_t)a; }
FNSTR2NUM(string_to_uint, u, unsigned int)
FNINOUT(uint, unsigned int)
FNNUMERICARRAY(uint, unsigned int)
FNDYNAMICARRAY(uint, unsigned int)

EXTERN signed long schar_to_long(signed char a) { return (signed long) a; }
EXTERN signed long uchar_to_long(unsigned char a) { return (signed long) a; }
EXTERN signed long short_to_long(signed short a) { return (signed long) a; }
EXTERN signed long ushort_to_long(unsigned short a) { return (signed long) a; }
EXTERN signed long int_to_long(signed int a) { return (signed long) a; }
EXTERN signed long uint_to_long(unsigned int a) { return (signed long) a; }
EXTERN signed long long_to_long(signed long a) { return a; }
EXTERN signed long ulong_to_long(unsigned long a) { return (signed long) a; }
EXTERN signed long longlong_to_long(signed long long a) { return (signed long) a; }
EXTERN signed long ulonglong_to_long(unsigned long long a) { return (signed long) a; }
EXTERN signed long float_to_long(float a) { return (signed long) a; }
EXTERN signed long double_to_long(double a) { return (signed long) a; }
EXTERN signed long pointer_to_long(void *a) { return (signed long)(uintptr_t)a; }
FNSTR2NUM(string_to_long, ld, long)
FNINOUT(long, long)
FNNUMERICARRAY(long, long)
FNDYNAMICARRAY(long, long)

EXTERN unsigned long schar_to_ulong(signed char a) { return (unsigned long) a; }
EXTERN unsigned long uchar_to_ulong(unsigned char a) { return (unsigned long) a; }
EXTERN unsigned long short_to_ulong(signed short a) { return (unsigned long) a; }
EXTERN unsigned long ushort_to_ulong(unsigned short a) { return (unsigned long) a; }
EXTERN unsigned long int_to_ulong(signed int a) { return (unsigned long) a; }
EXTERN unsigned long uint_to_ulong(unsigned int a) { return (unsigned long) a; }
EXTERN unsigned long long_to_ulong(signed long a) { return (unsigned long) a; }
EXTERN unsigned long ulong_to_ulong(unsigned long a) { return a; }
EXTERN unsigned long longlong_to_ulong(signed long long a) { return (unsigned long) a; }
EXTERN unsigned long ulonglong_to_ulong(unsigned long long a) { return (unsigned long) a; }
EXTERN unsigned long float_to_ulong(float a) { return (unsigned long) a; }
EXTERN unsigned long double_to_ulong(double a) { return (unsigned long) a; }
EXTERN unsigned long pointer_to_ulong(void *a) { return (unsigned long)(uintptr_t)a; }
FNSTR2NUM(string_to_ulong, lu, unsigned long)
FNINOUT(ulong, unsigned long)
FNNUMERICARRAY(ulong, unsigned long)
FNDYNAMICARRAY(ulong, unsigned long)

EXTERN signed long long schar_to_longlong(signed char a) { return (signed long long) a; }
EXTERN signed long long uchar_to_longlong(unsigned char a) { return (signed long long) a; }
EXTERN signed long long short_to_longlong(signed short a) { return (signed long long) a; }
EXTERN signed long long ushort_to_longlong(unsigned short a) { return (signed long long) a; }
EXTERN signed long long int_to_longlong(signed int a) { return (signed long long) a; }
EXTERN signed long long uint_to_longlong(unsigned int a) { return (signed long long) a; }
EXTERN signed long long long_to_longlong(signed long a) { return (signed long long) a; }
EXTERN signed long long ulong_to_longlong(unsigned long a) { return (signed long long) a; }
EXTERN signed long long longlong_to_longlong(signed long long a) { return a; }
EXTERN signed long long ulonglong_to_longlong(unsigned long long a) { return (signed long long) a; }
EXTERN signed long long float_to_longlong(float a) { return (signed long long) a; }
EXTERN signed long long double_to_longlong(double a) { return (signed long long) a; }
EXTERN signed long long pointer_to_longlong(void *a) { return (signed long long)(uintptr_t)a; }
FNSTR2NUM(string_to_longlong, lld, long long)
FNINOUT(longlong, long long)
FNNUMERICARRAY(longlong, long long)
FNDYNAMICARRAY(longlong, long long)

EXTERN unsigned long long schar_to_ulonglong(signed char a) { return (unsigned long long) a; }
EXTERN unsigned long long uchar_to_ulonglong(unsigned char a) { return (unsigned long long) a; }
EXTERN unsigned long long short_to_ulonglong(signed short a) { return (unsigned long long) a; }
EXTERN unsigned long long ushort_to_ulonglong(unsigned short a) { return (unsigned long long) a; }
EXTERN unsigned long long int_to_ulonglong(signed int a) { return (unsigned long long) a; }
EXTERN unsigned long long uint_to_ulonglong(unsigned int a) { return (unsigned long long) a; }
EXTERN unsigned long long long_to_ulonglong(signed long a) { return (unsigned long long) a; }
EXTERN unsigned long long ulong_to_ulonglong(unsigned long a) { return (unsigned long long) a; }
EXTERN unsigned long long longlong_to_ulonglong(signed long long a) { return (unsigned long long) a; }
EXTERN unsigned long long ulonglong_to_ulonglong(unsigned long long a) { return a; }
EXTERN unsigned long long float_to_ulonglong(float a) { return (unsigned long long) a; }
EXTERN unsigned long long double_to_ulonglong(double a) { return (unsigned long long) a; }
EXTERN unsigned long long pointer_to_ulonglong(void *a) { return (signed long long)(uintptr_t)a; }
FNSTR2NUM(string_to_ulonglong, llu, unsigned long long)
FNINOUT(ulonglong, unsigned long long)
FNNUMERICARRAY(ulonglong, unsigned long long)
FNDYNAMICARRAY(ulonglong, unsigned long long)

EXTERN float schar_to_float(signed char a) { return (float) a; }
EXTERN float uchar_to_float(unsigned char a) { return (float) a; }
EXTERN float short_to_float(signed short a) { return (float) a; }
EXTERN float ushort_to_float(unsigned short a) { return (float) a; }
EXTERN float int_to_float(signed int a) { return (float) a; }
EXTERN float uint_to_float(unsigned int a) { return (float) a; }
EXTERN float long_to_float(signed long a) { return (float) a; }
EXTERN float ulong_to_float(unsigned long a) { return (float) a; }
EXTERN float longlong_to_float(signed long long a) { return (float) a; }
EXTERN float ulonglong_to_float(unsigned long long a) { return (float) a; }
EXTERN float float_to_float(float a) { return a; }
EXTERN float double_to_float(double a) { return (float) a; }
EXTERN float pointer_to_float(void *a) { return (float)(uintptr_t)a; }
FNSTR2NUM(string_to_float, f, float)
FNINOUT(float, float)
FNNUMERICARRAY(float, float)

EXTERN double schar_to_double(signed char a) { return (double) a; }
EXTERN double uchar_to_double(unsigned char a) { return (double) a; }
EXTERN double short_to_double(signed short a) { return (double) a; }
EXTERN double ushort_to_double(unsigned short a) { return (double) a; }
EXTERN double int_to_double(signed int a) { return (double) a; }
EXTERN double uint_to_double(unsigned int a) { return (double) a; }
EXTERN double long_to_double(signed long a) { return (double) a; }
EXTERN double ulong_to_double(unsigned long a) { return (double) a; }
EXTERN double longlong_to_double(signed long long a) { return (double) a; }
EXTERN double ulonglong_to_double(unsigned long long a) { return (double) a; }
EXTERN double float_to_double(float a) { return (double) a; }
EXTERN double double_to_double(double a) { return a; }
EXTERN double pointer_to_double(void *a) { return (double)(uintptr_t)a; }
FNSTR2NUM(string_to_double, lf, double)
FNINOUT(double, double)
FNNUMERICARRAY(double, double)

EXTERN int pointer_in(int *pint) { return *pint; }
EXTERN void pointer_out(int **ppint)
{
    if (ppint == NULL)
        return;
    static int outint = 99;
    *ppint = &outint;
}
EXTERN void pointer_incr(char **pp) { *pp = *pp + 1; }
EXTERN void *pointer_byref(void **pp) { return *pp; }
EXTERN void ** pointer_ret_byref(void *p) {
    static void *static_p;
    static_p = p;
    return &static_p;
}
EXTERN void pointer_noop(void *p) {}
EXTERN void *pointer_to_pointer(void *p) { return p; }
EXTERN void *pointer_add(void *p, int n) {
    return n + (char *)p;
}
EXTERN void *pointer_errno(void *p) {
    errno = EINVAL;
    return p;
}
EXTERN void *pointer_reflect(void *in, void **outP) {
    if (outP)
        *outP = in;
    return in;
}
EXTERN int pointer_dispose(void *in, int ret) {
    return ret;
}
EXTERN int pointer_retval(void *in, void **out) {
    *out = (char*)in + 1;
    return 1;
}

#ifdef _WIN32
EXTERN void *pointer_lasterror(void *p)
{
    SetLastError(ERROR_INVALID_DATA);
    return p;
}
#endif

EXTERN void pointer_array_incr(char ** in, char **out, int n) {
    int i;
    for (i = 0; i < n; ++i)
        out[i] = in[i] + 1;
}

EXTERN void pointer_array_exchange(char ** inout1, char **inout2, int n) {
    int i;
    void *p;
    for (i = 0; i < n; ++i) {
        p = inout1[i];
        inout1[i] = inout2[i];
        inout2[i] = p;
    }
}

struct struct_with_pointer_array {
    char *ptrs[3];
};
EXTERN void struct_pointer_array_incr(struct struct_with_pointer_array *fromP, struct struct_with_pointer_array *toP) {
    pointer_array_incr(fromP->ptrs, toP->ptrs, 3);
}
EXTERN void struct_pointer_array_exchange(struct struct_with_pointer_array *a, struct struct_with_pointer_array *b) {
    pointer_array_exchange(a->ptrs, b->ptrs, 3);
}

static char utf8_test_string[] = {0xc3,0xa0,0xc3,0xa1,0xc3,0xa2, 0};
static char jis_test_string[]  = {'8', 'c', 0, 0};
static Tcl_UniChar unichar_test_string[] = {0xe0, 0xe1, 0xe2, 0};
static Tcl_UniChar unichar_test_string2[] = {0xe3, 0xe4, 0xe5, 0};
#ifdef _WIN32
static WCHAR winchar_test_string[] = {0xe0, 0xe1, 0xe2, 0};
static WCHAR winchar_test_string2[] = {0xe3, 0xe4, 0xe5, 0};
static WCHAR winchar_multisz_test_string[] = {
    'a', ' ', 'b', 0, 0xe0, 0xe1, 0, 'z', 0, 0};
#endif

FNSTRINGS(string, char)
EXTERN const char *ascii_return() {
    return "abc";
}
EXTERN const char **ascii_return_byref() {
    static const char *s = "abc";
    return &s;
}
EXTERN const char *utf8_return() {
    return utf8_test_string;
}
EXTERN const char **utf8_return_byref() {
    static const char *s = utf8_test_string;
    return &s;
}
EXTERN const char *jis0208_return() {
    return jis_test_string;
}
EXTERN const char **jis0208_return_byref() {
    static const char *s = jis_test_string;
    return &s;
}
EXTERN int string_param_out(char **strPP) {
    if (strPP == NULL)
        return 0;
    *strPP = "abc";
    return sizeof("abc") - 1;
}
EXTERN const char *string_array_in (const char *strings[], int index)
{
    return strings[index];
}
EXTERN int string_array_out (char *strings[], int n)
{
    int i;
    static char *strs[] = {"abc", "def", "ghi"};
    for (i = 0; i < n; ++i) {
        strings[i] = strs[i%3];
    }
    return n;
}

FNSTRINGS(unistring, Tcl_UniChar)
EXTERN const Tcl_UniChar *unistring_return() {
    return unichar_test_string;
}
EXTERN const Tcl_UniChar **unistring_return_byref() {
    static const Tcl_UniChar *s = unichar_test_string;
    return &s;
}
EXTERN int unistring_param_out(Tcl_UniChar **strPP) {
    if (strPP == NULL)
        return 0;
    *strPP = unichar_test_string;
    return (sizeof(unichar_test_string)/sizeof(unichar_test_string[0])) - 1;
}

EXTERN const Tcl_UniChar *unistring_array_in (const Tcl_UniChar *strings[], int index)
{
    return strings[index];
}
EXTERN int unistring_array_out (Tcl_UniChar *strings[], int n)
{
    int i;
    static Tcl_UniChar *strs[] = {unichar_test_string, unichar_test_string2};
    for (i = 0; i < n; ++i)
        strings[i] = strs[i%2];
    return n;
}

#ifdef _WIN32
FNSTRINGS(winstring, WCHAR)
EXTERN const WCHAR *winstring_return() {
    return winchar_test_string;
}
EXTERN const WCHAR **winstring_return_byref() {
    static const WCHAR *s = winchar_test_string;
    return &s;
}
EXTERN int winstring_param_out(WCHAR **strPP) {
    if (strPP == NULL)
        return 0;
    *strPP = winchar_test_string;
    return (sizeof(winchar_test_string)/sizeof(winchar_test_string[0])) - 1;
}

EXTERN const WCHAR *winstring_array_in (const WCHAR *strings[], int index)
{
    return strings[index];
}
EXTERN int winstring_array_out (WCHAR *strings[], int n)
{
    int i;
    static WCHAR *strs[] = {winchar_test_string, winchar_test_string2};
    for (i = 0; i < n; ++i)
        strings[i] = strs[i%2];
    return n;
}
EXTERN int winstring_multisz(WCHAR *in, int nout, WCHAR *out) {
    int count = 0;
    size_t totalLen = 0;
    size_t len;
    WCHAR *from = in;
    while ((len = wcslen(from)) != 0) {
        ++count;
        totalLen += len + 1;
        from += len+1;
    }
    if ((int) totalLen <= nout) {
        memmove(out, in, sizeof(WCHAR) * (totalLen+1));
    } else {
       if (nout) {
           *out = 0;
       }
    }
    return count;
}
EXTERN void winstring_multisz_param_out(WCHAR **out) {
    *out =  winchar_multisz_test_string;
}
EXTERN const WCHAR *winstring_multisz_return(WCHAR **out) {
    return winchar_multisz_test_string;
}
EXTERN const WCHAR *winstring_multisz_reflect(WCHAR *p) {
    return p;
}
EXTERN const WCHAR **winstring_multisz_return_byref(WCHAR **out) {
    static const WCHAR *s = winchar_multisz_test_string;
    return &s;
}


#endif

FNSTRINGS(binary, unsigned char)

EXTERN int jis0208_out(int bufsize, char *out)
{
    /* "8c" is the U+543e in jis0208 */
    int i, len;
    for (i = 0, len = 0; i < bufsize - 3; i += 2) {
        out[i]     = '8';
        out[i + 1] = 'c';
        ++len;
    }
    out[i] = '\0';
    out[i+1] = '\0';
    return len;
}

EXTERN int jis0208_in(char *in)
{
    /* "8c" is the U+543e in jis0208 */
    int len = 0;
    while (*in) {
        if (*in++ != '8')
            break;
        if (*in++ != 'c')
            break;
        ++len;
    }
    return len;
}

/* Caller responsible to ensure adequate memory! */
EXTERN int jis0208_inout(char *in)
{
    int len = (int) strlen(in);
    if (in[len+1] != 0) {
        printf("jis0208_inout assumption false.");
        exit(1);
    }
    memmove(in + len, in, len + 2); /* Two null bytes */
    return len;
}

EXTERN int bytes_out(int n, unsigned char *in, unsigned char *out)
{
    int i;
    if (out == NULL || in == NULL)
        return 0;
    for (i = 0; i < n; ++i)
        out[i] = in[i];
    return i;
}

EXTERN void bytes_inout(int n, unsigned char *inout)
{
    int i;
    unsigned char ch;
    if (inout == NULL)
        return;
    for (i = 0; i < n / 2; ++i) {
        ch       = inout[i];
        inout[i] = inout[n - i - 1];
        inout[n - i - 1] = ch;
    }
}

EXTERN void get_array_int(int *a, int n) {
    int i;
    for (i = 0; i < n; ++i)
        a[i] = 2*a[i];
}
EXTERN int getTestStructSize() { return (int)sizeof(TestStruct); }
EXTERN int getTestStruct(TestStruct *tsP)
{
    if (tsP == NULL)
        return 0;

    memset(tsP, 0, sizeof(*tsP));

#define OFF(type, field) (type) offsetof(TestStruct, field)
    tsP->c     = SCHAR_MIN;
    tsP->i     = INT_MIN;
    tsP->shrt  = SHRT_MIN;
    tsP->uint  = UINT_MAX;
    tsP->ushrt = USHRT_MAX;
    tsP->l     = LONG_MIN;
    tsP->uc    = UCHAR_MAX;
    tsP->ul    = ULONG_MAX;
    strncpy(tsP->chars, "CHARS", sizeof(tsP->chars));
    tsP->ll = LLONG_MIN;
    tsP->unic[0] = 'U';
    tsP->unic[1] = 'N';
    tsP->unic[2] = 'I';
    tsP->unic[3] = 'C';
    tsP->unic[4] = 0;
    tsP->ull     = ULLONG_MAX;
    tsP->b[0]    = 1;
    tsP->b[1]    = 2;
    tsP->b[2]    = 3;
    tsP->f       = -0.25;
    strncpy(tsP->s.c, "INNER", sizeof(tsP->s.c));
    tsP->d = 0.125;
#ifdef _WIN32
    lstrcpyW(tsP->wchars, L"WCHARS");
#endif
    return sizeof(*tsP);
}
EXTERN TestStruct  returnTestStruct() {
    struct TestStruct ts;
    getTestStruct(&ts);
    return ts;
}
EXTERN TestStruct *returnTestStructByRef() {
    static struct TestStruct ts;
    getTestStruct(&ts);
    return &ts;
}

/* Increments every numeric field value by 1 */
EXTERN void incrTestStruct(TestStruct *tsP)
{
    if (tsP == NULL)
        return;
#define SET(fld)                 \
    do {                         \
        tsP->fld = tsP->fld + 1; \
    } while (0)

    SET(c);
    SET(i);
    SET(shrt);
    SET(uint);
    SET(ushrt);
    SET(l);
    SET(uc);
    SET(ul);
    SET(ll);
    SET(ull);
    SET(f);
    SET(d);
#undef SET
}

/* Increments every numeric field value by 1 */
EXTERN void incrTestStructByVal(TestStruct from, TestStruct *tsP)
{
    if (tsP == NULL)
        return;
#define SET(fld)                 \
    do {                         \
        tsP->fld = from.fld + 1; \
    } while (0)

    SET(c);
    SET(i);
    SET(shrt);
    SET(uint);
    SET(ushrt);
    SET(l);
    SET(uc);
    SET(ul);
    SET(ll);
    SET(ull);
    SET(f);
    SET(d);

    memmove(tsP->s.c, from.s.c, sizeof(tsP->s.c));
    memmove(tsP->b, from.b, sizeof(tsP->b));
    memmove(tsP->chars, from.chars, sizeof(tsP->chars));
    memmove(tsP->unic, from.unic, sizeof(tsP->unic));
#ifdef _WIN32
    memmove(tsP->wchars, from.wchars, sizeof(tsP->wchars));
#endif
#undef SET
}

struct SimpleStruct {
    unsigned char c;
    long long ll;
    short s;
};
struct SimpleOuterStruct {
    float f;
    struct SimpleStruct s;
    char *p;
};

EXTERN struct SimpleOuterStruct
incrSimpleOuterStructByVal(struct SimpleOuterStruct outer)
{
    outer.f++;
    outer.s.c++;
    outer.s.ll++;
    outer.s.s++;
    outer.p++;
    return outer;
}

EXTERN int structArrayFill (int n, struct SimpleStruct *out)
{
    int i;
    int val = 0;
    for (i = 0; i < n; ++i) {
        out[i].c  = ++val;
        out[i].ll = ++val;
        out[i].s  = ++val;
    }
    return val;
}
EXTERN int structCheckByVal(struct SimpleStruct in, signed char c, long long ll, short s)
{
    return in.c == c && in.ll == ll && in.s == s;
}
EXTERN int structCheck(struct SimpleStruct *in, signed char c, long long ll, short s)
{
    return in->c == c && in->ll == ll && in->s == s;
}
EXTERN void structArrayCopy (struct SimpleStruct *fromP, struct SimpleStruct *toP, int n) {
    int i;
    for (i = 0; i < n; ++i) {
        toP[i] = fromP[i];
    }
}
EXTERN void structArrayExchange (struct SimpleStruct *a, struct SimpleStruct *b, int n) {
    int i;
    for (i = 0; i < n; ++i) {
        struct SimpleStruct c = a[i];
        a[i]           = b[i];
        b[i]           = c;
    }
}
struct StructWithSize {
    unsigned int cbSize;
    double value;
};
EXTERN unsigned int structGetSize (struct StructWithSize *a) {
    a->value = 42.0;
    return a->cbSize;
}
struct StructWithPointer {
    void *p;
};
EXTERN void makeStructWithPointer(struct StructWithPointer *strucP, void *p) {
    strucP->p = p;
}
EXTERN void *extractStructWithPointer(struct StructWithPointer *strucP) {
    return strucP->p;
}

struct StructWithNestedArrays {
    struct SimpleStruct s[3];
    void *p[3];
};
EXTERN void
nestedStructArrayCopy(struct StructWithNestedArrays *from,
                      struct StructWithNestedArrays *to)
{
    int i;
    structArrayCopy(from->s, to->s, 3);
    for (i = 0; i < 3; ++i)
        to->p[i] = from->p[i];
}

struct StructWithFunc {
    unsigned char c;
    void *fn;
};
EXTERN void *
getStructWithFunc(unsigned char c, void *in, struct StructWithFunc *out)
{
    out->fn = in;
    out->c  = c;
    return in;
}

struct StructWithStrings {
    char *s;
    char *utf8;
    char *jis;
    Tcl_UniChar *uni;
#ifdef _WIN32
    WCHAR *ws;
#endif
};

EXTERN int checkStructWithStrings(struct StructWithStrings *structP)
{
    int i;
    if (strcmp(structP->s, "abc"))
        return 1;
    if (strcmp(structP->utf8, utf8_test_string))
        return 2;
    /* Shift-jis is double null terminated */
    for (i = 0; i < sizeof(jis_test_string); ++i) {
        if (structP->jis[i] != jis_test_string[i])
            return 3;
    }
    for (i = 0;
         i < (sizeof(unichar_test_string) / sizeof(unichar_test_string[0]));
         ++i) {
        if (structP->uni[i] != unichar_test_string[i])
            return 4;
    }
#ifdef _WIN32
    for (i = 0;
         i < (sizeof(winchar_test_string) / sizeof(winchar_test_string[0]));
         ++i) {
        if (structP->uni[i] != winchar_test_string[i])
            return 5;
    }
#endif
    return 0;
}
EXTERN int checkStructWithStringsByVal(struct StructWithStrings s)
{
    return checkStructWithStrings(&s);
}
EXTERN void getStructWithStrings(struct StructWithStrings *sP) {
    sP->s = "abc";
    sP->utf8 = utf8_test_string;
    sP->jis  = jis_test_string;
    sP->uni  = unichar_test_string;
#ifdef _WIN32
    sP->ws   = winchar_test_string;
#endif
}
EXTERN struct StructWithStrings returnStructWithStrings() {
    struct StructWithStrings s = {
        "abc",
        utf8_test_string,
        jis_test_string,
        unichar_test_string,
#ifdef _WIN32
        winchar_test_string,
#endif

    };
    return s;
}
EXTERN void getStructWithNullStrings(struct StructWithStrings *sP) {
    sP->s    = NULL;
    sP->utf8 = NULL;
    sP->jis  = NULL;
    sP->uni  = NULL;
#ifdef _WIN32
    sP->ws  = NULL;
#endif
}

struct StructWithStringArrays {
    char *strings[3];
    Tcl_UniChar *unistrings[3];
#ifdef _WIN32
    WCHAR *winstrings[3];
#endif
};

EXTERN void
getStringFromStructStringArray(const struct StructWithStringArrays *structP,
                               int i,
                               char **stringP,
                               Tcl_UniChar **unistringP
#ifdef _WIN32
                               ,
                               WCHAR **winstringP
#endif
)
{
    *stringP = structP->strings[i];
    *unistringP = structP->unistrings[i];
#ifdef _WIN32
    *winstringP = structP->winstrings[i];
#endif
}
EXTERN void
getStringFromStructByvalStringArray(struct StructWithStringArrays s,
                                    int i,
                                    char **stringP,
                                    Tcl_UniChar **unistringP
#ifdef _WIN32
                                    ,
                                    WCHAR **winstringP
#endif
)
{
    *stringP = s.strings[i];
    *unistringP = s.unistrings[i];
#ifdef _WIN32
    *winstringP = s.winstrings[i];
#endif
}

struct StructWithVLA {
    unsigned short count;
    int values[1];
};
EXTERN int copyStructWithVLA (struct StructWithVLA *from, struct StructWithVLA *to)
{
    int i;
    int sum;
    if (to) {
        to->count = from->count;
    }
    for (sum =0, i = 0; i < from->count; ++i) {
        if (to) {
            to->values[i] = from->values[i];
        }
        sum += from->values[i];
    }
    return sum;
}

EXTERN void
getEinvalString(char *bufP)
{
    strcpy(bufP, strerror(EINVAL));
}

EXTERN int setErrno(int i)
{
    errno = EINVAL;
    return i;
}

#ifdef _WIN32
EXTERN int setWinError(int i)
{
    SetLastError(ERROR_INVALID_DATA);
    return i;
}
#endif

/* Callback test functions */

EXTERN void noargs_caller(void (*fnptr)(void)) {
    fnptr();
}

#define FNCALLBACK(token_, type_) \
typedef type_ (* token_##callback)(type_); \
EXTERN type_ token_ ## _fn_caller (type_ val, token_ ## callback cb_fn) { \
    return cb_fn(val); \
} \
typedef type_ (* token_##callback_byref)(type_ *); \
EXTERN type_ token_ ## _fn_caller_byref (type_ val, token_ ## callback_byref cb_fn) { \
    return cb_fn(&val); \
}

FNCALLBACK(schar, signed char)
FNCALLBACK(uchar, unsigned char)
FNCALLBACK(short, short)
FNCALLBACK(ushort, unsigned short)
FNCALLBACK(int, int)
FNCALLBACK(uint, unsigned int)
FNCALLBACK(long, long)
FNCALLBACK(ulong, unsigned long)
FNCALLBACK(longlong, long long)
FNCALLBACK(ulonglong, unsigned long long)
FNCALLBACK(float, float)
FNCALLBACK(double, double)
FNCALLBACK(pointer, void*)

EXTERN int callback_check_byref(void *p, int (*fn)(void *)) {
    return fn(p);
}


EXTERN
double
manyargs_callback(signed char ch,
                  unsigned char uch,
                  short shrt,
                  unsigned short ushort,
                  int i,
                  unsigned int ui,
                  long l,
                  unsigned long ul,
                  long long ll,
                  unsigned long long ull,
                  float f,
                  double d,
                  double (*fn)(signed char ch,
                               unsigned char uch,
                               short shrt,
                               unsigned short ushort,
                               int i,
                               unsigned int ui,
                               long l,
                               unsigned long ul,
                               long long ll,
                               unsigned long long ull,
                               float f,
                               double d))
{
    return fn(ch, uch, shrt, ushort, i, ui, l, ul, ll, ull, f, d);
}

DLLEXPORT
double CFFI_STDCALL
manyargs_callback_stdcall(signed char ch,
                  unsigned char uch,
                  short shrt,
                  unsigned short ushort,
                  int i,
                  unsigned int ui,
                  long l,
                  unsigned long ul,
                  long long ll,
                  unsigned long long ull,
                  float f,
                  double d,
                  double (CFFI_STDCALL * fn)(signed char ch,
                               unsigned char uch,
                               short shrt,
                               unsigned short ushort,
                               int i,
                               unsigned int ui,
                               long l,
                               unsigned long ul,
                               long long ll,
                               unsigned long long ull,
                               float f,
                               double d))
{
    return fn(ch, uch, shrt, ushort, i, ui, l, ul, ll, ull, f, d);
}

DLLEXPORT
int callback_int2 (int i, int j, int(*fn)(int, int)) {
    return fn(i, j);
}

DLLEXPORT
int formatVarargs(char *buf, int bufSize, const char *fmt,...)
{
    int n;
    va_list args;
    va_start(args, fmt);
    n = vsnprintf(buf, bufSize, fmt, args);
    va_end(args);
    return n;
}

/*
 * Variable size struct tests
 */
#define DefineVarsizeStruct(countType_, valueType_)                        \
    typedef struct {                                                       \
        countType_ count;                                                  \
        valueType_ values[1];                                              \
    } StructWithVLA##countType_##valueType_;                               \
                                                                           \
    typedef struct {                                                       \
        unsigned short shrt;                                               \
        StructWithVLA##countType_##valueType_ nested;                      \
    } StructWithNestedVLA##countType_##valueType_;                         \
                                                                           \
    DLLEXPORT void copyVarSizeStruct##countType_##valueType_(              \
        StructWithVLA##countType_##valueType_ *in,                         \
        StructWithVLA##countType_##valueType_ *inout)                      \
    {                                                                      \
        long long i;                                                       \
        countType_ count = in->count;                                      \
        if (inout->count < in->count)                                      \
            count = inout->count;                                          \
        for (i = 0; i < count; ++i)                                        \
            inout->values[i] = (valueType_)(intptr_t) ((long long)in->values[i] + 1); \
    }                                                                      \
    DLLEXPORT void copyNestedVarSizeStruct##countType_##valueType_(        \
        StructWithNestedVLA##countType_##valueType_ *in,                   \
        StructWithNestedVLA##countType_##valueType_ *inout)                \
    {                                                                      \
        long long i;                                                       \
        countType_ count = in->nested.count;                               \
        inout->shrt      = in->shrt+1;                                       \
        if (inout->nested.count < in->nested.count)                        \
            count = inout->nested.count;                                   \
        for (i = 0; i < count; ++i)                                        \
            inout->nested.values[i] =                                      \
                (valueType_)(intptr_t)((long long)in->nested.values[i] + 1);         \
    }

typedef void *voidpointer;
DefineVarsizeStruct(int, int)
DefineVarsizeStruct(char, double)
DefineVarsizeStruct(int64_t, char)
DefineVarsizeStruct(int, voidpointer)

union TestUnion {
    int i;
    double dbl;
    unsigned char uc;
};
struct StructWithUnion {
    int tag;
    union TestUnion u;
};
DLLEXPORT void incrStructWithUnion(struct StructWithUnion in, struct StructWithUnion *out) {
    switch (in.tag) {
    case 0:
        out->u.i = in.u.i + 1;
        break;
    case 1:
        out->u.dbl = in.u.dbl + 1;
        break;
    case 2:
        out->u.uc = in.u.uc + 1;
        break;
    default:
        memset(out, 0, sizeof(*out));
        break;
    }
    out->tag = in.tag;
}
DLLEXPORT void incrUnion(int tag, union TestUnion *inout) {
    switch (tag) {
    case 0:
        inout->i += 1;
        break;
    case 1:
        inout->dbl += 1;
        break;
    case 2:
        inout->uc += 1;
        break;
    default:
        memset(inout, 0, sizeof(*inout));
        break;
    }
}

#if defined(_MSC_VER) || defined(__GNUC__)
#define FNPACK(pack_)  \
typedef struct StructPack ## pack_ { \
    unsigned char uc; \
    double dbl; \
    short s; \
} StructPack ## pack_; \
DLLEXPORT int modifyStructPack ## pack_(struct StructPack ## pack_ *inout) \
{ \
    int off; \
    /* \
     * Use a temporary because either of the following causes MSVC to bugcheck \
     * inout->uc += offsetof(StructPack1, uc); \
     * inout->uc = inout->uc + offsetof(StructPack1, uc); \
     */ \
    off = offsetof(StructPack ## pack_, uc); \
    inout->uc  = inout->uc + off; \
    off = offsetof(StructPack ## pack_, dbl); \
    inout->dbl = inout->dbl + off; \
    off = offsetof(StructPack ## pack_, s); \
    inout->s   = inout->s + off; \
    return sizeof(*inout); \
}

#pragma pack(1)
FNPACK(1)
#pragma pack(2)
FNPACK(2)
#pragma pack(4)
FNPACK(4)
#pragma pack()

#endif

struct BaseInterface;
struct DerivedInterface;

int BaseInterfaceGet(struct BaseInterface *);
int BaseInterfaceSet(struct BaseInterface *, int a);
void BaseInterfaceDelete(struct BaseInterface *);
int DerivedInterfaceSetMax(struct DerivedInterface *tiP, int a, int b);

typedef struct BaseInterfaceVtable {
    int (*get)(struct BaseInterface *);
    int (*set)(struct BaseInterface *, int a);
    void (*delete)(struct BaseInterface *);
} BaseInterfaceVtable;

BaseInterfaceVtable baseVtable = {
    BaseInterfaceGet,
    BaseInterfaceSet,
    BaseInterfaceDelete
};

typedef struct DerivedInterfaceVtable {
    BaseInterfaceVtable base;
    int (*setmax)(struct DerivedInterface *, int a, int b);
} DerivedInterfaceVtable;

DerivedInterfaceVtable derivedVtable = {
    {BaseInterfaceGet, BaseInterfaceSet, BaseInterfaceDelete},
    DerivedInterfaceSetMax};

typedef struct BaseInterface {
    BaseInterfaceVtable *vtable;
    int baseValue;
} BaseInterface;

typedef struct DerivedInterface {
    DerivedInterfaceVtable *vtable;
    int baseValue;
} DerivedInterface;

DLLEXPORT BaseInterface *BaseInterfaceNew(int val) {
    struct BaseInterface *tiP;
    tiP = (struct BaseInterface *) malloc(sizeof(*tiP));
    tiP->vtable = &baseVtable;
    tiP->baseValue = val;
    return tiP;
}
int BaseInterfaceGet(struct BaseInterface *tiP)
{
    return tiP->baseValue;
}
int BaseInterfaceSet(struct BaseInterface *tiP, int newValue)
{
    int old = tiP->baseValue;
    tiP->baseValue = newValue;
    return old;
}
void BaseInterfaceDelete(struct BaseInterface *tiP)
{
    free(tiP);
}

DLLEXPORT DerivedInterface *DerivedInterfaceNew(int val) {
    struct DerivedInterface *tiP;
    tiP = (struct DerivedInterface *) malloc(sizeof(*tiP));
    tiP->vtable = &derivedVtable;
    tiP->baseValue = val;
    return tiP;
}

int DerivedInterfaceSetMax(struct DerivedInterface *tiP, int a, int b)
{
    int old  = tiP->baseValue;
    tiP->baseValue = a > b ? a : b;
    return old;
}

/* Stdcall versions */
struct BaseInterfaceStdcall;
int CFFI_STDCALL BaseInterfaceGetStdcall(struct BaseInterfaceStdcall *);
int CFFI_STDCALL BaseInterfaceSetStdcall(struct BaseInterfaceStdcall *, int a);
void CFFI_STDCALL BaseInterfaceDeleteStdcall(struct BaseInterfaceStdcall *);

typedef struct BaseInterfaceVtableStdcall {
    int (CFFI_STDCALL * get)(struct BaseInterfaceStdcall *);
    int (CFFI_STDCALL * set)(struct BaseInterfaceStdcall *, int a);
    void (CFFI_STDCALL * delete)(struct BaseInterfaceStdcall *);
} BaseInterfaceVtableStdcall;

BaseInterfaceVtableStdcall baseVtableStdcall = {
    BaseInterfaceGetStdcall,
    BaseInterfaceSetStdcall,
    BaseInterfaceDeleteStdcall
};

typedef struct BaseInterfaceStdcall {
    BaseInterfaceVtableStdcall *vtable;
    int baseValue;
} BaseInterfaceStdcall;

DLLEXPORT BaseInterfaceStdcall * CFFI_STDCALL BaseInterfaceNewStdcall(int val) {
    struct BaseInterfaceStdcall *tiP;
    tiP = (struct BaseInterfaceStdcall *) malloc(sizeof(*tiP));
    tiP->vtable = &baseVtableStdcall;
    tiP->baseValue = val;
    return tiP;
}
int CFFI_STDCALL BaseInterfaceGetStdcall(struct BaseInterfaceStdcall *tiP)
{
    return tiP->baseValue;
}
int CFFI_STDCALL BaseInterfaceSetStdcall(struct BaseInterfaceStdcall *tiP, int newValue)
{
    int old = tiP->baseValue;
    tiP->baseValue = newValue;
    return old;
}
void CFFI_STDCALL BaseInterfaceDeleteStdcall(struct BaseInterfaceStdcall *tiP)
{
    free(tiP);
}

DLLEXPORT void copyUuid(const UUID *from, UUID *to)
{
    if (from)
        *to = *from;
    else
        memset(to, 0, sizeof(*to));
}

DLLEXPORT void incrUuid(UUID *uuidP)
{
    unsigned char *p = (unsigned char *)uuidP;
    p[0] += 1;
    p[4] += 1;
    p[6] += 1;
    p[15] += 1;
}

DLLEXPORT void reverseUuidArray(int n, const UUID *from, UUID *to)
{
    if (from == NULL || n <= 0)
        return;

    int i;
    for (i = 0; i < n; ++i) {
        to[n-i-1] = from[i];
    }
}

