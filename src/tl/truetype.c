#include "truetype.h"
#include "cstdint.h"
#include "bits.h"

typedef char tl_tt__check_size32[sizeof(int32_t)==4 ? 1 : -1];
typedef char tl_tt__check_size16[sizeof(int16_t)==2 ? 1 : -1];

// #define your own STBTT_sort() to override this to avoid qsort
#ifndef STBTT_sort
#include <stdlib.h>
#define STBTT_sort(data,num_items,item_size,compare_func)   qsort(data,num_items,item_size,compare_func)
#endif

// #define your own STBTT_ifloor/STBTT_iceil() to avoid math.h
#ifndef STBTT_ifloor
#include <math.h>
#define STBTT_ifloor(x)   ((int) floor(x))
#define STBTT_iceil(x)    ((int) ceil(x))
#endif

// #define your own functions "STBTT_malloc" / "STBTT_free" to avoid malloc.h
#ifndef STBTT_malloc
#include <malloc.h>
#define STBTT_malloc(x,u)  malloc(x)
#define STBTT_free(x,u)    free(x)
#endif

#ifndef STBTT_assert
#include <assert.h>
#define STBTT_assert(x)    assert(x)
#endif

#ifndef STBTT_strlen
#include <string.h>
#define STBTT_strlen(x)    strlen(x)
#endif

#ifndef STBTT_memcpy
#include <memory.h>
#define STBTT_memcpy       memcpy
#define STBTT_memset       memset
#endif

//////////////////////////////////////////////////////////////////////////
//
// accessors to parse data from file
//

// on platforms that don't allow misaligned reads, if we want to allow
// truetype fonts that aren't padded to alignment, define ALLOW_UNALIGNED_TRUETYPE

#define ttBYTE(p)     (* (uint8_t *) (p))
#define ttCHAR(p)     (* (int8_t *) (p))
#define ttFixed(p)    ttLONG(p)

#if defined(STB_TRUETYPE_BIGENDIAN) && !defined(ALLOW_UNALIGNED_TRUETYPE)

	#define ttUSHORT(p)   (* (uint16_t *) (p))
	#define ttSHORT(p)    (* (int16_t *) (p))
	#define ttULONG(p)    (* (uint32_t *) (p))
	#define ttLONG(p)     (* (int32_t *) (p))

#elif TL_LITTLE_ENDIAN && TL_UNALIGNED_ACCESS

	static uint16_t ttUSHORT(const uint8_t *p) { return tl_byteswap16(*(uint16_t*)p); }
	static int16_t  ttSHORT(const uint8_t *p)   { return (int16_t)tl_byteswap16(*(int16_t*)p); }
	static uint32_t ttULONG(const uint8_t *p)  { return tl_byteswap32(*(uint32_t*)p); }
	static int32_t  ttLONG(const uint8_t *p)    { return (int32_t)tl_byteswap32(*(int32_t*)p); }
#else

	static uint16_t ttUSHORT(const uint8_t *p) { return p[0]*256 + p[1]; }
	static int16_t ttSHORT(const uint8_t *p)   { return p[0]*256 + p[1]; }
	static uint32_t ttULONG(const uint8_t *p)  { return (p[0]<<24) + (p[1]<<16) + (p[2]<<8) + p[3]; }
	static int32_t ttLONG(const uint8_t *p)    { return (p[0]<<24) + (p[1]<<16) + (p[2]<<8) + p[3]; }

#endif

#define tl_tt_tag4(p,c0,c1,c2,c3) ((p)[0] == (c0) && (p)[1] == (c1) && (p)[2] == (c2) && (p)[3] == (c3))
#define tl_tt_tag(p,str)           tl_tt_tag4(p,str[0],str[1],str[2],str[3])

static int tl_tt__isfont(const uint8_t *font)
{
   // check the version number
   if (tl_tt_tag(font, "1"))   return 1; // TrueType 1
   if (tl_tt_tag(font, "typ1"))   return 1; // TrueType with type 1 font -- we don't support this!
   if (tl_tt_tag(font, "OTTO"))   return 1; // OpenType with CFF
   if (tl_tt_tag4(font, 0,1,0,0)) return 1; // OpenType 1.0
   return 0;
}

// @OPTIMIZE: binary search
static uint32_t tl_tt__find_table(uint8_t *data, uint32_t fontstart, char *tag)
{
   int32_t num_tables = ttUSHORT(data+fontstart+4);
   uint32_t tabledir = fontstart + 12;
   int32_t i;
   for (i=0; i < num_tables; ++i) {
      uint32_t loc = tabledir + 16*i;
      if (tl_tt_tag(data+loc+0, tag))
         return ttULONG(data+loc+8);
   }
   return 0;
}

int tl_tt_GetFontOffsetForIndex(const unsigned char *font_collection, int index)
{
   // if it's just a font, there's only one valid index
   if (tl_tt__isfont(font_collection))
      return index == 0 ? 0 : -1;

   // check if it's a TTC
   if (tl_tt_tag(font_collection, "ttcf")) {
      // version 1?
      if (ttULONG(font_collection+4) == 0x00010000 || ttULONG(font_collection+4) == 0x00020000) {
         int32_t n = ttLONG(font_collection+8);
         if (index >= n)
            return -1;
         return ttULONG(font_collection+12+index*14);
      }
   }
   return -1;
}

int tl_tt_InitFont(tl_tt_fontinfo *info, const unsigned char *data2, int fontstart)
{
   uint8_t *data = (uint8_t *) data2;
   uint32_t cmap, t;
   int32_t i,numTables;

   info->data = data;
   info->fontstart = fontstart;

   cmap = tl_tt__find_table(data, fontstart, "cmap");
   info->loca = tl_tt__find_table(data, fontstart, "loca");
   info->head = tl_tt__find_table(data, fontstart, "head");
   info->glyf = tl_tt__find_table(data, fontstart, "glyf");
   info->hhea = tl_tt__find_table(data, fontstart, "hhea");
   info->hmtx = tl_tt__find_table(data, fontstart, "hmtx");
   if (!cmap || !info->loca || !info->head || !info->glyf || !info->hhea || !info->hmtx)
      return 0;

   t = tl_tt__find_table(data, fontstart, "maxp");
   if (t)
      info->numGlyphs = ttUSHORT(data+t+4);
   else
      info->numGlyphs = 0xffff;

   // find a cmap encoding table we understand *now* to avoid searching
   // later. (todo: could make this installable)
   // the same regardless of glyph.
   numTables = ttUSHORT(data + cmap + 2);
   info->index_map = 0;
   for (i=0; i < numTables; ++i) {
      uint32_t encoding_record = cmap + 4 + 8 * i;
      // find an encoding we understand:
      switch(ttUSHORT(data+encoding_record)) {
         case STBTT_PLATFORM_ID_MICROSOFT:
            switch (ttUSHORT(data+encoding_record+2)) {
               case STBTT_MS_EID_UNICODE_BMP:
               case STBTT_MS_EID_UNICODE_FULL:
                  // MS/Unicode
                  info->index_map = cmap + ttULONG(data+encoding_record+4);
                  break;
            }
            break;
      }
   }
   if (info->index_map == 0)
      return 0;

   info->indexToLocFormat = ttUSHORT(data+info->head + 50);
   return 1;
}

