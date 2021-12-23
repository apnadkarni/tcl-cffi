# Sample to generate a PDF from a PNG using haru library
#  haru.tcl input.png output.pdf

package require cffi

cffi::Wrapper create HPDF libhpdf[info sharedlibextension]

# Define some aliases. Note aliases are never necessary. They're there only
# to improve readability and reduce typing.
cffi::alias define HPDF_REAL   float
cffi::alias define HPDF_Doc    pointer.HPDF_Doc


# Non-zero HPDF_STATUS values signal an error.
cffi::alias define HPDF_STATUS_UNCHECKED ulong
cffi::alias define HPDF_STATUS {HPDF_STATUS_UNCHECKED zero}

HPDF stdcall HPDF_GetError HPDF_STATUS_UNCHECKED {
    hpdf HPDF_Doc
}
proc HPDFNullHandler {callinfo} {
    # The $callinfo dictionary contains the information about call failure.
    # This handler is intended to be used for functions that take a HPDF_Doc
    # as an input parameter and return another handle which is NULL on failure.
    # The detail error code has then to be retrieved from the HPDF_Doc. By
    # *convention* the input parameter holding the HPDF_Doc handle should be
    # called hpdf so this handler knows how to get hold of it.

    # The In element of the $callinfo dictionary holds input arguments.
    if {[dict exists $callinfo In hpdf]} {
        set hpdf [dict get $callinfo In hpdf]
        # Ensure the HPDF_Doc is itself not NULL.
        if {! [cffi::pointer isnull $hpdf]} {
            set error_status [HPDF_GetError $hpdf]
            catch {HPDF_ResetError $hpdf}
            if {[dict exists $callinfo Command]} {
                throw {HPDFStatus $error_status} "Error code $error_status returned from function [dict get $callinfo Command]."
            } else {
                throw {HPDFStatus $error_status} "Error code $error_status returned from function."
            }
        }
    }
    # Fallback in cases where the hpdf name convention not followed or hpdf
    # itself is NULL. If the Command key exists, it is the name of the
    # function that was called.
    if {[dict exists $callinfo Command]} {
        throw HPDFNull "Null pointer returned from function [dict get $callinfo Command]."
    } else {
        throw HPDFNull "Null pointer returned from function."
    }
}

# All handles that are "owned" by the document object are marked as unsafe
# because they are not to be explicitly freed. This loses us some protection
# against reusing a owned pointer when the owning pointer is released but
# that's life. Note checks for NULL will still be in effect as we do not
# specify the nullok annotation.
cffi::alias define HPDF_Page   {pointer.HPDF_Page unsafe}
cffi::alias define HPDF_Image  {pointer.HPDF_Image unsafe {onerror HPDFNullHandler}}

# Notes on the functions below:
#
# HPDF_New - takes a callback function and structure. This is neither necessary
# nor desirable as it uses setjmp/longjmp under the covers. We will set it to
# NULL and use plain old C style error checking. Setting default as NULL means
# the arguments do not need to be explicitly specified on the call. Note the use
# of nullok as otherwise the NULL pointers will be rejected!
#
# HPDF_Free - frees resources allocated for a HPDF_Doc. The corresponding
# HPDF_Doc must NOT be accessed again. Accordingly we add a dispose annotation
# which tells CFFI the function frees the handle/pointer. An attempt to use it
# again by mistake will raise a Tcl exception rather than crashing the program.

HPDF stdcalls {
    HPDF_New HPDF_Doc {
        user_error_fn {pointer {default NULL} nullok}
        user_error_data {pointer {default NULL} nullok}
    }
    HPDF_Free void {
        hpdf {HPDF_Doc dispose}
    }
    HPDF_SaveToFile HPDF_STATUS {
        hpdf HPDF_Doc
        pdf_path string
    }
    HPDF_LoadPngImageFromFile HPDF_Image {
        hpdf HPDF_Doc
        png_path string
    }
    HPDF_ResetError void {
        hpdf HPDF_Doc
    }
    HPDF_AddPage HPDF_Page {
        hpdf HPDF_Doc
    }
    HPDF_Page_SetWidth HPDF_STATUS {
        hpage HPDF_Page
        width HPDF_REAL
    }
    HPDF_Page_SetHeight HPDF_STATUS {
        hpage HPDF_Page height HPDF_REAL
    }
    HPDF_Page_DrawImage HPDF_STATUS {
        hpage HPDF_Page
        himg HPDF_Image
        x HPDF_REAL
        y HPDF_REAL
        width HPDF_REAL 
        height HPDF_REAL
    }
    HPDF_Image_GetHeight {uint nonzero} {himg HPDF_Image}
    HPDF_Image_GetWidth {uint nonzero} {himg HPDF_Image}
}


proc error_exit {msg} {
    puts stderr $msg
    exit 
}

if {[llength $::argv] != 2} {
    error_exit "Syntax: [info script] PNGINFILE PDFOUTFILE"
}
try {
    set hpdf [HPDF_New];# Let the two parameters default to NULL
    set himg [HPDF_LoadPngImageFromFile $hpdf [lindex $::argv 0]]
    set width [HPDF_Image_GetWidth $himg]
    set height [HPDF_Image_GetHeight $himg]
    set hpage [HPDF_AddPage $hpdf]

    # Make page same size as icon
    HPDF_Page_SetWidth $hpage $width
    HPDF_Page_SetHeight $hpage $height

    # Draw the image
    HPDF_Page_DrawImage $hpage $himg 0 0 $width $height

    # Save the PDF
    HPDF_SaveToFile $hpdf [lindex $::argv 1]

    # Finally, free up resources. Not really necessary since exiting anyways but ...
    HPDF_Free $hpdf
} on error {result} {
    error_exit $result
}
