// GNUPLOT - block.trm 
// Copyright (c) 2020 Bastian Maerkisch. All rights reserved.
//
#include <gnuplot.h>
#pragma hdrstop
#include "driver.h"

// @experimental {
#define TERM_BODY
#define TERM_PUBLIC static
#define TERM_TABLE
#define TERM_TABLE_START(x) GpTermEntry x {
#define TERM_TABLE_END(x)   };
// } @experimental

#ifdef TERM_REGISTER
	register_term(block)
#endif

//#ifdef TERM_PROTO
TERM_PUBLIC void BLOCK_options(GpTermEntry * pThis, GnuPlot * pGp);
TERM_PUBLIC void BLOCK_init(GpTermEntry * pThis);
TERM_PUBLIC void BLOCK_reset(GpTermEntry * pThis);
TERM_PUBLIC void BLOCK_text(GpTermEntry * pThis);
TERM_PUBLIC void BLOCK_graphics(GpTermEntry * pThis);
TERM_PUBLIC void BLOCK_put_text(GpTermEntry * pThis, uint x, uint y, const char * str);
TERM_PUBLIC void BLOCK_point(GpTermEntry * pThis, uint x, uint y, int point);
TERM_PUBLIC void BLOCK_linetype(GpTermEntry * pThis, int linetype);
TERM_PUBLIC void BLOCK_dashtype(GpTermEntry * pThis, int type, t_dashtype * custom_dash_type);
TERM_PUBLIC void BLOCK_set_color(GpTermEntry * pThis, const t_colorspec * color);
//#endif

#ifndef TERM_PROTO_ONLY
#ifdef TERM_BODY

// default values for quadrants
#define BLOCK_XMAX  (DUMB_XMAX)
#define BLOCK_YMAX  (DUMB_YMAX)
#define BLOCK_VCHAR (2)
#define BLOCK_HCHAR (2)
// we want really small point symbols
// FIXME: Should we go for 1, 2? Aspect ratio?
#define BLOCK_VTIC  (2)
#define BLOCK_HTIC  (2)

enum BLOCK_modes {
	BLOCK_MODE_DOT, 
	BLOCK_MODE_HALF, 
	BLOCK_MODE_HALFH,
	BLOCK_MODE_QUAD, 
	BLOCK_MODE_SEXT, 
	BLOCK_MODE_BRAILLE
};

struct BLOCK_modeinfo_entry {
	short mode;
	short cellx;
	short celly;
};

#define USQR(a) (((uint)a)*((uint)a))

// internal functions for color mapping 
static void to_rgb255(GpTermEntry * pThis, int v, rgb255_color * rgb255);

static int BLOCK_mode = BLOCK_MODE_QUAD;
static int BLOCK_xchars = BLOCK_XMAX;
static int BLOCK_ychars = BLOCK_YMAX;
static bool BLOCK_optimize = TRUE;
static bool BLOCK_charpoints = FALSE;
static const BLOCK_modeinfo_entry BLOCK_modeinfo[6] = {
	{ BLOCK_MODE_DOT,     1, 1 },
	{ BLOCK_MODE_HALF,    1, 2 },
	{ BLOCK_MODE_HALFH,   2, 1 },
	{ BLOCK_MODE_QUAD,    2, 2 },
	{ BLOCK_MODE_SEXT,    2, 3 },
	{ BLOCK_MODE_BRAILLE, 2, 4 }
};

enum BLOCK_id {
	BLOCK_ENH, BLOCK_NOENH,
	// The code relies on those having the same order as the modes.
	BLOCK_DOT, BLOCK_HALF, BLOCK_HALFH,
	BLOCK_QUADRANTS, BLOCK_SEXTANTS, BLOCK_BRAILLE,
	BLOCK_ANSI, BLOCK_ANSI256, BLOCK_ANSIRGB, BLOCK_NOCOLOR,
	BLOCK_OPTIMIZE, BLOCK_NOOPTIMIZE,
	BLOCK_CHARPOINTS, BLOCK_GPPOINTS,
	BLOCK_SIZE,
	BLOCK_OTHER
};

static struct gen_table BLOCK_opts[] = {
	{ "enh$anced", BLOCK_ENH },
	{ "noe$nhanced", BLOCK_NOENH },
	{ "dot", BLOCK_DOT },
	{ "half", BLOCK_HALF },
	{ "halfh", BLOCK_HALFH },
	{ "quad$rants", BLOCK_QUADRANTS },
	{ "sex$tants", BLOCK_SEXTANTS },
	{ "br$aille", BLOCK_BRAILLE },
	{ "ansi", BLOCK_ANSI },
	{ "ansi256", BLOCK_ANSI256 },
	{ "pal$ette", BLOCK_ANSI256 },
	{ "ansirgb", BLOCK_ANSIRGB },
	{ "tru$ecolor", BLOCK_ANSIRGB },
	{ "opt$imize", BLOCK_OPTIMIZE },
	{ "noopt$imize", BLOCK_NOOPTIMIZE },
	{ "chars$ymbols", BLOCK_CHARPOINTS },
	{ "gps$ymbols", BLOCK_GPPOINTS },
	{ "mono", BLOCK_NOCOLOR },
	{ "siz$e", BLOCK_SIZE },
	{ NULL, BLOCK_OTHER }
};

