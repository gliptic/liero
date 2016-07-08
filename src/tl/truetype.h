// stb_truetype.h - v0.3 - public domain - 2009 Sean Barrett / RAD Game Tools
//
//   This library processes TrueType files:
//        parse files
//        extract glyph metrics
//        extract glyph shapes
//        render glyphs to one-channel bitmaps with antialiasing (box filter)
//
//   Todo:
//        non-MS cmaps
//        crashproof on bad data
//        hinting
//        subpixel positioning when rendering bitmap
//        cleartype-style AA
//
// ADDITIONAL CONTRIBUTORS
//
//   Mikko Mononen: compound shape support, more cmap formats
//
// VERSIONS
//
//   0.3 (2009-06-24) cmap fmt=12, compound shapes (MM)
//                    userdata, malloc-from-userdata, non-zero fill (STB)
//   0.2 (2009-03-11) Fix unsigned/signed char warnings
//   0.1 (2009-03-09) First public release
//
// USAGE
//
//   Include this file in whatever places neeed to refer to it. In ONE C/C++
//   file, write:
//      #define STB_TRUETYPE_IMPLEMENTATION
//   before the #include of this file. This expands out the actual
//   implementation into that C/C++ file.
//
//   Look at the header-file sections below for the API, but here's a quick skim:
//
//   Simple 3D API (don't ship this, but it's fine for tools and quick start,
//                  and you can cut and paste from it to move to more advanced)
//           tl_tt_BakeFontBitmap()               -- bake a font to a bitmap for use as texture
//           tl_tt_GetBakedQuad()                 -- compute quad to draw for a given char
//
//   "Load" a font file from a memory buffer (you have to keep the buffer loaded)
//           tl_tt_InitFont()
//           tl_tt_GetFontOffsetForIndex()        -- use for TTC font collections
//
//   Render a unicode codepoint to a bitmap
//           tl_tt_GetCodepointBitmap()           -- allocates and returns a bitmap
//           tl_tt_MakeCodepointBitmap()          -- renders into bitmap you provide
//           tl_tt_GetCodepointBitmapBox()        -- how big the bitmap must be
//
//   Character advance/positioning
//           tl_tt_GetCodepointHMetrics()
//           tl_tt_GetFontVMetrics()
//
// NOTES
//
//   The system uses the raw data found in the .ttf file without changing it
//   and without building auxiliary data structures. This is a bit inefficient
//   on little-endian systems (the data is big-endian), but assuming you're
//   caching the bitmaps or glyph shapes this shouldn't be a big deal.
//
//   It appears to be very hard to programmatically determine what font a
//   given file is in a general way. I provide an API for this, but I don't
//   recommend it.
//
//
// SOURCE STATISTICS (based on v0.3, 1800 LOC)
//
//   Documentation & header file        350 LOC  \___ 500 LOC documentation
//   Sample code                        140 LOC  /
//   Truetype parsing                   580 LOC  ---- 600 LOC TrueType
//   Software rasterization             240 LOC  \                           .
//   Curve tesselation                  120 LOC   \__ 500 LOC Bitmap creation
//   Bitmap management                   70 LOC   /
//   Baked bitmap interface              70 LOC  /
//   Font name matching & access        150 LOC  ---- 150
//   C runtime library abstraction       60 LOC  ----  60

#include "config.h"
#include "image.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
////
////   INTEGRATION WITH RUNTIME LIBRARIES
////

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
////
////   INTERFACE
////
////

#ifndef __STB_INCLUDE_STB_TRUETYPE_H__
#define __STB_INCLUDE_STB_TRUETYPE_H__

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////////
//
// TEXTURE BAKING API
//
// If you use this API, you only have to call two functions ever.
//

typedef struct
{
   unsigned short x0,y0,x1,y1; // coordinates of bbox in bitmap
   float xoff,yoff,xadvance;
} tl_tt_bakedchar;

extern int tl_tt_BakeFontBitmap(const unsigned char *data, int offset,  // font location (use offset=0 for plain .ttf)
                                float pixel_height,                     // height of font in pixels
                                unsigned char *pixels, int pw, int ph,  // bitmap to be filled in
                                int first_char, int num_chars,          // characters to bake
                                tl_tt_bakedchar *chardata);             // you allocate this, it's num_chars long
// if return is positive, the first unused row of the bitmap
// if return is negative, returns the negative of the number of characters that fit
// if return is 0, no characters fit and no rows were used
// This uses a very crappy packing.

typedef struct
{
   float x0,y0,s0,t0; // top-left
   float x1,y1,s1,t1; // bottom-right
} tl_tt_aligned_quad;

extern void tl_tt_GetBakedQuad(tl_tt_bakedchar *chardata, int pw, int ph,  // same data as above
                               int char_index,             // character to display
                               float *xpos, float *ypos,   // pointers to current position in screen pixel space
                               tl_tt_aligned_quad *q,      // output: quad to draw
                               int opengl_fillrule);       // true if opengl fill rule; false if DX9 or earlier