int tl_tt_FindGlyphIndex(const tl_tt_fontinfo *info, int unicode_codepoint)
{
   uint8_t *data = info->data;
   uint32_t index_map = info->index_map;

   uint16_t format = ttUSHORT(data + index_map + 0);
   if (format == 0) { // apple byte encoding
      int32_t bytes = ttUSHORT(data + index_map + 2);
      if (unicode_codepoint < bytes-6)
         return ttBYTE(data + index_map + 6 + unicode_codepoint);
      return 0;
   } else if (format == 6) {
      uint32_t first = ttUSHORT(data + index_map + 6);
      uint32_t count = ttUSHORT(data + index_map + 8);
      if ((uint32_t) unicode_codepoint >= first && (uint32_t) unicode_codepoint < first+count)
         return ttUSHORT(data + index_map + 10 + (unicode_codepoint - first)*2);
      return 0;
   } else if (format == 2) {
      STBTT_assert(0); // @TODO: high-byte mapping for japanese/chinese/korean
      return 0;
   } else if (format == 4) { // standard mapping for windows fonts: binary search collection of ranges
      uint16_t segcount = ttUSHORT(data+index_map+6) >> 1;
      uint16_t searchRange = ttUSHORT(data+index_map+8) >> 1;
      uint16_t entrySelector = ttUSHORT(data+index_map+10);
      uint16_t rangeShift = ttUSHORT(data+index_map+12) >> 1;
      uint16_t item, offset, start, end;

      // do a binary search of the segments
      uint32_t endCount = index_map + 14;
      uint32_t search = endCount;

      if (unicode_codepoint > 0xffff)
         return 0;

      // they lie from endCount .. endCount + segCount
      // but searchRange is the nearest power of two, so...
      if (unicode_codepoint >= ttUSHORT(data + search + rangeShift*2))
         search += rangeShift*2;

      // now decrement to bias correctly to find smallest
      search -= 2;
      while (entrySelector) {
         uint16_t start, end;
         searchRange >>= 1;
         start = ttUSHORT(data + search + 2 + segcount*2 + 2);
         end = ttUSHORT(data + search + 2);
         start = ttUSHORT(data + search + searchRange*2 + segcount*2 + 2);
         end = ttUSHORT(data + search + searchRange*2);
         if (unicode_codepoint > end)
            search += searchRange*2;
         --entrySelector;
      }
      search += 2;

      item = (uint16_t) ((search - endCount) >> 1);

      STBTT_assert(unicode_codepoint <= ttUSHORT(data + endCount + 2*item));
      start = ttUSHORT(data + index_map + 14 + segcount*2 + 2 + 2*item);
      end = ttUSHORT(data + index_map + 14 + 2 + 2*item);
      if (unicode_codepoint < start)
         return 0;

      offset = ttUSHORT(data + index_map + 14 + segcount*6 + 2 + 2*item);
      if (offset == 0)
         return unicode_codepoint + ttSHORT(data + index_map + 14 + segcount*4 + 2 + 2*item);

      return ttUSHORT(data + offset + (unicode_codepoint-start)*2 + index_map + 14 + segcount*6 + 2 + 2*item);
   } else if (format == 12) {
      uint16_t ngroups = ttUSHORT(data+index_map+6);
      int32_t low,high;
      //uint16_t g = 0;
      low = 0; high = (int32_t)ngroups;
      // Binary search the right group.
      while (low <= high) {
         int32_t mid = low + ((high-low) >> 1); // rounds down, so low <= mid < high
         uint32_t start_char = ttULONG(data+index_map+16+mid*12);
         uint32_t end_char = ttULONG(data+index_map+16+mid*12+4);
         if ((uint32_t) unicode_codepoint < start_char)
            high = mid-1;
         else if ((uint32_t) unicode_codepoint > end_char)
            low = mid+1;
         else {
            uint32_t start_glyph = ttULONG(data+index_map+16+mid*12+8);
            return start_glyph + unicode_codepoint-start_char;
         }
      }
      return 0; // not found
   }
   // @TODO
   STBTT_assert(0);
   return 0;
}

int tl_tt_GetCodepointShape(const tl_tt_fontinfo *info, int unicode_codepoint, tl_tt_vertex **vertices)
{
   return tl_tt_GetGlyphShape(info, tl_tt_FindGlyphIndex(info, unicode_codepoint), vertices);
}

static void tl_tt_setvertex(tl_tt_vertex *v, uint8_t type, int16_t x, int16_t y, int16_t cx, int16_t cy)
{
   v->type = type;
   v->x = x;
   v->y = y;
   v->cx = cx;
   v->cy = cy;
}

static int tl_tt__GetGlyfOffset(const tl_tt_fontinfo *info, int glyph_index)
{
   int g1,g2;

   if (glyph_index >= info->numGlyphs) return -1; // glyph index out of range
   if (info->indexToLocFormat >= 2)    return -1; // unknown index->glyph map format

   if (info->indexToLocFormat == 0) {
      g1 = info->glyf + ttUSHORT(info->data + info->loca + glyph_index * 2) * 2;
      g2 = info->glyf + ttUSHORT(info->data + info->loca + glyph_index * 2 + 2) * 2;
   } else {
      g1 = info->glyf + ttULONG (info->data + info->loca + glyph_index * 4);
      g2 = info->glyf + ttULONG (info->data + info->loca + glyph_index * 4 + 4);
   }

   return g1==g2 ? -1 : g1; // if length is 0, return -1
}

int tl_tt_GetGlyphBox(const tl_tt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1)
{
   int g = tl_tt__GetGlyfOffset(info, glyph_index);
   if (g < 0) return 0;

   if (x0) *x0 = ttSHORT(info->data + g + 2);
   if (y0) *y0 = ttSHORT(info->data + g + 4);
   if (x1) *x1 = ttSHORT(info->data + g + 6);
   if (y1) *y1 = ttSHORT(info->data + g + 8);
   return 1;
}