TERM_PUBLIC void BLOCK_options(GpTermEntry * pThis, GnuPlot * pGp)
{
	int cmd;
	while(!pGp->Pgm.EndOfCommand()) {
		switch(cmd = pGp->Pgm.LookupTableForCurrentToken(&BLOCK_opts[0])) {
#ifndef NO_DUMB_ENHANCED_SUPPORT
			case BLOCK_ENH:
			    pGp->Pgm.Shift();
			    pThis->flags |= TERM_ENHANCED_TEXT;
			    break;
			case BLOCK_NOENH:
			    pGp->Pgm.Shift();
			    pThis->flags &= ~TERM_ENHANCED_TEXT;
			    break;
#endif
			case BLOCK_DOT:
			case BLOCK_HALF:
			case BLOCK_HALFH:
			case BLOCK_QUADRANTS:
			case BLOCK_SEXTANTS:
			case BLOCK_BRAILLE:
			    BLOCK_mode = BLOCK_MODE_DOT + cmd - BLOCK_DOT;
			    pGp->Pgm.Shift();
			    break;
			case BLOCK_ANSI:
			case BLOCK_ANSI256:
			case BLOCK_ANSIRGB:
			    pGp->Pgm.Shift();
			    if(cmd == BLOCK_ANSI)
				    pGp->TDumbB.ColorMode = DUMB_ANSI;
			    else if(cmd == BLOCK_ANSI256)
				    pGp->TDumbB.ColorMode = DUMB_ANSI256;
			    else
				    pGp->TDumbB.ColorMode = DUMB_ANSIRGB;
#ifndef NO_DUMB_COLOR_SUPPORT
			    pThis->make_palette = DUMB_make_palette;
			    pThis->flags &= ~TERM_MONOCHROME;
#endif
			    break;
			case BLOCK_NOCOLOR:
			    pGp->Pgm.Shift();
			    pGp->TDumbB.ColorMode = 0;
			    pThis->make_palette = NULL;
			    pThis->flags |= TERM_MONOCHROME;
			    break;
			case BLOCK_OPTIMIZE:
			    pGp->Pgm.Shift();
			    BLOCK_optimize = TRUE;
			    break;
			case BLOCK_NOOPTIMIZE:
			    pGp->Pgm.Shift();
			    BLOCK_optimize = FALSE;
			    break;
			case BLOCK_CHARPOINTS:
			    pGp->Pgm.Shift();
			    BLOCK_charpoints = TRUE;
			    break;
			case BLOCK_GPPOINTS:
			    pGp->Pgm.Shift();
			    BLOCK_charpoints = FALSE;
			    break;
			case BLOCK_SIZE: {
			    float width, height;
			    pGp->Pgm.Shift();
			    pGp->ParseTermSize(&width, &height, PIXELS);
			    BLOCK_xchars = static_cast<int>(width);
			    BLOCK_ychars = static_cast<int>(height);
			    break;
		    }
			case BLOCK_OTHER:
			default:
			    pGp->IntErrorCurToken("unknown option");
		}
	}
	// initialize terminal canvas size
	pThis->MaxX = BLOCK_modeinfo[BLOCK_mode].cellx * BLOCK_xchars - 1;
	pThis->MaxY = BLOCK_modeinfo[BLOCK_mode].celly * BLOCK_ychars - 1;
	// init options string
	{
		const char * mode_strings[] = { "dot", "half", "halfh", "quadrants", "sextants", "braille" };
		const char * color_strings[] = {"mono", "ansi", "ansi256", "ansirgb"};
		GPT._TermOptions.Printf("%s %s %soptimize %senhanced size %d,%d", mode_strings[BLOCK_mode],
		    color_strings[pGp->TDumbB.ColorMode == 0 ? 0 : pGp->TDumbB.ColorMode - DUMB_ANSI + 1],
		    (BLOCK_optimize ? "" : "no"), (pThis->flags & TERM_ENHANCED_TEXT) ? "" : "no", BLOCK_xchars, BLOCK_ychars);
	}
}