// Call GetBakedQuad with char_index = 'character - first_char', and it
// creates the quad you need to draw and advances the current position.
// It's inefficient; you might want to c&p it and optimize it.


//////////////////////////////////////////////////////////////////////////////
//
// FONT LOADING
//
//

TL_TT_API int tl_tt_GetFontOffsetForIndex(const unsigned char *data, int index);
// Each .ttf file may have more than one font. Each has a sequential index
// number starting from 0. Call this function to get the font offset for a
// given index; it returns -1 if the index is out of range. A regular .ttf
// file will only define one font and it always be at offset 0, so it will
// return '0' for index 0, and -1 for all other indices. You can just skip
// this step if you know it's that kind of font.


// The following structure is defined publically so you can declare one on
// the stack or as a global or etc.
typedef struct
{
   void           *userdata;
   unsigned char  *data;         // pointer to .ttf file
   int             fontstart;    // offset of start of font

   int numGlyphs;                // number of glyphs, needed for range checking

   int loca,head,glyf,hhea,hmtx; // table locations as offset from start of .ttf
   int index_map;                // a cmap mapping for our chosen character encoding
   int indexToLocFormat;         // format needed to map from glyph index to glyph
} tl_tt_fontinfo;

TL_TT_API int tl_tt_InitFont(tl_tt_fontinfo *info, const unsigned char *data, int offset);
// Given an offset into the file that defines a font, this function builds
// the necessary cached info for the rest of the system. You must allocate
// the tl_tt_fontinfo yourself, and tl_tt_InitFont will fill it out. You don't
// need to do anything special to free it, because the contents are a pure
// cache with no additional data structures. Returns 0 on failure.


//////////////////////////////////////////////////////////////////////////////
//
// CHARACTER TO GLYPH-INDEX CONVERSIOn

TL_TT_API int tl_tt_FindGlyphIndex(const tl_tt_fontinfo *info, int unicode_codepoint);
// If you're going to perform multiple operations on the same character
// and you want a speed-up, call this function with the character you're
// going to process, then use glyph-based functions instead of the
// codepoint-based functions.


//////////////////////////////////////////////////////////////////////////////
//
// CHARACTER PROPERTIES
//

TL_TT_API float tl_tt_ScaleForPixelHeight(const tl_tt_fontinfo *info, float pixels);
// computes a scale factor to produce a font whose "height" is 'pixels' tall.
// Height is measured as the distance from the highest ascender to the lowest
// descender; in other words, it's equivalent to calling tl_tt_GetFontVMetrics
// and computing:
//       scale = pixels / (ascent - descent)
// so if you prefer to measure height by the ascent only, use a similar calculation.

TL_TT_API void tl_tt_GetFontVMetrics(const tl_tt_fontinfo *info, int *ascent, int *descent, int *lineGap);
// ascent is the coordinate above the baseline the font extends; descent
// is the coordinate below the baseline the font extends (i.e. it is typically negative)
// lineGap is the spacing between one row's descent and the next row's ascent...
// so you should advance the vertical position by "*ascent - *descent + *lineGap"
//   these are expressed in unscaled coordinates

TL_TT_API void tl_tt_GetCodepointHMetrics(const tl_tt_fontinfo *info, int codepoint, int *advanceWidth, int *leftSideBearing);
// leftSideBearing is the offset from the current horizontal position to the left edge of the character
// advanceWidth is the offset from the current horizontal position to the next horizontal position
//   these are expressed in unscaled coordinates

TL_TT_API int  tl_tt_GetCodepointKernAdvance(const tl_tt_fontinfo *info, int ch1, int ch2);
// an additional amount to add to the 'advance' value between ch1 and ch2
// @TODO; for now always returns 0!

TL_TT_API int tl_tt_GetCodepointBox(const tl_tt_fontinfo *info, int codepoint, int *x0, int *y0, int *x1, int *y1);
// Gets the bounding box of the visible part of the glyph, in unscaled coordinates

TL_TT_API void tl_tt_GetGlyphHMetrics(const tl_tt_fontinfo *info, int glyph_index, int *advanceWidth, int *leftSideBearing);
TL_TT_API int  tl_tt_GetGlyphKernAdvance(const tl_tt_fontinfo *info, int glyph1, int glyph2);
TL_TT_API int  tl_tt_GetGlyphBox(const tl_tt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1);
// as above, but takes one or more glyph indices for greater efficiency


//////////////////////////////////////////////////////////////////////////////
//
// GLYPH SHAPES (you probably don't need these, but they have to go before
// the bitmaps for C declaration-order reasons)
//