int tl_tt_GetCodepointBox(const tl_tt_fontinfo *info, int codepoint, int *x0, int *y0, int *x1, int *y1)
{
   return tl_tt_GetGlyphBox(info, tl_tt_FindGlyphIndex(info,codepoint), x0,y0,x1,y1);
}

int tl_tt_GetGlyphShape(const tl_tt_fontinfo *info, int glyph_index, tl_tt_vertex **pvertices)
{
   int16_t numberOfContours;
   uint8_t *endPtsOfContours;
   uint8_t *data = info->data;
   tl_tt_vertex *vertices=0;
   int num_vertices=0;
   int g = tl_tt__GetGlyfOffset(info, glyph_index);

   *pvertices = NULL;

   if (g < 0) return 0;

   numberOfContours = ttSHORT(data + g);

   if (numberOfContours > 0) {
      uint8_t flags=0,flagcount;
      int32_t ins, i,j=0,m,n, next_move, was_off=0, off;
      int16_t x,y,cx,cy,sx,sy;
      uint8_t *points;
      endPtsOfContours = (data + g + 10);
      ins = ttUSHORT(data + g + 10 + numberOfContours * 2);
      points = data + g + 10 + numberOfContours * 2 + 2 + ins;

      n = 1+ttUSHORT(endPtsOfContours + numberOfContours*2-2);

      m = n + numberOfContours;  // a loose bound on how many vertices we might need
      vertices = (tl_tt_vertex *) STBTT_malloc(m * sizeof(vertices[0]), info->userdata);
      if (vertices == 0)
         return 0;

      next_move = 0;
      flagcount=0;

      // in first pass, we load uninterpreted data into the allocated array
      // above, shifted to the end of the array so we won't overwrite it when
      // we create our final data starting from the front

      off = m - n; // starting offset for uninterpreted data, regardless of how m ends up being calculated

      // first load flags

      for (i=0; i < n; ++i) {
         if (flagcount == 0) {
            flags = *points++;
            if (flags & 8)
               flagcount = *points++;
         } else
            --flagcount;
         vertices[off+i].type = flags;
      }

      // now load x coordinates
      x=0;
      for (i=0; i < n; ++i) {
         flags = vertices[off+i].type;
         if (flags & 2) {
            int16_t dx = *points++;
            x += (flags & 16) ? dx : -dx; // ???
         } else {
            if (!(flags & 16)) {
               x = x + (int16_t) (points[0]*256 + points[1]);
               points += 2;
            }
         }
         vertices[off+i].x = x;
      }

      // now load y coordinates
      y=0;
      for (i=0; i < n; ++i) {
         flags = vertices[off+i].type;
         if (flags & 4) {
            int16_t dy = *points++;
            y += (flags & 32) ? dy : -dy; // ???
         } else {
            if (!(flags & 32)) {
               y = y + (int16_t) (points[0]*256 + points[1]);
               points += 2;
            }
         }
         vertices[off+i].y = y;
      }

      // now convert them to our format
      num_vertices=0;
      sx = sy = cx = cy = 0;
      for (i=0; i < n; ++i) {
         flags = vertices[off+i].type;
         x     = (int16_t) vertices[off+i].x;
         y     = (int16_t) vertices[off+i].y;
         if (next_move == i) {
            // when we get to the end, we have to close the shape explicitly
            if (i != 0) {
               if (was_off)
                  tl_tt_setvertex(&vertices[num_vertices++], STBTT_vcurve,sx,sy,cx,cy);
               else
                  tl_tt_setvertex(&vertices[num_vertices++], STBTT_vline,sx,sy,0,0);
            }

            // now start the new one
            tl_tt_setvertex(&vertices[num_vertices++], STBTT_vmove,x,y,0,0);
            next_move = 1 + ttUSHORT(endPtsOfContours+j*2);
            ++j;
            was_off = 0;
            sx = x;
            sy = y;
         } else {
            if (!(flags & 1)) { // if it's a curve
               if (was_off) // two off-curve control points in a row means interpolate an on-curve midpoint
                  tl_tt_setvertex(&vertices[num_vertices++], STBTT_vcurve, (cx+x)>>1, (cy+y)>>1, cx, cy);
               cx = x;
               cy = y;
               was_off = 1;
            } else {
               if (was_off)
                  tl_tt_setvertex(&vertices[num_vertices++], STBTT_vcurve, x,y, cx, cy);
               else
                  tl_tt_setvertex(&vertices[num_vertices++], STBTT_vline, x,y,0,0);
               was_off = 0;
            }
         }
      }
      if (i != 0) {
         if (was_off)
            tl_tt_setvertex(&vertices[num_vertices++], STBTT_vcurve,sx,sy,cx,cy);
         else
            tl_tt_setvertex(&vertices[num_vertices++], STBTT_vline,sx,sy,0,0);
      }
   } else if (numberOfContours == -1) {
      // Compound shapes.
      int more = 1;
      uint8_t *comp = data + g + 10;
      num_vertices = 0;
      vertices = 0;
      while (more) {
         uint16_t flags, gidx;
         int comp_num_verts = 0, i;
         tl_tt_vertex *comp_verts = 0, *tmp = 0;
         float mtx[6] = {1,0,0,1,0,0}, m, n;

         flags = ttSHORT(comp); comp+=2;
         gidx = ttSHORT(comp); comp+=2;

         if (flags & 2) { // XY values
            if (flags & 1) { // shorts
               mtx[4] = ttSHORT(comp); comp+=2;
               mtx[5] = ttSHORT(comp); comp+=2;
            } else {
               mtx[4] = ttCHAR(comp); comp+=1;
               mtx[5] = ttCHAR(comp); comp+=1;
            }
         }
         else {
            // @TODO handle matching point
            STBTT_assert(0);
         }
         if (flags & (1<<3)) { // WE_HAVE_A_SCALE
            mtx[0] = mtx[3] = ttSHORT(comp)/16384.0f; comp+=2;
            mtx[1] = mtx[2] = 0;
         } else if (flags & (1<<6)) { // WE_HAVE_AN_X_AND_YSCALE
            mtx[0] = ttSHORT(comp)/16384.0f; comp+=2;
            mtx[1] = mtx[2] = 0;
            mtx[3] = ttSHORT(comp)/16384.0f; comp+=2;
         } else if (flags & (1<<7)) { // WE_HAVE_A_TWO_BY_TWO
            mtx[0] = ttSHORT(comp)/16384.0f; comp+=2;
            mtx[1] = ttSHORT(comp)/16384.0f; comp+=2;
            mtx[2] = ttSHORT(comp)/16384.0f; comp+=2;
            mtx[3] = ttSHORT(comp)/16384.0f; comp+=2;
         }

         // Find transformation scales.
         m = (float) sqrt(mtx[0]*mtx[0] + mtx[1]*mtx[1]);
         n = (float) sqrt(mtx[2]*mtx[2] + mtx[3]*mtx[3]);

         // Get indexed glyph.
         comp_num_verts = tl_tt_GetGlyphShape(info, gidx, &comp_verts);
         if (comp_num_verts > 0) {
            // Transform vertices.
            for (i = 0; i < comp_num_verts; ++i) {
               tl_tt_vertex* v = &comp_verts[i];
               tl_tt_vertex_type x,y;
               x=v->x; y=v->y;
               v->x = (tl_tt_vertex_type)(m * (mtx[0]*x + mtx[2]*y + mtx[4]));
               v->y = (tl_tt_vertex_type)(n * (mtx[1]*x + mtx[3]*y + mtx[5]));
               x=v->cx; y=v->cy;
               v->cx = (tl_tt_vertex_type)(m * (mtx[0]*x + mtx[2]*y + mtx[4]));
               v->cy = (tl_tt_vertex_type)(n * (mtx[1]*x + mtx[3]*y + mtx[5]));
            }
            // Append vertices.
            tmp = (tl_tt_vertex*)STBTT_malloc((num_vertices+comp_num_verts)*sizeof(tl_tt_vertex), info->userdata);
            if (!tmp) {
               if (vertices) STBTT_free(vertices, info->userdata);
               if (comp_verts) STBTT_free(comp_verts, info->userdata);
               return 0;
            }
            if (num_vertices > 0) memcpy(tmp, vertices, num_vertices*sizeof(tl_tt_vertex));
            memcpy(tmp+num_vertices, comp_verts, comp_num_verts*sizeof(tl_tt_vertex));
            if (vertices) STBTT_free(vertices, info->userdata);
            vertices = tmp;
            STBTT_free(comp_verts, info->userdata);
            num_vertices += comp_num_verts;
         }
         // More components ?
         more = flags & (1<<5);
      }
   } else if (numberOfContours < 0) {
      // @TODO other compound variations?
      STBTT_assert(0);
   } else {
      // numberOfCounters == 0, do nothing
   }

   *pvertices = vertices;
   return num_vertices;
}