TERM_PUBLIC void BLOCK_init(GpTermEntry * pThis)
{
	GnuPlot * p_gp = pThis->P_Gp;
	// LSB is "opacity" 
	switch(p_gp->TDumbB.ColorMode) {
		case 0:
		    p_gp->BmpMakeBitmap(pThis->MaxX + 1, pThis->MaxY + 1, 1);
		    break;
		case DUMB_ANSI:
		    p_gp->BmpMakeBitmap(pThis->MaxX + 1, pThis->MaxY + 1, 5);
		    break;
		case DUMB_ANSI256:
		    p_gp->BmpMakeBitmap(pThis->MaxX + 1, pThis->MaxY + 1, 9);
#ifdef BLOCK_DEBUG_256
		    for(int i = 16; i <= 255; i++) {
			    rgb255_color c;
			    uint v;
			    to_rgb255(pThis, i, &c);
			    v = c.ToAnsi256();
			    if(i != v)
				    printf("i=%i -> %02x,%02x,%02x -> %i\n", i, c.r, c.g, c.b, v);
		    }
#endif
		    break;
		case DUMB_ANSIRGB:
		    p_gp->BmpMakeBitmap(pThis->MaxX + 1, pThis->MaxY + 1, 25);
		    break;
	}
	pThis->ChrH = BLOCK_modeinfo[BLOCK_mode].cellx;
	pThis->ChrV = BLOCK_modeinfo[BLOCK_mode].celly;
	// FIXME: is this correct or should the -1 go?
	p_gp->TDumbB.PtMax.Set(BLOCK_xchars - 1, BLOCK_ychars - 1);
	DUMB_init(pThis);
}

TERM_PUBLIC void BLOCK_reset(GpTermEntry * pThis)
{
	pThis->P_Gp->BmpFreeBitmap();
	DUMB_reset(pThis);
}
//
// Halfblocks, vertical:
//
static const uint32_t block_half_map[4] = { 0x00A0, 0x2580, 0x2584, 0x2588 };
//
// Halfblocks, horizontal:
//
static const uint32_t block_halfh_map[4] = { 0x00A0, 0x2590, 0x258D, 0x2588 };
/*
   ZX-81 like box drawing map:
    1 2
    3 4
 */
static const uint32_t block_quadrant_map[16] = {
	0x00A0, 0x2598, 0x259D, 0x2580, //  0,   1,   2,   12
	0x2596, 0x258C, 0x259E, 0x259B, //  3,  13,  23,  123
	0x2597, 0x259A, 0x2590, 0x259C, //  4,  14,  24,  124
	0x2584, 0x2599, 0x259F, 0x2588 // 34, 134, 234, 1234
};

/*
   Sextant map. Unfortunately non-continuous.
    1 2
    3 4
    5 6
 */
static uint32_t block_sextant_map[64] = {
	0x00A0,  0x1FB00,  0x1FB01,  0x1FB02,// *0* (empty), 1, 2, 12
	0x1FB03,  0x1FB04,  0x1FB05,  0x1FB06,// 3, 13, 23, 123
	0x1FB07,  0x1FB08,  0x1FB09,  0x1FB0A,// 4, 14, 24, 124
	0x1FB0B,  0x1FB0C,  0x1FB0D,  0x1FB0E,// 34, 134, 234, 1234,

	0x1FB0F,  0x1FB10,  0x1FB11,  0x1FB12,// 5, 15, 25, 125
	0x1FB13,   0x258C,  0x1FB14,  0x1FB15,// 35, *135* (left half), 235, 1235
	0x1FB16,  0x1FB17,  0x1FB18,  0x1FB19,// 45, 145, 245, 1245
	0x1FB1A,  0x1FB1B,  0x1FB1C,  0x1FB1D,// 345, 1345, 2345, 12345

	0x1FB1E,  0x1FB1F,  0x1FB20,  0x1FB21,// 6, 16, 26, 126
	0x1FB22,  0x1FB23,  0x1FB24,  0x1FB25,// 36, 136, 236, 1236
	0x1FB26,  0x1FB27,   0x2590,  0x1FB28,// 46, 146, *246* (right half), 1246
	0x1FB29,  0x1FB2A,  0x1FB2B,  0x1FB2C,// 346, 1346, 2346, 12346,

	0x1FB2D,  0x1FB1E,  0x1FB2F,  0x1FB30,// 56, 156, 256, 1256
	0x1FB31,  0x1FB32,  0x1FB33,  0x1FB34,// 356, 1356, 2356, 12356
	0x1FB35,  0x1FB36,  0x1FB37,  0x1FB38,// 456, 1456, 2456, 12456
	0x1FB39,  0x1FB3A,  0x1FB3B,   0x2588,// 3456, 13456, 23456, *12345* (full)
};

/* optimization bit patterns */
const int quad_pat[10] = {
	0x0e, 0x0d, 0x0b, 0x07, // 3 bits
	0x0c, 0x0a, 0x09,    // 2 bits
	0x06, 0x05,
	0x03
};
const int nquad_pats = sizeof(quad_pat) / sizeof(int);
const int sext_pat[] = {
	0x3e, 0x3d, 0x3b, 0x37, 0x2f, 0x1f, // 5 bits
	0x3c, 0x3a, 0x39, 0x36, 0x35, 0x33,
	0x2e, 0x2d, 0x2b, 0x27,          // 4 bits
	0x1e, 0x1d, 0x1b, 0x17,
	0x0f,
	0x38, 0x34, 0x32, 0x31,          // 3 bits
	0x2c, 0x2a, 0x29, 0x26, 0x25, 0x23,
	0x1c, 0x1a, 0x19, 0x16, 0x15, 0x13,
	0x0e, 0x0d, 0x0b, 0x07
};
const int nsext_pats = sizeof(sext_pat) / sizeof(int);

