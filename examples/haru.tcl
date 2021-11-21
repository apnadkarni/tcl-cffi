# Sample to generate a PDF from a PNG using haru library
#  haru.tcl input.png output.pdf

package require cffi

cffi::dyncall::Library create HPDF libhpdf.[info sharedlibextension]

# Define some aliases. Note aliases are never necessary. They're there only
# to improve readability and reduce typing.
cffi::alias define HPDF_STATUS ulong
cffi::alias define HPDF_REAL   float
cffi::alias define HPDF_Doc    pointer.HPDF_Doc
cffi::alias define HPDF_Page   pointer.HPDF_Page
cffi::alias define HPDF_Image  pointer.HPDF_Image

# The HPDF_New function takes a callback function and structure. This is neither
# necessary nor desirable as it uses setjmp/longjmp under the covers. We will
# set it to NULL and use plain old C style error checking. Setting default as
# NULL means the arguments do not need to be explicitly specified on the call.
HPDF stdcall HPDF_New {HPDF_Doc nonzero} {
    user_error_fn {pointer {default NULL}}
    user_error_data {pointer {default NULL}}
}

# The HPDF_Free function frees resources allocated for a HPDF_Doc. The corresponding
# HPDF_Doc must NOT be accessed again. Accordingly we add a dispose annotation
# which tells CFFI the function frees the handle/pointer. An attempt to use it
# again by mistake will raise a Tcl exception rather than crashing the program.
HPDF stdcall HPDF_Free void {
    hpdf {HPDF_Doc dispose}
}

HPDF stdcall HPDF_SaveToFile HPDF_STATUS {
    hpdf HPDF_Doc
    pdf_path string
}
HPDF stdcall HPDF_LoadPngImageFromFile HPDF_Image {
    hpdf HPDF_Doc
    png_path string
}
HPDF stdcall HPDF_GetError HPDF_STATUS {
    hpdf HPDF_Doc
}
HPDF stdcall HPDF_ResetError void {
    hpdf HPDF_Doc
}
HPDF stdcall HPDF_AddPage {HPDF_Page nonzero} {
    hpdf HPDF_Doc
}
HPDF stdcall HPDF_Page_SetWidth HPDF_STATUS {
    hpage HPDF_Page
    width HPDF_REAL
}
HPDF stdcall HPDF_Page_SetHeight HPDF_STATUS {
    hpage HPDF_Page height HPDF_REAL
}
HPDF stdcall HPDF_Page_DrawImage HPDF_STATUS {
    hpage HPDF_Page
    himg HPDF_Image
    x HPDF_REAL
    y HPDF_REAL
    width HPDF_REAL 
    height HPDF_REAL
}

HPDF stdcall HPDF_Image_GetHeight {uint nonzero} {himg HPDF_Image}
HPDF stdcall HPDF_Image_GetWidth {uint nonzero} {himg HPDF_Image}

proc error_exit {msg} {
    puts stderr $msg
    exit 
}

if {[llength $::argv] != 2} {
    error_exit "Syntax: [info script] PNGINFILE PDFOUTFILE"
}

set hpdf [HPDF_New];# Let the two parameters default to NULL
if {[cffi::pointer isnull $hpdf]} {
    # NOTE: CANNOT use HPDF_GetError here since that requires a HPDF
    error_exit "Could not create PDF document."
}

set himg [HPDF_LoadPngImageFromFile $hpdf [lindex $::argv 0]]
if {[cffi::pointer isnull $himg]} {
    set error_code [HPDF_GetError $hpdf]
    error_exit "Could not load PNG, error=$error_code."
}
set width [HPDF_Image_GetWidth $himg]
set height [HPDF_Image_GetHeight $himg]

set hpage [HPDF_AddPage $hpdf]
if {[cffi::pointer isnull $hpage]} {
    set error_code [HPDF_GetError $hpdf]
    error_exit "Could not create Page, error=$error_code."
}

# Make page same size as icon
HPDF_Page_SetWidth $hpage $width
HPDF_Page_SetHeight $hpage $height

# Draw the image
set error_code [HPDF_Page_DrawImage $hpage $himg 0 0 $width $height]
if {$error_code != 0} {
    error_exit "Could not draw image, error=$error_code"
}

# Save the PDF
set error_code [HPDF_SaveToFile $hpdf [lindex $::argv 1]]
if {$error_code != 0} {
    error_exit "Could not save file, error=$error_code"
}

# Finally, free up resources. Not really necessary since exiting anyways but ...
HPDF_Free $hpdf