#ifndef STBTT_vmove // you can predefine these to use different values (but why?)
   enum {
      STBTT_vmove=1,
      STBTT_vline,
      STBTT_vcurve
   };
#endif

#ifndef tl_tt_vertex // you can predefine this to use different values
                   // (we share this with other code at RAD)
   #define tl_tt_vertex_type short // can't use tl_tt_int16 because that's not visible in the header file
   typedef struct
   {
      tl_tt_vertex_type x,y,cx,cy;
      unsigned char type,padding;
   } tl_tt_vertex;
#endif

TL_TT_API int tl_tt_GetCodepointShape(const tl_tt_fontinfo *info, int unicode_codepoint, tl_tt_vertex **vertices);
TL_TT_API int tl_tt_GetGlyphShape(const tl_tt_fontinfo *info, int glyph_index, tl_tt_vertex **vertices);
// returns # of vertices and fills *vertices with the pointer to them
//   these are expressed in "unscaled" coordinates

TL_TT_API void tl_tt_FreeShape(const tl_tt_fontinfo *info, tl_tt_vertex *vertices);
// frees the data allocated above

//////////////////////////////////////////////////////////////////////////////
//
// BITMAP RENDERING
//

TL_TT_API void tl_tt_FreeBitmap(unsigned char *bitmap, void *userdata);
// frees the bitmap allocated below

TL_TT_API tl_image tl_tt_GetCodepointBitmap(const tl_tt_fontinfo *info, float scale_x, float scale_y, int codepoint, int *xoff, int *yoff);
// allocates a large-enough single-channel 8bpp bitmap and renders the
// specified character/glyph at the specified scale into it, with
// antialiasing. 0 is no coverage (transparent), 255 is fully covered (opaque).
// *width & *height are filled out with the width & height of the bitmap,
// which is stored left-to-right, top-to-bottom.
//
// xoff/yoff are the offset it pixel space from the glyph origin to the top-left of the bitmap

TL_TT_API void tl_tt_MakeCodepointBitmap(const tl_tt_fontinfo *info, tl_image* output, float scale_x, float scale_y, int codepoint);
// the same as above, but you pass in storage for the bitmap in the form
// of 'output', with row spacing of 'out_stride' bytes. the bitmap is
// clipped to out_w/out_h bytes. call the next function to get the
// height and width and positioning info

TL_TT_API void tl_tt_GetCodepointBitmapBox(const tl_tt_fontinfo *font, int codepoint, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1);
// get the bbox of the bitmap centered around the glyph origin; so the
// bitmap width is ix1-ix0, height is iy1-iy0, and location to place
// the bitmap top left is (leftSideBearing*scale,iy0).
// (Note that the bitmap uses y-increases-down, but the shape uses
// y-increases-up, so CodepointBitmapBox and CodepointBox are inverted.)

TL_TT_API tl_image tl_tt_GetGlyphBitmap(const tl_tt_fontinfo *info, float scale_x, float scale_y, int glyph, int *xoff, int *yoff);
TL_TT_API void tl_tt_GetGlyphBitmapBox(const tl_tt_fontinfo *font, int glyph, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1);
TL_TT_API void tl_tt_MakeGlyphBitmap(const tl_tt_fontinfo *info, tl_image* output, float scale_x, float scale_y, int glyph);

//extern void tl_tt_get_true_bbox(tl_tt_vertex *vertices, int num_verts, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1);

TL_TT_API void tl_tt_Rasterize(tl_image *result, float flatness_in_pixels, tl_tt_vertex *vertices, int num_verts, float scale_x, float scale_y, int x_off, int y_off, int invert, void *userdata);

//////////////////////////////////////////////////////////////////////////////
//
// Finding the right font...
//
// You should really just solve this offline, keep your own tables
// of what font is what, and don't try to get it out of the .ttf file.
// That's because getting it out of the .ttf file is really hard, because
// the names in the file can appear in many possible encodings, in many
// possible languages, and e.g. if you need a case-insensitive comparison,
// the details of that depend on the encoding & language in a complex way
// (actually underspecified in truetype, but also gigantic).
//
// But you can use the provided functions in two possible ways:
//     tl_tt_FindMatchingFont() will use *case-sensitive* comparisons on
//             unicode-encoded names to try to find the font you want;
//             you can run this before calling tl_tt_InitFont()
//
//     tl_tt_GetFontNameString() lets you get any of the various strings
//             from the file yourself and do your own comparisons on them.
//             You have to have called tl_tt_InitFont() first.