#ifndef NO_DUMB_COLOR_SUPPORT
static void to_rgb255(GpTermEntry * pThis, int v, rgb255_color * rgb255)
{
	switch(pThis->P_Gp->TDumbB.ColorMode) {
		case DUMB_ANSIRGB:
		    // direct RGB
		    rgb255->r = (v >> 16) & 0xff;
		    rgb255->g = (v >>  8) & 0xff;
		    rgb255->b = (v >>  0) & 0xff;
		    break;
		case DUMB_ANSI:
		case DUMB_ANSI256:
		    if(v < 16) {
			    // "standard" colors
			    v = rgb255_color::AnsiTab16[v];
			    rgb255->r = ((v >> 0) & 0x0f) << 4;
			    rgb255->g = ((v >> 4) & 0x0f) << 4;
			    rgb255->b = ((v >> 8) & 0x0f) << 4;
		    }
		    else if(v < 232) {
			    // 6x6x6 color cube
			    v -= 16;
#define MAPCUBE6(n) ((n) ? (n) * 40 + 55 : 0)
			    rgb255->r = MAPCUBE6((v / 36) % 6);
			    rgb255->g = MAPCUBE6((v /  6) % 6);
			    rgb255->b = MAPCUBE6((v     ) % 6);
		    }
		    else {
			    // gray scale
			    v = (v - 232) * 10 + 8; // like Mintty
			    rgb255->r = rgb255->b = rgb255->g = v;
		    }
		    break;
	}
}

static const char * ansi_bg_colorstring(GpTermEntry * pThis, t_colorspec * color)
{
	if(pThis->P_Gp->TDumbB.ColorMode == DUMB_ANSI) {
		// This needs special attention due to multiple attributes being used.
		rgb255_color rgb255;
		uint   n;
		static char colorstring[32];
		rgb255.r = (color->lt >> 16) & 0xff;
		rgb255.g = (color->lt >>  8) & 0xff;
		rgb255.b = (color->lt >>  0) & 0xff;
		n = rgb255.NearestAnsi();
		if(n <= 8) {
			sprintf(colorstring, "\033[%im", 40 + (n % 8));
		}
		else {
			// For color >= 8, we try to set both, the (dark) background color,
			// and then the brighter color using the aixterm sequence. Unfortunately,
			// that is not widely supported, so we likely get the right color, but
			// it is too dark.
			sprintf(colorstring, "\033[%i;%im", 40 + (n % 8), (n >= 8 ? 100 : 40) + (n % 8));
		}
		return colorstring;
	}
	else {
		// Hack alert: We change foreground color commands to
		// background color commands by changing a single byte only.
		char * c = (char *)pThis->P_Gp->AnsiColorString(color, NULL);
		if(c[0] && c[2] == '3')
			c[2] = '4';
		return c;
	}
}

#endif