void tl_tt_GetGlyphHMetrics(const tl_tt_fontinfo *info, int glyph_index, int *advanceWidth, int *leftSideBearing)
{
   uint16_t numOfLongHorMetrics = ttUSHORT(info->data+info->hhea + 34);
   if (glyph_index < numOfLongHorMetrics) {
      if (advanceWidth)     *advanceWidth    = ttSHORT(info->data + info->hmtx + 4*glyph_index);
      if (leftSideBearing)  *leftSideBearing = ttSHORT(info->data + info->hmtx + 4*glyph_index + 2);
   } else {
      if (advanceWidth)     *advanceWidth    = ttSHORT(info->data + info->hmtx + 4*(numOfLongHorMetrics-1));
      if (leftSideBearing)  *leftSideBearing = ttSHORT(info->data + info->hmtx + 4*numOfLongHorMetrics + 2*(glyph_index - numOfLongHorMetrics));
   }
}

int tl_tt_GetGlyphKernAdvance(const tl_tt_fontinfo *info, int glyph1, int glyph2)
{
   return 0;
}

int tl_tt_GetCodepointKernAdvance(const tl_tt_fontinfo *info, int ch1, int ch2)
{
   return 0;
}

void tl_tt_GetCodepointHMetrics(const tl_tt_fontinfo *info, int codepoint, int *advanceWidth, int *leftSideBearing)
{
   tl_tt_GetGlyphHMetrics(info, tl_tt_FindGlyphIndex(info,codepoint), advanceWidth, leftSideBearing);
}

void tl_tt_GetFontVMetrics(const tl_tt_fontinfo *info, int *ascent, int *descent, int *lineGap)
{
   if (ascent ) *ascent  = ttSHORT(info->data+info->hhea + 4);
   if (descent) *descent = ttSHORT(info->data+info->hhea + 6);
   if (lineGap) *lineGap = ttSHORT(info->data+info->hhea + 8);
}

float tl_tt_ScaleForPixelHeight(const tl_tt_fontinfo *info, float height)
{
   int fheight = ttSHORT(info->data + info->hhea + 4) - ttSHORT(info->data + info->hhea + 6);
   return (float) height / fheight;
}

void tl_tt_FreeShape(const tl_tt_fontinfo *info, tl_tt_vertex *v)
{
   STBTT_free(v, info->userdata);
}

//////////////////////////////////////////////////////////////////////////////
//
// antialiasing software rasterizer
//

void tl_tt_GetGlyphBitmapBox(const tl_tt_fontinfo *font, int glyph, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1)
{
   int x0,y0,x1,y1;
   if (!tl_tt_GetGlyphBox(font, glyph, &x0,&y0,&x1,&y1))
      x0=y0=x1=y1=0; // e.g. space character
   // now move to integral bboxes (treating pixels as little squares, what pixels get touched)?
   if (ix0) *ix0 =  STBTT_ifloor(x0 * scale_x);
   if (iy0) *iy0 = -STBTT_iceil (y1 * scale_y);
   if (ix1) *ix1 =  STBTT_iceil (x1 * scale_x);
   if (iy1) *iy1 = -STBTT_ifloor(y0 * scale_y);
}

void tl_tt_GetCodepointBitmapBox(const tl_tt_fontinfo *font, int codepoint, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1)
{
   tl_tt_GetGlyphBitmapBox(font, tl_tt_FindGlyphIndex(font,codepoint), scale_x, scale_y, ix0,iy0,ix1,iy1);
}

typedef struct tl_tt__edge {
   float x0,y0, x1,y1;
   int invert;
} tl_tt__edge;

typedef struct tl_tt__active_edge
{
   int x,dx;
   float ey;
   struct tl_tt__active_edge *next;
   int valid;
} tl_tt__active_edge;

#define FIXSHIFT   10
#define FIX        (1 << FIXSHIFT)
#define FIXMASK    (FIX-1)