extern int tl_tt_FindMatchingFont(const unsigned char *fontdata, const char *name, int flags);
// returns the offset (not index) of the font that matches, or -1 if none
//   if you use STBTT_MACSTYLE_DONTCARE, use a font name like "Arial Bold".
//   if you use any other flag, use a font name like "Arial"; this checks
//     the 'macStyle' header field; i don't know if fonts set this consistently
#define STBTT_MACSTYLE_DONTCARE     0
#define STBTT_MACSTYLE_BOLD         1
#define STBTT_MACSTYLE_ITALIC       2
#define STBTT_MACSTYLE_UNDERSCORE   4
#define STBTT_MACSTYLE_NONE         8   // <= not same as 0, this makes us check the bitfield is 0

extern int tl_tt_CompareUTF8toUTF16_bigendian(const char *s1, int len1, const char *s2, int len2);
// returns 1/0 whether the first string interpreted as utf8 is identical to
// the second string interpreted as big-endian utf16... useful for strings from next func

extern char *tl_tt_GetFontNameString(const tl_tt_fontinfo *font, int *length, int platformID, int encodingID, int languageID, int nameID);
// returns the string (which may be big-endian double byte, e.g. for unicode)
// and puts the length in bytes in *length.
//
// some of the values for the IDs are below; for more see the truetype spec:
//     http://developer.apple.com/textfonts/TTRefMan/RM06/Chap6name.html
//     http://www.microsoft.com/typography/otspec/name.htm

enum { // platformID
   STBTT_PLATFORM_ID_UNICODE   =0,
   STBTT_PLATFORM_ID_MAC       =1,
   STBTT_PLATFORM_ID_ISO       =2,
   STBTT_PLATFORM_ID_MICROSOFT =3
};

enum { // encodingID for STBTT_PLATFORM_ID_UNICODE
   STBTT_UNICODE_EID_UNICODE_1_0    =0,
   STBTT_UNICODE_EID_UNICODE_1_1    =1,
   STBTT_UNICODE_EID_ISO_10646      =2,
   STBTT_UNICODE_EID_UNICODE_2_0_BMP=3,
   STBTT_UNICODE_EID_UNICODE_2_0_FULL=4,
};

enum { // encodingID for STBTT_PLATFORM_ID_MICROSOFT
   STBTT_MS_EID_SYMBOL        =0,
   STBTT_MS_EID_UNICODE_BMP   =1,
   STBTT_MS_EID_SHIFTJIS      =2,
   STBTT_MS_EID_UNICODE_FULL  =10,
};

enum { // encodingID for STBTT_PLATFORM_ID_MAC; same as Script Manager codes
   STBTT_MAC_EID_ROMAN        =0,   STBTT_MAC_EID_ARABIC       =4,
   STBTT_MAC_EID_JAPANESE     =1,   STBTT_MAC_EID_HEBREW       =5,
   STBTT_MAC_EID_CHINESE_TRAD =2,   STBTT_MAC_EID_GREEK        =6,
   STBTT_MAC_EID_KOREAN       =3,   STBTT_MAC_EID_RUSSIAN      =7,
};

enum { // languageID for STBTT_PLATFORM_ID_MICROSOFT; same as LCID...
       // problematic because there are e.g. 16 english LCIDs and 16 arabic LCIDs
   STBTT_MS_LANG_ENGLISH     =0x0409,   STBTT_MS_LANG_ITALIAN     =0x0410,
   STBTT_MS_LANG_CHINESE     =0x0804,   STBTT_MS_LANG_JAPANESE    =0x0411,
   STBTT_MS_LANG_DUTCH       =0x0413,   STBTT_MS_LANG_KOREAN      =0x0412,
   STBTT_MS_LANG_FRENCH      =0x040c,   STBTT_MS_LANG_RUSSIAN     =0x0419,
   STBTT_MS_LANG_GERMAN      =0x0407,   STBTT_MS_LANG_SPANISH     =0x0409,
   STBTT_MS_LANG_HEBREW      =0x040d,   STBTT_MS_LANG_SWEDISH     =0x041D,
};

enum { // languageID for STBTT_PLATFORM_ID_MAC
   STBTT_MAC_LANG_ENGLISH      =0 ,   STBTT_MAC_LANG_JAPANESE     =11,
   STBTT_MAC_LANG_ARABIC       =12,   STBTT_MAC_LANG_KOREAN       =23,
   STBTT_MAC_LANG_DUTCH        =4 ,   STBTT_MAC_LANG_RUSSIAN      =32,
   STBTT_MAC_LANG_FRENCH       =1 ,   STBTT_MAC_LANG_SPANISH      =6 ,
   STBTT_MAC_LANG_GERMAN       =2 ,   STBTT_MAC_LANG_SWEDISH      =5 ,
   STBTT_MAC_LANG_HEBREW       =10,   STBTT_MAC_LANG_CHINESE_SIMPLIFIED =33,
   STBTT_MAC_LANG_ITALIAN      =3 ,   STBTT_MAC_LANG_CHINESE_TRAD =19,
};

#ifdef __cplusplus
}
#endif

#endif // __STB_INCLUDE_STB_TRUETYPE_H__