TERM_PUBLIC void BLOCK_text(GpTermEntry * pThis)
{
	GnuPlot * p_gp = pThis->P_Gp;
	char * line;
	uchar * s;
	int x, y;
	uint32_t v;
	int cellx, celly; // number of pseudo-pixels per cell
	const int * idx;
	const int idx_dot[1]       = { 1 };
	const int idx_half[2]      = { 0x02, 0x01 };
	const int idx_quadrants[4] = { 0x04, 0x08, 0x01, 0x02 };
	const int idx_sextants[6]  = { 0x10, 0x20, 0x04, 0x08, 0x01, 0x02 };
	const int idx_braille[8]   = { 0x40, 0x80, 0x04, 0x20, 0x02, 0x10, 0x01, 0x08 };
	const int * idx_list[6] = { idx_dot, idx_half, idx_half, idx_quadrants, idx_sextants, idx_braille };
#ifndef NO_DUMB_COLOR_SUPPORT
#ifdef DEBUG_BLOCK_OPTI
	uint nfullcells = 0;
	uint noptimisable = 0;
	uint noptimised = 0;
#endif
	uint   nsets, set;
	const  int * pattern;
	size_t bufsiz;
	if(p_gp->TDumbB.ColorMode > 0) {
		fputs("\033[0;39m", GPT.P_GpOutFile); /* reset colors to default */
		memzero(&p_gp->TDumbB.PrevColor, sizeof(t_colorspec));
	}
#endif
	// Initialize variable for loop below.
	cellx = BLOCK_modeinfo[BLOCK_mode].cellx;
	celly = BLOCK_modeinfo[BLOCK_mode].celly;
	idx = idx_list[BLOCK_mode];
#ifndef NO_DUMB_COLOR_SUPPORT
	switch(BLOCK_mode) {
		case BLOCK_MODE_HALF:
		case BLOCK_MODE_HALFH:
		    nsets = 0;
		    pattern = NULL;
		    break;
		case BLOCK_MODE_QUAD:
		    nsets = nquad_pats;
		    pattern = quad_pat;
		    break;
		case BLOCK_MODE_SEXT:
		    nsets = nsext_pats;
		    pattern = sext_pat;
		    break;
	}
#endif
	// convert bitmap Unicode characters
	// printf("vvvvvvvvvvvvvvvvvvvvvvvvvvvv\n");
	// FIXME: last row and column not handled properly!
	bufsiz = (pThis->MaxX + 1) * 8;
	line = (char *)SAlloc::M(bufsiz);
	for(y = pThis->MaxY - celly + 1; y >= 0; y -= celly) {
		int yd = y / celly; // character cell coordinate
		s = (uchar *)line;
		for(x = 0; x <= static_cast<int>(pThis->MaxX); x += cellx) {
			int xd = x / cellx; // character cell coordinate
			int i = 0; // index to character map
			bool resetbg = FALSE;
			const char * c;
			t_colorspec color;
			// increase buffer size if necessary
			if(((char *)s - line) > static_cast<ssize_t>(bufsiz - 50)) {
				bufsiz += (pThis->MaxX + 1) * 8;
				char * l = (char *)SAlloc::R(line, bufsiz);
				s = (uchar *)(l + ((char *)s - line));
				line = l;
			}
			if(*reinterpret_cast<const char *>(&p_gp->TDumbB.Pixel(xd, yd)) != ' ') {
				//
				// There is a character in the charcell.
				//
#ifndef NO_DUMB_COLOR_SUPPORT
				// Handle the character's color first.
				t_colorspec * color = &p_gp->TDumbB.P_Colors[p_gp->TDumbB.PtMax.x * yd + xd];
				c = p_gp->AnsiColorString(color, &p_gp->TDumbB.PrevColor);
				if(c[0]) {
					strcpy((char *)s, c);
					s += strlen(c);
				}
				memcpy(&p_gp->TDumbB.PrevColor, color, sizeof(t_colorspec));
#endif
				// Copy the character.
				c = reinterpret_cast<const char *>(&p_gp->TDumbB.Pixel(xd, yd));
				for(i = 0; i < sizeof(charcell) && *c; i++, c++, s++)
					*s = *c;
			}
			else {
				//
				// No character. Print graphics.
				//
				int k;
				int n = 0; // number of set pixels in charcell
				uint cell[8]; // array of all pixels in charcell
#ifndef NO_DUMB_COLOR_SUPPORT
				int r = 0, g = 0, b = 0; // color components
				rgb255_color col[8];
#endif
				// get all pixels within charcell
				for(k = 0; k < (cellx * celly); k++) {
					cell[k] = v = p_gp->_Bmp.GetPixel(x + (k % cellx), y + (k / cellx));
					// Is the pixel set? (first plane is "opacity")
					if(v & 1) {
						// update character index
						i += idx[k];
#ifndef NO_DUMB_COLOR_SUPPORT
						// sum up RGB colors
						to_rgb255(pThis, v >> 1, col + k);
						r += USQR(col[k].r);
						g += USQR(col[k].g);
						b += USQR(col[k].b);
						n++;
#endif
					}
				}
#ifndef NO_DUMB_COLOR_SUPPORT
#ifdef DEBUG_BLOCK_OPTI
				if(n == cellx * celly) {
					bool issame = TRUE;
					nfullcells++;
					for(int kk = 1; kk < (cellx*celly); kk++)
						issame &= (cell[0] == cell[kk]);
					if(!issame)
						noptimisable++;
				}
#endif
				// In case all pixels of the charcell are set, we do try to optimize
				// changing both, fore- and background colors. This always
				// works for half-blocks, mostly for quadrants, but is increasingly
				// difficult for sextants. We cannot do that for Braille symbols.
				if((p_gp->TDumbB.ColorMode > 0) && BLOCK_optimize && ((((BLOCK_mode == BLOCK_MODE_HALF) || (BLOCK_mode == BLOCK_MODE_HALFH)) &&/* @sobolev &-->&& */
					(n == 2) && !(cell[0] == cell[1])) ||
				    ((BLOCK_mode == BLOCK_MODE_QUAD) && (n == 4) && !(cell[0] == cell[1] && cell[1] == cell[2] && cell[2] == cell[3])) ||
				    ((BLOCK_mode == BLOCK_MODE_SEXT) && (n == 6) && !(cell[0] == cell[1] && cell[1] == cell[2] && cell[2] == cell[3] &&
				    cell[3] == cell[4] && cell[4] == cell[5])))) {
					bool found = (n == 2);
					uint mask, first;
					for(set = 0; set < nsets && !found; set++) {
						// The pattern list entries really are bit-maps of the
						// pixel combinations to test.
						mask = pattern[set];
						// look for the first pixel set
						first = 0;
						while((mask & 1) == 0) {
							mask >>= 1;
							first++;
						}
						// now test the others for equality
						for(k = first + 1, found = TRUE; k < cellx*celly; k++) {
							mask >>= 1;
							if(mask & 1)
								found &= (cell[first] == cell[k]);
						}
					}
					if(found) {
						int r2 = 0, g2 = 0, b2 = 0;
						int m = 0;
						r = g = b = 0;
						n = i = 0;
#ifdef DEBUG_BLOCK_OPTI
						noptimised++;
#endif
						if(oneof2(BLOCK_mode, BLOCK_MODE_HALF, BLOCK_MODE_HALFH))
							mask = 0x01;
						else
							mask = pattern[set-1];
						for(k = 0; k < cellx*celly; k++) {
							if(mask & 1) {
								r += USQR(col[k].r);
								g += USQR(col[k].g);
								b += USQR(col[k].b);
								i += idx[k];
								n++;
							}
							else {
								r2 += USQR(col[k].r);
								g2 += USQR(col[k].g);
								b2 += USQR(col[k].b);
								m++;
							}
							mask >>= 1;
						}
						color.type = TC_RGB;
						color.lt = ((uint)sqrt(r2 / m) << 16) + ((uint)sqrt(g2 / m) <<  8) + ((uint)sqrt(b2 / m));
						c = ansi_bg_colorstring(pThis, &color);
						if(c) {
							strcpy((char *)s, c);
							s += strlen(c);
							resetbg = TRUE;
						}
					}
				}
				// Any pixel set within this charcell?
				// Then average over the color over the charcell.
				if(n != 0) {
					// average charcell color
					color.type = TC_RGB;
					color.lt = ((uint)sqrt(r / n) << 16) + ((uint)sqrt(g / n) <<  8) + ((uint)sqrt(b / n));
					c = p_gp->AnsiColorString(&color, &p_gp->TDumbB.PrevColor);
					if(c[0]) {
						strcpy((char *)s, c);
						s += strlen(c);
					}
					memcpy(&p_gp->TDumbB.PrevColor, &color, sizeof(t_colorspec));
				}
#endif
				switch(BLOCK_mode) {
					case BLOCK_MODE_DOT:
					    // v = (n > 0 ? '*' : ' ');
					    v = (n > 0 ? 0x25CF : 0x00A0);
					    break;
					case BLOCK_MODE_HALF:
					    v = block_half_map[i];
					    break;
					case BLOCK_MODE_HALFH:
					    v = block_halfh_map[i];
					    break;
					case BLOCK_MODE_QUAD:
					    v = block_quadrant_map[i];
					    break;
					case BLOCK_MODE_SEXT:
					    v = block_sextant_map[i];
					    break;
					case BLOCK_MODE_BRAILLE:
					    // Braille characters are well-ordered,
					    // so we don't need a mapping.
					    v = 0x2800 + i;
					    break;
				}
				s += ucs4toutf8(v, s);

#ifndef NO_DUMB_COLOR_SUPPORT
				// turn bg color off again
				if(resetbg) {
					sprintf((char *)s, "\033[49m"); s += 5;
					sprintf((char *)s, "\033[49m"); s += 5;
				}
#endif
			}
		}
		*s++ = '\n';
		*s = '\0';
		fputs(line, GPT.P_GpOutFile);
	}
	// printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

#ifndef NO_DUMB_COLOR_SUPPORT
	if(p_gp->TDumbB.ColorMode > 0) {
		fputs("\033[0;39;49m", GPT.P_GpOutFile); /* reset colors to default */
	}
#ifdef DEBUG_BLOCK_OPTI
	printf("full cells : %u (%3.1f\%)\n", nfullcells, 100 * ((float)nfullcells) / BLOCK_xchars / BLOCK_ychars);
	printf("optimisable: %u (%3.1f\%)\n", noptimisable, 100 * ((float)noptimisable) / nfullcells);
	printf("optimised  : %u (%3.1f\%)\n", noptimised, 100 * ((float)noptimised) / noptimisable);
#endif
#endif
	SAlloc::F(line);
	fflush(GPT.P_GpOutFile); /* finish the graphics */
}