static tl_tt__active_edge *new_active(tl_tt__edge *e, int off_x, float start_point, void *userdata)
{
   tl_tt__active_edge *z = (tl_tt__active_edge *) STBTT_malloc(sizeof(*z), userdata); // @TODO: make a pool of these!!!
   float dxdy = (e->x1 - e->x0) / (e->y1 - e->y0);
   STBTT_assert(e->y0 <= start_point);
   if (!z) return z;
   // round dx down to avoid going too far
   if (dxdy < 0)
      z->dx = -STBTT_ifloor(FIX * -dxdy);
   else
      z->dx = STBTT_ifloor(FIX * dxdy);
   z->x = STBTT_ifloor(FIX * (e->x0 + dxdy * (start_point - e->y0)));
   z->x -= off_x * FIX;
   z->ey = e->y1;
   z->next = 0;
   z->valid = e->invert ? 1 : -1;
   return z;
}

// note: this routine clips fills that extend off the edges... ideally this
// wouldn't happen, but it could happen if the truetype glyph bounding boxes
// are wrong, or if the user supplies a too-small bitmap
static void tl_tt__fill_active_edges(unsigned char *scanline, int len, tl_tt__active_edge *e, int max_weight)
{
   // non-zero winding fill
   int x0=0, w=0;

   while (e) {
      if (w == 0) {
         // if we're currently at zero, we need to record the edge start point
         x0 = e->x; w += e->valid;
      } else {
         int x1 = e->x; w += e->valid;
         // if we went to zero, we need to draw
         if (w == 0) {
            int i = x0 >> FIXSHIFT;
            int j = x1 >> FIXSHIFT;

            if (i < len && j >= 0) {
               if (i == j) {
                  // x0,x1 are the same pixel, so compute combined coverage
                  scanline[i] = scanline[i] + (uint8_t) ((x1 - x0) * max_weight >> FIXSHIFT);
               } else {
                  if (i >= 0) // add antialiasing for x0
                     scanline[i] = scanline[i] + (uint8_t) (((FIX - (x0 & FIXMASK)) * max_weight) >> FIXSHIFT);
                  else
                     i = -1; // clip

                  if (j < len) // add antialiasing for x1
                     scanline[j] = scanline[j] + (uint8_t) (((x1 & FIXMASK) * max_weight) >> FIXSHIFT);
                  else
                     j = len; // clip

                  for (++i; i < j; ++i) // fill pixels between x0 and x1
                     scanline[i] = scanline[i] + (uint8_t) max_weight;
               }
            }
         }
      }

      e = e->next;
   }
}

static void tl_tt__rasterize_sorted_edges(tl_image *result, tl_tt__edge *e, int n, int vsubsample, int off_x, int off_y, void *userdata)
{
   tl_tt__active_edge *active = NULL;
   int y;
   uint32_t j = 0;
   int max_weight = (255 / vsubsample);  // weight per vertical scanline
   int s; // vertical subsample index
   unsigned char scanline_data[512], *scanline;

   if (result->w > 512)
      scanline = (unsigned char *) STBTT_malloc(result->w, userdata);
   else
      scanline = scanline_data;

   y = off_y * vsubsample;
   e[n].y0 = (off_y + result->h) * (float) vsubsample + 1;

   while (j < result->h) {
      STBTT_memset(scanline, 0, result->w);
      for (s=0; s < vsubsample; ++s) {
         // find center of pixel for this scanline
         float scan_y = y + 0.5f;
         tl_tt__active_edge **step = &active;

         // update all active edges;
         // remove all active edges that terminate before the center of this scanline
         while (*step) {
            tl_tt__active_edge * z = *step;
            if (z->ey <= scan_y) {
               *step = z->next; // delete from list
               STBTT_assert(z->valid);
               z->valid = 0;
               STBTT_free(z, userdata);
            } else {
               z->x += z->dx; // advance to position for current scanline
               step = &((*step)->next); // advance through list
            }
         }

         // resort the list if needed
         for(;;) {
            int changed=0;
            step = &active;
            while (*step && (*step)->next) {
               if ((*step)->x > (*step)->next->x) {
                  tl_tt__active_edge *t = *step;
                  tl_tt__active_edge *q = t->next;

                  t->next = q->next;
                  q->next = t;
                  *step = q;
                  changed = 1;
               }
               step = &(*step)->next;
            }
            if (!changed) break;
         }

         // insert all edges that start before the center of this scanline -- omit ones that also end on this scanline
         while (e->y0 <= scan_y) {
            if (e->y1 > scan_y) {
               tl_tt__active_edge *z = new_active(e, off_x, scan_y, userdata);
               // find insertion point
               if (active == NULL)
                  active = z;
               else if (z->x < active->x) {
                  // insert at front
                  z->next = active;
                  active = z;
               } else {
                  // find thing to insert AFTER
                  tl_tt__active_edge *p = active;
                  while (p->next && p->next->x < z->x)
                     p = p->next;
                  // at this point, p->next->x is NOT < z->x
                  z->next = p->next;
                  p->next = z;
               }
            }
            ++e;
         }

         // now process all active edges in XOR fashion
         if (active)
            tl_tt__fill_active_edges(scanline, result->w, active, max_weight);

         ++y;
      }
      STBTT_memcpy(result->pixels + j * result->pitch, scanline, result->w);
      ++j;
   }

   while (active) {
      tl_tt__active_edge *z = active;
      active = active->next;
      STBTT_free(z, userdata);
   }

   if (scanline != scanline_data)
      STBTT_free(scanline, userdata);
}

static int tl_tt__edge_compare(const void *p, const void *q)
{
   tl_tt__edge *a = (tl_tt__edge *) p;
   tl_tt__edge *b = (tl_tt__edge *) q;

   if (a->y0 < b->y0) return -1;
   if (a->y0 > b->y0) return  1;
   return 0;
}

typedef struct
{
   float x,y;
} tl_tt__point;