TERM_PUBLIC void BLOCK_graphics(GpTermEntry * pThis)
{
	GnuPlot * p_gp = pThis->P_Gp;
	b_boxfill(pThis, FS_EMPTY, 0, 0, pThis->MaxX, pThis->MaxY);
	DUMB_graphics(pThis);
	// kludge: fix keybox size 
	if(p_gp->Gg.KeyT.width_fix == 0) 
		p_gp->Gg.KeyT.width_fix = 1;
	if(p_gp->Gg.KeyT.height_fix == 0) 
		p_gp->Gg.KeyT.height_fix = 1;
}

TERM_PUBLIC void BLOCK_put_text(GpTermEntry * pThis, uint x, uint y, const char * str)
{
	int xd = x / BLOCK_modeinfo[BLOCK_mode].cellx;
	int yd = y / BLOCK_modeinfo[BLOCK_mode].celly;
#ifndef NO_DUMB_ENHANCED_SUPPORT
	if(pThis->flags & TERM_ENHANCED_TEXT)
		ENHdumb_put_text(pThis, xd, yd, str);
	else
#endif
	DUMB_put_text(pThis, xd, yd, str);
}

TERM_PUBLIC void BLOCK_point(GpTermEntry * pThis, uint x, uint y, int point)
{
	GnuPlot * p_gp = pThis->P_Gp;
	int xd = x / BLOCK_modeinfo[BLOCK_mode].cellx;
	int yd = y / BLOCK_modeinfo[BLOCK_mode].celly;
	const uint32_t pointtypes[] = {
		'+', 'x' /* cross */, '*',
		0x25a1, 0x25a0, /* empty/full square */
		0x25cb, 0x25cf, /* empty/full circle */
		0x25b3, 0x25b2, /* empty/full upward triangle */
		0x25bd, 0x25bc, /* empty/full downward triangle */
		0x25c7, 0x25c6, /* empty/full diamond */
		0x2b20, 0x2b1f, /* empty/full pentagon */
	};
	if(!BLOCK_charpoints) {
		GnuPlot::DoPoint(pThis, x, y, point);
		return;
	}
	if((signed)x < 0 || (signed)y < 0)
		return;
	if(point >= 0) {
		p_gp->TDumbB.Pixel(xd, yd) = 0;
		ucs4toutf8(pointtypes[point % 15], (uchar *)&p_gp->TDumbB.Pixel(xd, yd));
#ifndef NO_DUMB_COLOR_SUPPORT
		memcpy(&p_gp->TDumbB.P_Colors[p_gp->TDumbB.PtMax.x * yd + xd], &p_gp->TDumbB.Color, sizeof(t_colorspec));
#endif
	}
	else {
		b_move(pThis, x, y);
		b_vector(pThis, x + 1, y);
	}
}