static void tl_tt__rasterize(tl_image *result, tl_tt__point *pts, int *wcount, int windings, float scale_x, float scale_y, int off_x, int off_y, int invert, void *userdata)
{
   float y_scale_inv = invert ? -scale_y : scale_y;
   tl_tt__edge *e;
   int n,i,j,k,m;
   int vsubsample = result->h < 8 ? 15 : 5;
   // vsubsample should divide 255 evenly; otherwise we won't reach full opacity

   // now we have to blow out the windings into explicit edge lists
   n = 0;
   for (i=0; i < windings; ++i)
      n += wcount[i];

   e = (tl_tt__edge *) STBTT_malloc(sizeof(*e) * (n+1), userdata); // add an extra one as a sentinel
   if (e == 0) return;
   n = 0;

   m=0;
   for (i=0; i < windings; ++i) {
      tl_tt__point *p = pts + m;
      m += wcount[i];
      j = wcount[i]-1;
      for (k=0; k < wcount[i]; j=k++) {
         int a=k,b=j;
         // skip the edge if horizontal
         if (p[j].y == p[k].y)
            continue;
         // add edge from j to k to the list
         e[n].invert = 0;
         if (invert ? p[j].y > p[k].y : p[j].y < p[k].y) {
            e[n].invert = 1;
            a=j,b=k;
         }
         e[n].x0 = p[a].x * scale_x;
         e[n].y0 = p[a].y * y_scale_inv * vsubsample;
         e[n].x1 = p[b].x * scale_x;
         e[n].y1 = p[b].y * y_scale_inv * vsubsample;
         ++n;
      }
   }

   // now sort the edges by their highest point (should snap to integer, and then by x)
   STBTT_sort(e, n, sizeof(e[0]), tl_tt__edge_compare);

   // now, traverse the scanlines and find the intersections on each scanline, use xor winding rule
   tl_tt__rasterize_sorted_edges(result, e, n, vsubsample, off_x, off_y, userdata);

   STBTT_free(e, userdata);
}

static void tl_tt__add_point(tl_tt__point *points, int n, float x, float y)
{
   if (!points) return; // during first pass, it's unallocated
   points[n].x = x;
   points[n].y = y;
}

// tesselate until threshhold p is happy... @TODO warped to compensate for non-linear stretching
static int tl_tt__tesselate_curve(tl_tt__point *points, int *num_points, float x0, float y0, float x1, float y1, float x2, float y2, float objspace_flatness_squared, int n)
{
   // midpoint
   float mx = (x0 + 2*x1 + x2)/4;
   float my = (y0 + 2*y1 + y2)/4;
   // versus directly drawn line
   float dx = (x0+x2)/2 - mx;
   float dy = (y0+y2)/2 - my;
   if (n > 16) // 65536 segments on one curve better be enough!
      return 1;
   if (dx*dx+dy*dy > objspace_flatness_squared) { // half-pixel error allowed... need to be smaller if AA
      tl_tt__tesselate_curve(points, num_points, x0,y0, (x0+x1)/2.0f,(y0+y1)/2.0f, mx,my, objspace_flatness_squared,n+1);
      tl_tt__tesselate_curve(points, num_points, mx,my, (x1+x2)/2.0f,(y1+y2)/2.0f, x2,y2, objspace_flatness_squared,n+1);
   } else {
      tl_tt__add_point(points, *num_points,x2,y2);
      *num_points = *num_points+1;
   }
   return 1;
}

// returns number of contours
tl_tt__point *tl_tt_FlattenCurves(tl_tt_vertex *vertices, int num_verts, float objspace_flatness, int **contour_lengths, int *num_contours, void *userdata)
{
   tl_tt__point *points=0;
   int num_points=0;

   float objspace_flatness_squared = objspace_flatness * objspace_flatness;
   int i,n=0,start=0, pass;

   // count how many "moves" there are to get the contour count
   for (i=0; i < num_verts; ++i)
      if (vertices[i].type == STBTT_vmove)
         ++n;

   *num_contours = n;
   if (n == 0) return 0;

   *contour_lengths = (int *) STBTT_malloc(sizeof(**contour_lengths) * n, userdata);

   if (*contour_lengths == 0) {
      *num_contours = 0;
      return 0;
   }

   // make two passes through the points so we don't need to realloc
   for (pass=0; pass < 2; ++pass) {
      float x=0,y=0;
      if (pass == 1) {
         points = (tl_tt__point *) STBTT_malloc(num_points * sizeof(points[0]), userdata);
         if (points == NULL) goto error;
      }
      num_points = 0;
      n= -1;
      for (i=0; i < num_verts; ++i) {
         switch (vertices[i].type) {
            case STBTT_vmove:
               // start the next contour
               if (n >= 0)
                  (*contour_lengths)[n] = num_points - start;
               ++n;
               start = num_points;

               x = vertices[i].x, y = vertices[i].y;
               tl_tt__add_point(points, num_points++, x,y);
               break;
            case STBTT_vline:
               x = vertices[i].x, y = vertices[i].y;
               tl_tt__add_point(points, num_points++, x, y);
               break;
            case STBTT_vcurve:
               tl_tt__tesselate_curve(points, &num_points, x,y,
                                        vertices[i].cx, vertices[i].cy,
                                        vertices[i].x,  vertices[i].y,
                                        objspace_flatness_squared, 0);
               x = vertices[i].x, y = vertices[i].y;
               break;
         }
      }
      (*contour_lengths)[n] = num_points - start;
   }

   return points;
error:
   STBTT_free(points, userdata);
   STBTT_free(*contour_lengths, userdata);
   *contour_lengths = 0;
   *num_contours = 0;
   return NULL;
}

void tl_tt_Rasterize(tl_image *result, float flatness_in_pixels, tl_tt_vertex *vertices, int num_verts, float scale_x, float scale_y, int x_off, int y_off, int invert, void *userdata)
{
   float scale = scale_x > scale_y ? scale_y : scale_x;
   int winding_count, *winding_lengths;
   tl_tt__point *windings = tl_tt_FlattenCurves(vertices, num_verts, flatness_in_pixels / scale, &winding_lengths, &winding_count, userdata);
   if (windings) {
      tl_tt__rasterize(result, windings, winding_lengths, winding_count, scale_x, scale_y, x_off, y_off, invert, userdata);
      STBTT_free(winding_lengths, userdata);
      STBTT_free(windings, userdata);
   }
}

void tl_tt_FreeBitmap(unsigned char *bitmap, void *userdata)
{
   STBTT_free(bitmap, userdata);
}