TERM_PUBLIC void BLOCK_linetype(GpTermEntry * pThis, int linetype)
{
	t_colorspec color;
	// set dash pattern
	if(linetype != LT_AXIS)
		b_setlinetype(pThis, LT_SOLID); // always solid
	else
		b_setlinetype(pThis, LT_AXIS);
	// set line color
	color.type = TC_LT;
	color.lt = linetype;
	BLOCK_set_color(pThis, &color);
	// set text color
	DUMB_linetype(pThis, linetype);
}

TERM_PUBLIC void BLOCK_dashtype(GpTermEntry * pThis, int type, t_dashtype * custom_dash_type)
{
	if(type >= 0)
		;
	else if(type == DASHTYPE_AXIS)
		type = LT_AXIS;
	else
		type = LT_SOLID; /* solid, also for custom dash types */
	b_setlinetype(pThis, type);
}

TERM_PUBLIC void BLOCK_set_color(GpTermEntry * pThis, const t_colorspec * color)
{
	GnuPlot * p_gp = pThis->P_Gp;
	switch(color->type) {
		case TC_LT: {
		    if(color->lt == LT_BACKGROUND)
			    b_setvalue(pThis, 0);
		    else {
			    // map line type to colors 
			    int n = color->lt + 1;
			    if(n <= 0)
				    n = 7; // should be "default", but choose white instead
			    else if(n > 15) n = ((n - 1) % 15) + 1;
			    switch(p_gp->TDumbB.ColorMode) {
				    case 0:
					b_setvalue(pThis, 1);
					break;
#ifndef NO_DUMB_COLOR_SUPPORT
				    case DUMB_ANSI:
				    case DUMB_ANSI256:
					b_setvalue(pThis, (n << 1) + 1);
					break;
				    case DUMB_ANSIRGB: {
					uint v = rgb255_color::AnsiTab16[n];
					uint r = (v >> 0) & 0x0f;
					uint g = (v >> 4) & 0x0f;
					uint b = (v >> 8) & 0x0f;
					b_setvalue(pThis, (((r << 20) + (g << 12) + (b << 4)) << 1) + 1);
					break;
				}
#endif
			    }
		    }
		    break;
	    }
#ifndef NO_DUMB_COLOR_SUPPORT
		case TC_RGB:
		    if(p_gp->TDumbB.ColorMode == DUMB_ANSIRGB) {
			    b_setvalue(pThis, ((color->lt & 0xffffff) << 1) + 1);
		    }
		    else {
			    rgb255_color rgb255;
			    rgb255.r = (color->lt >> 16) & 0xff;
			    rgb255.g = (color->lt >>  8) & 0xff;
			    rgb255.b = (color->lt >>  0) & 0xff;
			    if(p_gp->TDumbB.ColorMode == DUMB_ANSI256)
				    b_setvalue(pThis, (rgb255.ToAnsi256() << 1) + 1);
			    else
				    b_setvalue(pThis, (rgb255.NearestAnsi() << 1) + 1);
		    }
		    break;
		case TC_FRAC: {
		    rgb255_color rgb255;
		    p_gp->Rgb255MaxColorsFromGray(color->value, &rgb255);
		    if(p_gp->TDumbB.ColorMode == DUMB_ANSIRGB) {
			    uint color = (((uint)rgb255.r) << 16) | (((uint)rgb255.g) <<  8) | ((uint)rgb255.b);
			    b_setvalue(pThis, ((color & 0xffffff) << 1) + 1);
		    }
		    else if(p_gp->TDumbB.ColorMode == DUMB_ANSI256)
			    b_setvalue(pThis, (rgb255.ToAnsi256() << 1) + 1);
		    else
			    b_setvalue(pThis, (rgb255.NearestAnsi() << 1) + 1);
		    break;
	    }
#endif
		default:
		    // should never happen
		    break;
	}
#ifndef NO_DUMB_COLOR_SUPPORT
	if(p_gp->TDumbB.ColorMode > 0)
		DUMB_set_color(pThis, color);
#endif
}

#endif /* TERM_BODY */

#ifdef TERM_TABLE

TERM_TABLE_START(block_driver)
	"block", 
	"Pseudo-graphics using Unicode blocks or Braille characters",
	BLOCK_XMAX, 
	BLOCK_YMAX,
	BLOCK_VCHAR, 
	BLOCK_HCHAR,
	BLOCK_VTIC, 
	BLOCK_HTIC,
	BLOCK_options, 
	BLOCK_init, 
	BLOCK_reset,
	BLOCK_text, 
	GnuPlot::NullScale, 
	BLOCK_graphics,
	b_move, 
	b_vector, 
	BLOCK_linetype,
	BLOCK_put_text, 
	GnuPlot::NullTextAngle, 
	GnuPlot::NullJustifyText,
	BLOCK_point, 
	GnuPlot::DoArrow, 
	set_font_null, 
	0,
	#ifndef NO_DUMB_ENHANCED_SUPPORT
	TERM_MONOCHROME | TERM_ENHANCED_TEXT,
	#else
	TERM_MONOCHROME,
	#endif
	0, 
	0, 
	b_boxfill, 
	b_linewidth,
	#ifdef USE_MOUSE
	NULL, 
	NULL, 
	NULL, 
	NULL, 
	NULL,
	#endif
	NULL, 
	NULL, 
	BLOCK_set_color,
	b_filled_polygon,
	NULL,     /* image */
	#ifndef NO_DUMB_ENHANCED_SUPPORT
	ENHdumb_OPEN, 
	ENHdumb_FLUSH, 
	do_enh_writec,
	#else
	NULL, 
	NULL, 
	NULL,
	#endif /* NO_DUMB_ENHANCED_SUPPORT */
	NULL,
	NULL,
	0.,
	NULL,
	NULL,
	NULL,
	BLOCK_dashtype 
TERM_TABLE_END(block_driver)

#undef LAST_TERM
#define LAST_TERM block_driver

#endif /* TERM_TABLE */
#endif /* TERM_PROTO_ONLY */

#ifdef TERM_HELP
START_HELP(block)
"1 block",
"?set terminal block",
"?block",
" The `block` terminal generates pseudo-graphic output using Unicode block or",
" Braille characters to increase the resolution.  It is an alternative for",
" terminal graphics.  It requires a UTF-8 capable terminal or viewer.  Drawing",
" uses the old internal bitmap code.  Text is printed on top of the graphics",
" using the `dumb` terminal's routines.",
"",
" Syntax:",
"         set term block",
"                 {dot | half | quadrants | sextants | braille}",
"                 {{no}enhanced}",
"                 {size <x>, <y>}",
#ifndef NO_DUMB_COLOR_SUPPORT
"                 {mono | ansi | ansi256 | ansirgb}",
"                 {{no}optimize}",
#endif
"                 {charpoints | gppoints}",
"",
" `size` sets the terminal size in character cells.",
"",
" `dot`, `half`, `quadrants`, `sextants`, or `braille` select the character",
" set used for the creation of pseudo-graphics.  `dots` uses simple dots,",
" while `half` uses half-block characters.  `quadrants` uses block characters",
" which yield double resolution in both directions.  `sextants` uses 2x3 block",
" characters, and `braille` uses Braille characters which give a 2x4 pseudo-",
" resolution.",
"",
" Note that the 2x3 block characters have only been included in Unicode 13.",
" Hence, font support is still limited.  Similarly for Braille.  Usable fonts",
" include e.g. `unscii`, `IBM 3270`, `GNU Unifont`, and `DejaVu Sans`.",
#ifndef NO_DUMB_COLOR_SUPPORT
"",
" The `ansi`, `ansi256`, and `ansirgb` options will include escape sequences",
" in the output to output colors.  Note that these might not be handled by",
" your terminal.  Default is `mono`.  See `terminal dumb` for a list of",
" escape sequences.",
"",
" Using block characters increases the pseudo-resolution of the bitmap.  But",
" this is not the case for the color.  Multiple 'pixels' necessarily share the",
" same color.  This is dealt with by averaging the color of all pixels in a",
" charcell.  With `optimize`, the terminal tries to improve by setting both,",
" the background and foreground colors.  This trick works perfectly for the",
" `half` mode, but is increasingly difficult for `quadrants` or `sextants`.",
" For `braille` this cannot be used.",
#endif
"",
" `gppoints` aims to draw point symbols using graphics commands. Due to the",
" low resolution of the terminal this is mostly viable for `braille` mode.",
" `charpoints` uses Unicode symbol characters instead.  Note that these",
" are also always drawn on top of the graphics."
END_HELP(block)
#endif