tl_image tl_tt_GetGlyphBitmap(const tl_tt_fontinfo *info, float scale_x, float scale_y, int glyph, int *xoff, int *yoff)
{
   int ix0,iy0,ix1,iy1;
   tl_image gbm;
   tl_tt_vertex *vertices;
   int num_verts = tl_tt_GetGlyphShape(info, glyph, &vertices);

   gbm.w = 0;
   gbm.h = 0;
   gbm.bpp = 1;
   gbm.pitch = 0;
   gbm.pixels = NULL; // in case we error

   if (scale_x == 0) scale_x = scale_y;
   if (scale_y == 0) {
      if (scale_x == 0) return gbm;
      scale_y = scale_x;
   }

   tl_tt_GetGlyphBitmapBox(info, glyph, scale_x, scale_y, &ix0,&iy0,&ix1,&iy1);

   // now we get the size
   gbm.w = (ix1 - ix0);
   gbm.h = (iy1 - iy0);
   gbm.pitch = gbm.w;


   if (xoff  ) *xoff   = ix0;
   if (yoff  ) *yoff   = iy0;

   if (gbm.w && gbm.h) {
      gbm.pixels = (unsigned char *) STBTT_malloc(gbm.w * gbm.h, info->userdata);
      if (gbm.pixels) {
         tl_tt_Rasterize(&gbm, 0.35f, vertices, num_verts, scale_x, scale_y, ix0, iy0, 1, info->userdata);
      }
   }
   STBTT_free(vertices, info->userdata);
   return gbm;
}

void tl_tt_MakeGlyphBitmap(const tl_tt_fontinfo *info, tl_image* output, float scale_x, float scale_y, int glyph)
{
   int ix0,iy0;
   tl_tt_vertex *vertices;
   int num_verts = tl_tt_GetGlyphShape(info, glyph, &vertices);
   tl_tt_GetGlyphBitmapBox(info, glyph, scale_x, scale_y, &ix0,&iy0,0,0);

   if (output->w && output->h)
      tl_tt_Rasterize(output, 0.35f, vertices, num_verts, scale_x, scale_y, ix0,iy0, 1, info->userdata);

   STBTT_free(vertices, info->userdata);
}

tl_image tl_tt_GetCodepointBitmap(const tl_tt_fontinfo *info, float scale_x, float scale_y, int codepoint, int *xoff, int *yoff)
{
   return tl_tt_GetGlyphBitmap(info, scale_x, scale_y, tl_tt_FindGlyphIndex(info,codepoint), xoff,yoff);
}

void tl_tt_MakeCodepointBitmap(const tl_tt_fontinfo *info, tl_image* output, float scale_x, float scale_y, int codepoint)
{
   tl_tt_MakeGlyphBitmap(info, output, scale_x, scale_y, tl_tt_FindGlyphIndex(info,codepoint));
}

//////////////////////////////////////////////////////////////////////////////
//
// bitmap baking
//
// This is SUPER-SHITTY packing to keep source code small

#if 0
extern int tl_tt_BakeFontBitmap(const unsigned char *data, int offset,  // font location (use offset=0 for plain .ttf)
                                float pixel_height,                     // height of font in pixels
                                unsigned char *pixels, int pw, int ph,  // bitmap to be filled in
                                int first_char, int num_chars,          // characters to bake
                                tl_tt_bakedchar *chardata)
{
   float scale;
   int x,y,bottom_y, i;
   tl_tt_fontinfo f;
   tl_tt_InitFont(&f, data, offset);
   STBTT_memset(pixels, 0, pw*ph); // background of 0 around pixels
   x=y=1;
   bottom_y = 1;

   scale = tl_tt_ScaleForPixelHeight(&f, pixel_height);

   for (i=0; i < num_chars; ++i) {
      int advance, lsb, x0,y0,x1,y1,gw,gh;
      int g = tl_tt_FindGlyphIndex(&f, first_char + i);
      tl_tt_GetGlyphHMetrics(&f, g, &advance, &lsb);
      tl_tt_GetGlyphBitmapBox(&f, g, scale,scale, &x0,&y0,&x1,&y1);
      gw = x1-x0;
      gh = y1-y0;
      if (x + gw + 1 >= pw)
         y = bottom_y, x = 1; // advance to next row
      if (y + gh + 1 >= ph) // check if it fits vertically AFTER potentially moving to next row
         return -i;
      STBTT_assert(x+gw < pw);
      STBTT_assert(y+gh < ph);
      tl_tt_MakeGlyphBitmap(&f, pixels+x+y*pw, gw,gh,pw, scale,scale, g);
      chardata[i].x0 = (int16_t) x;
      chardata[i].y0 = (int16_t) y;
      chardata[i].x1 = (int16_t) (x + gw);
      chardata[i].y1 = (int16_t) (y + gh);
      chardata[i].xadvance = scale * advance;
      chardata[i].xoff     = (float) x0;
      chardata[i].yoff     = (float) y0;
      x = x + gw + 2;
      if (y+gh+2 > bottom_y)
         bottom_y = y+gh+2;
   }
   return bottom_y;
}


void tl_tt_GetBakedQuad(tl_tt_bakedchar *chardata, int pw, int ph, int char_index, float *xpos, float *ypos, tl_tt_aligned_quad *q, int opengl_fillrule)
{
   float d3d_bias = opengl_fillrule ? 0 : -0.5f;
   float ipw = 1.0f / pw, iph = 1.0f / ph;
   tl_tt_bakedchar *b = chardata + char_index;
   int round_x = STBTT_ifloor((*xpos + b->xoff) + 0.5);
   int round_y = STBTT_ifloor((*ypos + b->yoff) + 0.5);

   q->x0 = round_x + d3d_bias;
   q->y0 = round_y + d3d_bias;
   q->x1 = round_x + b->x1 - b->x0 + d3d_bias;
   q->y1 = round_y + b->y1 - b->y0 + d3d_bias;

   q->s0 = b->x0 * ipw;
   q->t0 = b->y0 * ipw;
   q->s1 = b->x1 * iph;
   q->t1 = b->y1 * iph;

   *xpos += b->xadvance;
}

//////////////////////////////////////////////////////////////////////////////
//
// font name matching -- recommended not to use this
//

// check if a utf8 string contains a prefix which is the utf16 string; if so return length of matching utf8 string
static int32_t tl_tt__CompareUTF8toUTF16_bigendian_prefix(uint8_t *s1, int32_t len1, uint8_t *s2, int32_t len2)
{
   int32_t i=0;

   // convert utf16 to utf8 and compare the results while converting
   while (len2) {
      uint16_t ch = s2[0]*256 + s2[1];
      if (ch < 0x80) {
         if (i >= len1) return -1;
         if (s1[i++] != ch) return -1;
      } else if (ch < 0x800) {
         if (i+1 >= len1) return -1;
         if (s1[i++] != 0xc0 + (ch >> 6)) return -1;
         if (s1[i++] != 0x80 + (ch & 0x3f)) return -1;
      } else if (ch >= 0xd800 && ch < 0xdc00) {
         uint32_t c;
         uint16_t ch2 = s2[2]*256 + s2[3];
         if (i+3 >= len1) return -1;
         c = ((ch - 0xd800) << 10) + (ch2 - 0xdc00) + 0x10000;
         if (s1[i++] != 0xf0 + (c >> 18)) return -1;
         if (s1[i++] != 0x80 + ((c >> 12) & 0x3f)) return -1;
         if (s1[i++] != 0x80 + ((c >>  6) & 0x3f)) return -1;
         if (s1[i++] != 0x80 + ((c      ) & 0x3f)) return -1;
         s2 += 2; // plus another 2 below
         len2 -= 2;
      } else if (ch >= 0xdc00 && ch < 0xe000) {
         return -1;
      } else {
         if (i+2 >= len1) return -1;
         if (s1[i++] != 0xe0 + (ch >> 12)) return -1;
         if (s1[i++] != 0x80 + ((ch >> 6) & 0x3f)) return -1;
         if (s1[i++] != 0x80 + ((ch     ) & 0x3f)) return -1;
      }
      s2 += 2;
      len2 -= 2;
   }
   return i;
}

int tl_tt_CompareUTF8toUTF16_bigendian(const char *s1, int len1, const char *s2, int len2)
{
   return len1 == tl_tt__CompareUTF8toUTF16_bigendian_prefix((uint8_t*) s1, len1, (uint8_t*) s2, len2);
}

// returns results in whatever encoding you request... but note that 2-byte encodings
// will be BIG-ENDIAN... use tl_tt_CompareUTF8toUTF16_bigendian() to compare
char *tl_tt_GetFontNameString(const tl_tt_fontinfo *font, int *length, int platformID, int encodingID, int languageID, int nameID)
{
   int32_t i,count,stringOffset;
   uint8_t *fc = font->data;
   uint32_t offset = font->fontstart;
   uint32_t nm = tl_tt__find_table(fc, offset, "name");
   if (!nm) return NULL;

   count = ttUSHORT(fc+nm+2);
   stringOffset = nm + ttUSHORT(fc+nm+4);
   for (i=0; i < count; ++i) {
      uint32_t loc = nm + 6 + 12 * i;
      if (platformID == ttUSHORT(fc+loc+0) && encodingID == ttUSHORT(fc+loc+2)
          && languageID == ttUSHORT(fc+loc+4) && nameID == ttUSHORT(fc+loc+6)) {
         *length = ttUSHORT(fc+loc+8);
         return (char *) (fc+stringOffset+ttUSHORT(fc+loc+10));
      }
   }
   return NULL;
}

static int tl_tt__matchpair(uint8_t *fc, uint32_t nm, uint8_t *name, int32_t nlen, int32_t target_id, int32_t next_id)
{
   int32_t i;
   int32_t count = ttUSHORT(fc+nm+2);
   int32_t stringOffset = nm + ttUSHORT(fc+nm+4);

   for (i=0; i < count; ++i) {
      uint32_t loc = nm + 6 + 12 * i;
      int32_t id = ttUSHORT(fc+loc+6);
      if (id == target_id) {
         // find the encoding
         int32_t platform = ttUSHORT(fc+loc+0), encoding = ttUSHORT(fc+loc+2), language = ttUSHORT(fc+loc+4);

         // is this a Unicode encoding?
         if (platform == 0 || (platform == 3 && encoding == 1) || (platform == 3 && encoding == 10)) {
            int32_t slen = ttUSHORT(fc+loc+8), off = ttUSHORT(fc+loc+10);

            // check if there's a prefix match
            int32_t matchlen = tl_tt__CompareUTF8toUTF16_bigendian_prefix(name, nlen, fc+stringOffset+off,slen);
            if (matchlen >= 0) {
               // check for target_id+1 immediately following, with same encoding & language
               if (i+1 < count && ttUSHORT(fc+loc+12+6) == next_id && ttUSHORT(fc+loc+12) == platform && ttUSHORT(fc+loc+12+2) == encoding && ttUSHORT(fc+loc+12+4) == language) {
                  int32_t slen = ttUSHORT(fc+loc+12+8), off = ttUSHORT(fc+loc+12+10);
                  if (slen == 0) {
                     if (matchlen == nlen)
                        return 1;
                  } else if (matchlen < nlen && name[matchlen] == ' ') {
                     ++matchlen;
                     if (tl_tt_CompareUTF8toUTF16_bigendian((char*) (name+matchlen), nlen-matchlen, (char*)(fc+stringOffset+off),slen))
                        return 1;
                  }
               } else {
                  // if nothing immediately following
                  if (matchlen == nlen)
                     return 1;
               }
            }
         }

         // @TODO handle other encodings
      }
   }
   return 0;
}

static int tl_tt__matches(uint8_t *fc, uint32_t offset, uint8_t *name, int32_t flags)
{
   int32_t nlen = STBTT_strlen((char *) name);
   uint32_t nm,hd;
   if (!tl_tt__isfont(fc+offset)) return 0;

   // check italics/bold/underline flags in macStyle...
   if (flags) {
      hd = tl_tt__find_table(fc, offset, "head");
      if ((ttUSHORT(fc+hd+44) & 7) != (flags & 7)) return 0;
   }

   nm = tl_tt__find_table(fc, offset, "name");
   if (!nm) return 0;

   if (flags) {
      // if we checked the macStyle flags, then just check the family and ignore the subfamily
      if (tl_tt__matchpair(fc, nm, name, nlen, 16, -1))  return 1;
      if (tl_tt__matchpair(fc, nm, name, nlen,  1, -1))  return 1;
      if (tl_tt__matchpair(fc, nm, name, nlen,  3, -1))  return 1;
   } else {
      if (tl_tt__matchpair(fc, nm, name, nlen, 16, 17))  return 1;
      if (tl_tt__matchpair(fc, nm, name, nlen,  1,  2))  return 1;
      if (tl_tt__matchpair(fc, nm, name, nlen,  3, -1))  return 1;
   }

   return 0;
}

int tl_tt_FindMatchingFont(const unsigned char *font_collection, const char *name_utf8, int32_t flags)
{
   int32_t i;
   for (i=0;;++i) {
      int32_t off = tl_tt_GetFontOffsetForIndex(font_collection, i);
      if (off < 0) return off;
      if (tl_tt__matches((uint8_t *) font_collection, off, (uint8_t*) name_utf8, flags))
         return off;
   }
}
#endif