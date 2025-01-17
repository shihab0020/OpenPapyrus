// GNUPLOT - pstricks.trm 
// Copyright 1990 - 1993, 1998, 2004, 2018
/*
 * This file is included by ../term.c.
 *
 * This terminal driver supports:
 *   The PSTricks macros for LaTeX.
 *
 * AUTHORS: David Kotz, Raymond Toy toy@soho.crd.ge.com
 *	Modified the eepic.trm file to use PSTricks macros instead.
 *	20 Mar 93:
 *		Utilized many suggestions from Gisli Ottarsson
 *		(gisli@liapunov.eecs.umich.edu) to create a new version.
 *		Should also work with TeX as well as LaTeX.
 *
 *   Bastian Maerkisch
 *
 * send your comments or suggestions to (gnuplot-info@lists.sourceforge.net).
 */
/*
 *  This file contains the PSTricks terminal driver, intended for use with the
 *  pstricks.sty macro package for LaTeX. This is an alternative to the
 *  eepic and latex driver. You need pstricks.sty, and, of course, a printer
 *  that understands PostScript.  Ghostscript understands Postscript too.
 *
 *  PSTricks is available at
 *     http://tug.org/PSTricks/
 *  This driver definitely does not come close to using the full capability
 *  of the PSTricks package.
 */
/*
 * adapted to the new terminal layout by Stefan Bodewig (Dec. 1995)
 *
 * adapted to support pm3d by Tim Piessens and Petr Mikulik (Jan. 2003)
 *
 * remove "hacktext" option since this functionality is now provided by
 * the %H format. Add rotated text at arbitrary angles, full color and
 * version 5 linetype support, filled boxes, pattern fill, variable
 * point size and line width, point symbols in line with other terminals,
 * boxed text, more arrow features, standalone (LaTeX) mode (including
 * text encoding support), transparency.  (BM, March/April 2018)
 *
 */
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
	register_term(pstricks)
#endif

//#ifdef TERM_PROTO
TERM_PUBLIC void PSTRICKS_options(GpTermEntry * pThis, GnuPlot * pGp);
TERM_PUBLIC void PSTRICKS_init(GpTermEntry * pThis);
TERM_PUBLIC void PSTRICKS_graphics(GpTermEntry * pThis);
TERM_PUBLIC void PSTRICKS_text(GpTermEntry * pThis);
TERM_PUBLIC void PSTRICKS_linetype(GpTermEntry * pThis, int linetype);
TERM_PUBLIC void PSTRICKS_move(GpTermEntry * pThis, uint x, uint y);
TERM_PUBLIC void PSTRICKS_point(GpTermEntry * pThis, uint x, uint y, int number);
TERM_PUBLIC void PSTRICKS_vector(GpTermEntry * pThis, uint ux, uint uy);
TERM_PUBLIC void PSTRICKS_arrow(GpTermEntry * pThis, uint sx, uint sy, uint ex, uint ey, int head);
TERM_PUBLIC void PSTRICKS_pointsize(GpTermEntry * pThis, double pointsize);
TERM_PUBLIC void PSTRICKS_put_text(GpTermEntry * pThis, uint x, uint y, const char str[]);
TERM_PUBLIC int PSTRICKS_justify_text(GpTermEntry * pThis, enum JUSTIFY mode);
TERM_PUBLIC int PSTRICKS_text_angle(GpTermEntry * pThis, int ang);
TERM_PUBLIC void PSTRICKS_reset(GpTermEntry * pThis);
TERM_PUBLIC void PSTRICKS_linewidth(GpTermEntry * pThis, double linewidth);
TERM_PUBLIC int PSTRICKS_make_palette(GpTermEntry * pThis, t_sm_palette *);
TERM_PUBLIC void PSTRICKS_set_color(GpTermEntry * pThis, const t_colorspec *);
TERM_PUBLIC void PSTRICKS_fillbox(GpTermEntry * pThis, int style, uint x1, uint y1, uint width, uint height);
TERM_PUBLIC void PSTRICKS_filled_polygon(GpTermEntry * pThis, int, gpiPoint *);
TERM_PUBLIC void PSTRICKS_dashtype(GpTermEntry * pThis, int type, t_dashtype * custom_dash_pattern);
TERM_PUBLIC void PSTRICKS_boxed_text(GpTermEntry * pThis, uint x, uint y, int option);

#define PSTRICKS_XMAX 10000.0
#define PSTRICKS_YMAX 10000.0
#define PSTRICKS_HTIC   150
#define PSTRICKS_VTIC   150
#define PSTRICKS_HCHAR  160
#define PSTRICKS_VCHAR  250
//#endif // TERM_PROTO 

#ifndef TERM_PROTO_ONLY
#ifdef TERM_BODY

/* terminate any line in progress */
static void PSTRICKS_endline();

/* coordinate mapping functions */
static inline double PSTRICKS_map_x(int x);
static inline double PSTRICKS_map_y(int y);

/* apply line color */
static void PSTRICKS_save_color(const char * color);
static void PSTRICKS_apply_linecolor();

/* mode */
static bool PST_standalone = FALSE;

/* plot size */
static bool PST_unit_plot = FALSE; /* Unit-sized plot off */
static GpSizeUnits PSTRICKS_explicit_units = INCHES;
static float PSTRICKS_size_x = 5.f;
static float PSTRICKS_size_y = 3.f;

static float PSTRICKS_posx;
static float PSTRICKS_posy;

/* TEXT */
static bool PST_colortext = FALSE;
static enum JUSTIFY PSTRICKS_justify = LEFT;
static int PSTRICKS_angle = 0;

/* if 1 below, then the file size is shorter thanks to a macro for polygon */
/* FIXME: This feature is broken, deactivate for now */
//#define PSTRICKS_SHORTER_FILE 1

static int PSTRICKS_color_type = TC_DEFAULT;
static int PSTRICKS_color = 0;
static char PSTRICKS_color_str[24] = "";
static bool PSTRICKS_palette_set = FALSE;
static int PSTRICKS_palette_size = 128;
static rgb_color PSTRICKS_rgbcolor;
static double PSTRICKS_alpha = 1.0;
static double PSTRICKS_alpha_old = 1.0;
static bool PSTRICKS_have_bg;
static rgb_color PSTRICKS_background = { 1.0, 1.0, 1.0 };

/* POINTS */
#define PSTRICKS_TINY_DOT       0.00025         /* A tiny dot */
static double PST_pointsize;
static double PSTRICKS_ps = 1.0;
static const char * PSTRICKS_points[] = {
	"\\PST@Plus",
	"\\PST@Cross",
	"\\PST@Star",
	"\\PST@Square",
	"\\PST@Fillsquare",
	"\\PST@Circle",
	"\\PST@Fillcircle",
	"\\PST@Triangle",
	"\\PST@Filltriangle",
	"\\PST@TriangleD",
	"\\PST@FilltriangleD",
	"\\PST@Diamond",
	"\\PST@Filldiamond",
	"\\PST@Pentagon",
	"\\PST@Fillpentagon"
};
#define PSTRICKS_POINT_TYPES (sizeof(PSTRICKS_points) / sizeof(char *))

/* LINES */
#define PSTRICKS_NUMLINES 6     /* number of linetypes below */
static const char * PSTRICKS_lines[] = {
	"\\PST@Border",
	"\\PST@Axes",
	"\\PST@Solid",
	"\\PST@Dashed",
	"\\PST@Dotted",
	"\\PST@LongDash"
};

#define PSTRICKS_NUMCOLORS 6
static const char * PSTRICKS_colors[] = { "red", "green", "blue", "magenta", "darkgray", "cyan", "yellow" };
static bool PST_rounded = FALSE; /* line caps and joins */
static int PSTRICKS_type; /* current line type */
static char PSTRICKS_old_linecolor[24] = "black"; /* current line color */
static char PSTRICKS_linecolor[24] = "black"; /* new line color */

/* line width */
static double PSTRICKS_lw;
static double PSTRICKS_lw_scale = 1.0;
static bool PSTRICKS_inline = FALSE; /* are we in the middle of a line */
static int PSTRICKS_linecount = 0; /* number of points in line so far */
#define PSTRICKS_LINEMAX 100 /* max value for linecount */

/* ARROWS */
static bool PST_psarrows = TRUE;
static double PSTRICKS_arrowwidth = -1.0;
static double PSTRICKS_arrowlength = -1.0;
static double PSTRICKS_arrowinset = -1.0;

/* FILL PATTERN */
static const char * PSTRICKS_patterns[] = {
	"solid", "crosshatch%s", "crosshatch%s,hatchsep=2.5pt", "solid", "vlines%s", "hlines%s", "vlines%s,hatchsep=2.5pt", "hlines%s,hatchsep=2.5pt", "dots*", "penrose%s"
};
#define PSTRICKS_NUMPATTERN (sizeof(PSTRICKS_patterns) / sizeof(char *))

/*  BOXED TEXT */
static bool PSTRICKS_in_textbox = FALSE;
static bool PSTRICKS_textbox_fill;
static bool PSTRICKS_textbox_frame;
static char * PSTRICKS_textbox_text  = NULL;
static double PSTRICKS_textbox_x;
static double PSTRICKS_textbox_y;
static char * PSTRICKS_textbox_fillcolor;
static double PSTRICKS_textbox_sep;

/*
 * Handle options
 */

/* option names */
enum PSTRICKS_id { PSTRICKS_DEFAULTOPTIONS,
		   PSTRICKS_SIZE,
		   PSTRICKS_UNIT, PSTRICKS_NOUNIT,
		   PSTRICKS_STANDALONE, PSTRICKS_INPUT,
		   PSTRICKS_BLACKTEXT, PSTRICKS_COLORTEXT,
		   PSTRICKS_ROUNDED, PSTRICKS_BUTT,
		   PSTRICKS_LINEWIDTH, PSTRICKS_POINTSCALE,
		   PSTRICKS_PSARROWS, PSTRICKS_GPARROWS,
		   PSTRICKS_BACKGROUND,
		   PSTRICKS_OTHER };

static struct gen_table PSTRICKS_opts[] = {
	{ "def$ault", PSTRICKS_DEFAULTOPTIONS },
	{ "size", PSTRICKS_SIZE },
	{ "u$nit", PSTRICKS_UNIT },
	{ "nou$nit", PSTRICKS_NOUNIT },
	{ "stand$alone", PSTRICKS_STANDALONE },
	{ "inp$ut", PSTRICKS_INPUT },
	{ "b$lacktext", PSTRICKS_BLACKTEXT },
	{ "colort$ext", PSTRICKS_COLORTEXT },
	{ "colourt$ext", PSTRICKS_COLORTEXT },
	{ "round$ed", PSTRICKS_ROUNDED },
	{ "butt", PSTRICKS_BUTT },
	{ "backg$round", PSTRICKS_BACKGROUND },
	{ "lw", PSTRICKS_LINEWIDTH },
	{ "linew$idth", PSTRICKS_LINEWIDTH },
	{ "points$cale", PSTRICKS_POINTSCALE },
	{ "ps", PSTRICKS_POINTSCALE },
	{ "psarrows", PSTRICKS_PSARROWS },
	{ "gparrows", PSTRICKS_GPARROWS },
	{ NULL, PSTRICKS_OTHER }
};

TERM_PUBLIC void PSTRICKS_options(GpTermEntry * pThis, GnuPlot * pGp)
{
	char size_str[80] = "";
	while(!pGp->Pgm.EndOfCommand()) {
		switch((enum PSTRICKS_id)pGp->Pgm.LookupTableForCurrentToken(&PSTRICKS_opts[0])) {
			case PSTRICKS_DEFAULTOPTIONS:
			    PST_unit_plot = FALSE;
			    PST_standalone = FALSE;
			    PSTRICKS_size_x = 5.0;
			    PSTRICKS_size_y = 3.0;
			    break;
			case PSTRICKS_UNIT:
			    PST_unit_plot = TRUE;
			    PSTRICKS_size_x = 5.0;
			    PSTRICKS_size_y = 5.0;
			    pThis->MaxX = (uint)PSTRICKS_XMAX;
			    pThis->MaxY = (uint)PSTRICKS_YMAX;
			    pThis->ChrV = (uint)(PSTRICKS_VCHAR * 5.0 / 3.0);
			    pThis->TicV = (uint)(PSTRICKS_VTIC * 5.0 / 3.0);
			    pGp->Pgm.Shift();
			    break;
			case PSTRICKS_NOUNIT:
			    PST_unit_plot = FALSE;
			    PSTRICKS_size_x = 5.0;
			    PSTRICKS_size_y = 3.0;
			    pThis->MaxX = (uint)(PSTRICKS_size_x * PSTRICKS_XMAX / 5.0);
			    pThis->MaxY = (uint)(PSTRICKS_size_y * PSTRICKS_YMAX / 5.0);
			    pThis->ChrV = PSTRICKS_VCHAR;
			    pThis->TicV = PSTRICKS_VTIC;
			    pGp->Pgm.Shift();
			    break;
			case PSTRICKS_SIZE: {
			    float width, height;
			    pGp->Pgm.Shift();
			    PST_unit_plot = FALSE;
			    PSTRICKS_explicit_units = pGp->ParseTermSize(&width, &height, INCHES);
			    PSTRICKS_size_x = width  / GpResolution;
			    PSTRICKS_size_y = height / GpResolution;
			    pThis->MaxX = (uint)(PSTRICKS_size_x * PSTRICKS_XMAX / 5.0);
			    pThis->MaxY = (uint)(PSTRICKS_size_y * PSTRICKS_YMAX / 5.0);
			    pThis->ChrV = PSTRICKS_VCHAR;
			    pThis->TicV = PSTRICKS_VTIC;
			    break;
		    }
			case PSTRICKS_STANDALONE:
			    PST_standalone = TRUE;
			    pGp->Pgm.Shift();
			    break;
			case PSTRICKS_INPUT:
			    PST_standalone = FALSE;
			    pGp->Pgm.Shift();
			    break;
			case PSTRICKS_COLORTEXT:
			    PST_colortext = TRUE;
			    pGp->Pgm.Shift();
			    break;
			case PSTRICKS_BLACKTEXT:
			    PST_colortext = FALSE;
			    pGp->Pgm.Shift();
			    break;
			case PSTRICKS_BUTT:
			    PST_rounded = FALSE;
			    pGp->Pgm.Shift();
			    break;
			case PSTRICKS_ROUNDED:
			    PST_rounded = TRUE;
			    pGp->Pgm.Shift();
			    break;
			case PSTRICKS_LINEWIDTH:
			    pGp->Pgm.Shift();
			    PSTRICKS_lw_scale = pGp->RealExpression();
			    if(PSTRICKS_lw_scale < 0.0)
				    PSTRICKS_lw_scale = 1.0;
			    break;
			case PSTRICKS_POINTSCALE:
			    pGp->Pgm.Shift();
			    PSTRICKS_ps = pGp->RealExpression();
			    if(PSTRICKS_ps < 0.0)
				    PSTRICKS_ps = 1.0;
			    break;
			case PSTRICKS_BACKGROUND: {
			    int background;
			    pGp->Pgm.Shift();
			    background = pGp->ParseColorName();
			    PSTRICKS_background.r = (double)((background >> 16) & 0xff) / 255.0;
			    PSTRICKS_background.g = (double)((background >>  8) & 0xff) / 255.0;
			    PSTRICKS_background.b = (double)( background        & 0xff) / 255.0;
			    break;
		    }
			case PSTRICKS_GPARROWS:
			    pGp->Pgm.Shift();
			    PST_psarrows = FALSE;
			    break;
			case PSTRICKS_PSARROWS:
			    pGp->Pgm.Shift();
			    PST_psarrows = TRUE;
			    break;
			default:
			    pGp->IntErrorCurToken("Unknown terminal option");
		}
	}
	if(!PST_unit_plot) {
		if(PSTRICKS_explicit_units == INCHES)
			snprintf(size_str, sizeof(size_str), "size %.2fin, %.2fin", PSTRICKS_size_x, PSTRICKS_size_y);
		else if(PSTRICKS_explicit_units == CM)
			snprintf(size_str, sizeof(size_str), "size %.2fcm, %.2fcm", PSTRICKS_size_x * 2.54, PSTRICKS_size_y * 2.54);
	}
	// update terminal option string
	slprintf(GPT._TermOptions, "%s %s linewidth %.1f pointscale %.1f %s background \"#%02x%02x%02x\" %sarrows %s",
	    PST_unit_plot ? "unit" : size_str, PST_rounded ? "rounded" : "butt", PSTRICKS_lw_scale, PSTRICKS_ps,
	    PST_colortext ? "colortext" : "blacktext", (int)floor(PSTRICKS_background.r * 255 + 0.5),
	    (int)floor(PSTRICKS_background.g * 255 + 0.5), (int)floor(PSTRICKS_background.b * 255 + 0.5), PST_psarrows ? "ps" : "gp", PST_standalone ? "standalone" : "input");
}

TERM_PUBLIC void PSTRICKS_init(GpTermEntry * pThis)
{
	fseek(GPT.P_GpOutFile, 0, SEEK_SET);
	if(PST_standalone) {
		const char * inputenc = pThis->P_Gp->LatexInputEncoding(GPT._Encoding);
		// multiple standalone graphs 
		fputs("\\documentclass[a4paper]{article}\n"
		    "\\usepackage[T1]{fontenc}\n"
		    "\\usepackage{pstricks}\n", GPT.P_GpOutFile);
		if(inputenc) {
			if(GPT._Encoding == S_ENC_UTF8)
				fputs("\\usepackage{pifont}\n\\usepackage[postscript,warnunknown]{ucs}\n", GPT.P_GpOutFile);
			fprintf(GPT.P_GpOutFile, "\\usepackage[%s]{inputenc}\n", inputenc);
		}
		fputs("\\begin{document}\n", GPT.P_GpOutFile);
	}
	fputs("% GNUPLOT: LaTeX picture using PSTRICKS macros\n", GPT.P_GpOutFile);
}

TERM_PUBLIC void PSTRICKS_graphics(GpTermEntry * pThis)
{
	char background[80] = "";
	if(PST_standalone) {
		fputs("\\begin{figure}\n", GPT.P_GpOutFile);
	}
	fputs("\
% Define new PST objects, if not already defined\n\
\\ifx\\PSTloaded\\undefined\n\
\\def\\PSTloaded{t}\n\
\\psset{arrowsize=.01 3.2 1.4 .3}\n\
\\psset{dotsize=0.15}\n\
\\catcode`@=11\n\n", GPT.P_GpOutFile);
	// Define line type objects 
	fputs("\
\\newpsobject{PST@Border}{psline}{linestyle=solid}\n\
\\newpsobject{PST@Axes}{psline}{linestyle=dotted,dotsep=.004}\n\
\\newpsobject{PST@Solid}{psline}{linestyle=solid}\n\
\\newpsobject{PST@Dashed}{psline}{linestyle=dashed,dash=.01 .01}\n\
\\newpsobject{PST@Dotted}{psline}{linestyle=dotted,dotsep=.008}\n\
\\newpsobject{PST@LongDash}{psline}{linestyle=dashed,dash=.02 .01}\n",
	    GPT.P_GpOutFile);
	// Define point objects 
	fputs("\
\\newpsobject{PST@Plus}{psdot}{linewidth=.001,linestyle=solid,dotstyle=+}\n\
\\newpsobject{PST@Cross}{psdot}{linewidth=.001,linestyle=solid,dotstyle=+,dotangle=45}\n\
\\newpsobject{PST@Star}{psdot}{linewidth=.001,linestyle=solid,dotstyle=*}\n\
\\newpsobject{PST@Square}{psdot}{linewidth=.001,linestyle=solid,dotstyle=square}\n\
\\newpsobject{PST@Fillsquare}{psdot}{linewidth=.001,linestyle=solid,dotstyle=square*}\n\
\\newpsobject{PST@Circle}{psdot}{linewidth=.001,linestyle=solid,dotstyle=o}\n\
\\newpsobject{PST@Fillcircle}{psdot}{linewidth=.001,linestyle=solid,dotstyle=*}\n\
\\newpsobject{PST@Triangle}{psdot}{linewidth=.001,linestyle=solid,dotstyle=triangle}\n\
\\newpsobject{PST@Filltriangle}{psdot}{linewidth=.001,linestyle=solid,dotstyle=triangle*}\n\
\\newpsobject{PST@TriangleD}{psdot}{linewidth=.001,linestyle=solid,dotstyle=triangle,dotangle=180}\n\
\\newpsobject{PST@FilltriangleD}{psdot}{linewidth=.001,linestyle=solid,dotstyle=triangle*,dotangle=180}\n\
\\newpsobject{PST@Diamond}{psdot}{linewidth=.001,linestyle=solid,dotstyle=square,dotangle=45}\n\
\\newpsobject{PST@Filldiamond}{psdot}{linewidth=.001,linestyle=solid,dotstyle=square*,dotangle=45}\n\
\\newpsobject{PST@Pentagon}{psdot}{linewidth=.001,linestyle=solid,dotstyle=pentagon}\n\
\\newpsobject{PST@Fillpentagon}{psdot}{linewidth=.001,linestyle=solid,dotstyle=pentagon*}\n",
	    GPT.P_GpOutFile);
	// Define arrow object 
	fputs("\
\\newpsobject{PST@Arrow}{psline}{linestyle=solid}\n\
\\catcode`@=12\n\n\
\\fi\n", GPT.P_GpOutFile);
	// background color 
	PSTRICKS_have_bg = FALSE;
	if(PSTRICKS_background.r != 1. || PSTRICKS_background.g != 1. || PSTRICKS_background.b != 1.) {
		PSTRICKS_have_bg = TRUE;
		fprintf(GPT.P_GpOutFile, "\\newrgbcolor{PST@BGCOLOR}{%f %f %f}\n",
		    PSTRICKS_background.r, PSTRICKS_background.g, PSTRICKS_background.b);
		strnzcpy(background, "[bgcolor=PST@BGCOLOR]", sizeof(background));
	}

	/* Set the scaled plot size, if it's not a unit plot */
	if(!PST_unit_plot) {
		// Choose 5in as unit for historical reasons only.
		fputs("\\psset{unit=5.0in}\n", GPT.P_GpOutFile);
		fprintf(GPT.P_GpOutFile, "\
\\pspicture%s(%f,%f)(%f,%f)\n\
\\ifx\\nofigs\\undefined\n\
\\catcode`@=11\n\n",
		    background,
		    0., 0.,
		    PSTRICKS_size_x / 5., PSTRICKS_size_y / 5.
		    );
	}
	else {
		fprintf(GPT.P_GpOutFile, "\\pspicture%s(%f,%f)(%f,%f)\n\\ifx\\nofigs\\undefined\n\\catcode`@=11\n\n", background, 0.0, 0.0, 1.0, 1.0);
	}
	PSTRICKS_posx = PSTRICKS_posy = 0;
	PSTRICKS_linetype(pThis, -1);
	PSTRICKS_palette_set = FALSE; // PM3D palette set? 
	// Re-set point symbol size 
	fputs("\\psset{dotscale=1}\n", GPT.P_GpOutFile);
	PST_pointsize = 1.0;
	strcpy(PSTRICKS_old_linecolor, "black");
	strcpy(PSTRICKS_linecolor, "black");
	PSTRICKS_lw = 1;
	PSTRICKS_arrowwidth = PSTRICKS_arrowlength = PSTRICKS_arrowinset = -1;
	fprintf(GPT.P_GpOutFile, "\\psset{linecap=%d,linejoin=%d}\n", PST_rounded ? 1 : 0, PST_rounded ? 1 : 0);
}

TERM_PUBLIC void PSTRICKS_text(GpTermEntry * pThis)
{
	PSTRICKS_endline();
	fputs("\
\\catcode`@=12\n\
\\fi\n\
\\endpspicture\n", GPT.P_GpOutFile);
	if(PST_standalone) {
		fputs("\\end{figure}\n", GPT.P_GpOutFile);
	}
}

/* coordinate mapping functions */
static inline double PSTRICKS_map_x(int x)
{
	return x / PSTRICKS_XMAX;
}

static inline double PSTRICKS_map_y(int y)
{
	return y / PSTRICKS_YMAX;
}

TERM_PUBLIC void PSTRICKS_linetype(GpTermEntry * pThis, int linetype)
{
	PSTRICKS_endline();
	// all lines except for axis default to solid 
	if(linetype == LT_AXIS)
		PSTRICKS_type = -1;
	else
		PSTRICKS_type = 0;
	// negative line types are all black 
	if(linetype < 0) {
		PSTRICKS_color_type = TC_DEFAULT;
		PSTRICKS_save_color("black");
	}
	else {
		PSTRICKS_color_type = TC_LT;
		PSTRICKS_color = linetype % PSTRICKS_NUMCOLORS;
		PSTRICKS_save_color(PSTRICKS_colors[PSTRICKS_color]);
	}
}

TERM_PUBLIC void PSTRICKS_dashtype(GpTermEntry * pThis, int dt, t_dashtype * custom_dash_pattern)
{
	PSTRICKS_endline();
	if(dt >= 0) {
		int linetype = dt;
		if(linetype >= PSTRICKS_NUMLINES + LT_BLACK)
			linetype %= (PSTRICKS_NUMLINES + LT_BLACK);
		if(linetype < LT_BLACK)
			linetype = LT_BLACK;
		PSTRICKS_type = linetype;
	}
	else if(dt == DASHTYPE_SOLID) {
		PSTRICKS_type = 0;
	}
	else if(dt == DASHTYPE_AXIS) {
		PSTRICKS_type = -1;
	}
	else if(dt == DASHTYPE_CUSTOM) {
		/* not supported */
	}
}

TERM_PUBLIC void PSTRICKS_move(GpTermEntry * pThis, uint x, uint y)
{
	PSTRICKS_endline();
	PSTRICKS_posx = static_cast<float>(PSTRICKS_map_x(x));
	PSTRICKS_posy = static_cast<float>(PSTRICKS_map_y(y));
}

TERM_PUBLIC void PSTRICKS_point(GpTermEntry * pThis, uint x, uint y, int number)
{
	PSTRICKS_move(pThis, x, y);
	// Print the character defined by 'number'; number < 0 means to use a dot, otherwise one of the defined points. 
	if(PST_pointsize <= 0.)
		return;
	PSTRICKS_apply_linecolor();
	if(number < 0) {
		fprintf(GPT.P_GpOutFile, "\\qdisk(%.4f,%.4f){%.4f}\n", PSTRICKS_map_x(x), PSTRICKS_map_y(y), PSTRICKS_TINY_DOT);
	}
	else {
		fprintf(GPT.P_GpOutFile, "%s(%.4f,%.4f)\n", PSTRICKS_points[number % PSTRICKS_POINT_TYPES], PSTRICKS_map_x(x), PSTRICKS_map_y(y));
	}
}

TERM_PUBLIC void PSTRICKS_vector(GpTermEntry * pThis, uint ux, uint uy)
{
	if(!PSTRICKS_inline) {
		PSTRICKS_inline = TRUE;
		/* Start a new line. This depends on line type */
		PSTRICKS_apply_linecolor();
		fprintf(GPT.P_GpOutFile, "%s(%.4f,%.4f)", PSTRICKS_lines[PSTRICKS_type + 2], PSTRICKS_posx, PSTRICKS_posy);
		PSTRICKS_linecount = 1;
	}
	else {
		/*
		 * Even though we are in middle of a path,
		 * we may want to start a new path command.
		 * If they are too long then latex will choke.
		 */
		if(PSTRICKS_linecount++ >= PSTRICKS_LINEMAX) {
			putc('\n', GPT.P_GpOutFile);
			fprintf(GPT.P_GpOutFile, "%s(%.4f,%.4f)", PSTRICKS_lines[PSTRICKS_type + 2], PSTRICKS_posx, PSTRICKS_posy);
			PSTRICKS_linecount = 1;
		}
	}
	if(PSTRICKS_linecount % 8 == 0)
		putc('\n', GPT.P_GpOutFile);
	PSTRICKS_posx = static_cast<float>(PSTRICKS_map_x(ux));
	PSTRICKS_posy = static_cast<float>(PSTRICKS_map_y(uy));
	fprintf(GPT.P_GpOutFile, "(%.4f,%.4f)", PSTRICKS_posx, PSTRICKS_posy);
}

static void PSTRICKS_endline()
{
	if(PSTRICKS_inline) {
		if(PSTRICKS_linecount % 8 != 0)
			putc('\n', GPT.P_GpOutFile);
		putc('\n', GPT.P_GpOutFile);
		PSTRICKS_inline = FALSE;
	}
}

TERM_PUBLIC void PSTRICKS_arrow(GpTermEntry * pThis, uint sx, uint sy, uint ex, uint ey, int head)
{
	const char * head_str = "";
	double width, length, inset = 0.0;
	// Note:  we cannot handle GPT.CArw.HeadFilled, HEADS_ONLY, and SHAFT_ONLY 
	PSTRICKS_endline();
	PSTRICKS_apply_linecolor();
	if(!PST_psarrows) {
		GnuPlot::DoArrow(pThis, sx, sy, ex, ey, head);
		return;
	}
	if(GPT.CArw.HeadLength <= 0) {
		double def_length = PSTRICKS_HTIC;
		width = 2 * def_length * sin(SMathConst::PiDiv180 * 15);
		length = def_length * cos(SMathConst::PiDiv180 * 15);
		inset = 0;
	}
	else {
		width = 2 * GPT.CArw.HeadLength * sin(SMathConst::PiDiv180 * GPT.CArw.HeadAngle);
		length = GPT.CArw.HeadLength * cos(SMathConst::PiDiv180 * GPT.CArw.HeadAngle);
		if(GPT.CArw.HeadBackAngle != 90.0)
			inset = width / 2. / tan(SMathConst::PiDiv180 * GPT.CArw.HeadBackAngle);
	}
	if(GPT.CArw.HeadAngle != 90.0 || GPT.CArw.HeadLength <= 0) {
		if((PSTRICKS_map_x(static_cast<int>(width)) / PSTRICKS_lw) != PSTRICKS_arrowwidth) {
			PSTRICKS_arrowwidth = PSTRICKS_map_x(static_cast<int>(width)) / PSTRICKS_lw;
			fprintf(GPT.P_GpOutFile, "\\psset{arrowsize=0 %.3f}\n", PSTRICKS_arrowwidth);
		}
		if((width != 0.) && ((length / width) != PSTRICKS_arrowlength)) {
			PSTRICKS_arrowlength = length / width;
			fprintf(GPT.P_GpOutFile, "\\psset{arrowlength=%.2f}\n", PSTRICKS_arrowlength);
		}
		if((width != 0.) && ((inset / width) != PSTRICKS_arrowinset)) {
			PSTRICKS_arrowinset = inset / width;
			fprintf(GPT.P_GpOutFile, "\\psset{arrowinset=%.2f}\n", PSTRICKS_arrowinset);
		}
		else if((width == 0.) && (PSTRICKS_arrowinset != 0.)) {
			PSTRICKS_arrowinset = 0.0;
			fputs("\\psset{arrowinset=0}\n", GPT.P_GpOutFile);
		}
		// fprintf(GPT.P_GpOutFile, "%% %.3f %.1f %.1f\n", GPT.CArw.HeadBackAngle / PSTRICKS_XMAX, GPT.CArw.HeadAngle, GPT.CArw.HeadBackAngle);
		if(width == 0.)
			head &= ~BOTH_HEADS;
		if((head & BOTH_HEADS) == BOTH_HEADS)
			head_str = "{<->}";
		else if(head & END_HEAD)
			head_str = "{->}";
		else if(head & BACKHEAD)
			head_str = "{<-}";
	}
	else {
		if((PSTRICKS_map_x(static_cast<int>(width)) / PSTRICKS_lw) != PSTRICKS_arrowwidth) {
			PSTRICKS_arrowwidth = PSTRICKS_map_x(static_cast<int>(width)) / PSTRICKS_lw;
			fprintf(GPT.P_GpOutFile, "\\psset{tbarsize=0 %.3f}\n", PSTRICKS_arrowwidth);
		}
		if((head & BOTH_HEADS) == BOTH_HEADS)
			head_str = "{|-|}";
		else if(head & END_HEAD)
			head_str = "{-|}";
		else if(head & BACKHEAD)
			head_str = "{|-}";
	}
	fprintf(GPT.P_GpOutFile, "%s%s(%.4f,%.4f)(%.4f,%.4f)\n", PSTRICKS_lines[PSTRICKS_type + 2], head_str,
	    PSTRICKS_map_x(sx), PSTRICKS_map_y(sy), PSTRICKS_map_x(ex), PSTRICKS_map_y(ey));
	PSTRICKS_posx = static_cast<float>(PSTRICKS_map_x(ex));
	PSTRICKS_posy = static_cast<float>(PSTRICKS_map_y(ey));
}

TERM_PUBLIC void PSTRICKS_pointsize(GpTermEntry * pThis, double pointsize)
{
	pointsize *= PSTRICKS_ps;
	if(PST_pointsize != pointsize) {
		PSTRICKS_endline();
		if(pointsize != 0.)
			fprintf(GPT.P_GpOutFile, "\\psset{dotscale=%f}\n", pointsize);
		PST_pointsize = pointsize;
	}
}

TERM_PUBLIC void PSTRICKS_put_text(GpTermEntry * pThis, uint x, uint y, const char str[])
{
	PSTRICKS_endline();
	if(PSTRICKS_in_textbox && (PSTRICKS_textbox_text == NULL)) {
		PSTRICKS_textbox_text = sstrdup(str);
		return;
	}
	// Skip this if the string is empty 
	if(!isempty(str)) {
		fputs("\\rput", GPT.P_GpOutFile);
		// Set justification 
		switch(PSTRICKS_justify) {
			case LEFT: fputs("[l]", GPT.P_GpOutFile); break;
			case CENTRE: break;
			case RIGHT: fputs("[r]", GPT.P_GpOutFile); break;
		}
		/* Set text angle */
		switch(PSTRICKS_angle) {
			case 0: break;
			case 90: fputs("{L}", GPT.P_GpOutFile); break;
			default: fprintf(GPT.P_GpOutFile, "{%i}", PSTRICKS_angle); break;
		}
		/* Set reference position and text */
		fprintf(GPT.P_GpOutFile, "(%.4f,%.4f)", PSTRICKS_map_x(x), PSTRICKS_map_y(y));
		if(!PST_colortext || (strcmp(PSTRICKS_linecolor, "black") == 0))
			fprintf(GPT.P_GpOutFile, "{%s}\n", str);
		else
			fprintf(GPT.P_GpOutFile, "{\\color{%s} %s}\n", PSTRICKS_linecolor, str);
	}
}

TERM_PUBLIC int PSTRICKS_justify_text(GpTermEntry * pThis, enum JUSTIFY mode)
{
	PSTRICKS_justify = mode;
	return TRUE;
}

TERM_PUBLIC int PSTRICKS_text_angle(GpTermEntry * pThis, int ang)
{
	PSTRICKS_angle = ang;
	while(PSTRICKS_angle < 0)
		PSTRICKS_angle += 360;
	while(PSTRICKS_angle > 360)
		PSTRICKS_angle -= 360;
	return TRUE;
}

TERM_PUBLIC void PSTRICKS_reset(GpTermEntry * pThis)
{
	PSTRICKS_endline();
	PSTRICKS_posx = PSTRICKS_posy = 0;
	if(PST_standalone)
		fputs("\\end{document}\n", GPT.P_GpOutFile);
}

TERM_PUBLIC void PSTRICKS_linewidth(GpTermEntry * pThis, double linewidth)
{
	linewidth *= PSTRICKS_lw_scale;
	if(linewidth * 0.0015 != PSTRICKS_lw) {
		PSTRICKS_endline();
		PSTRICKS_lw = linewidth * 0.0015;
		fprintf(GPT.P_GpOutFile, "\\psset{linewidth=%.4f}\n", PSTRICKS_lw);
	}
}

TERM_PUBLIC int PSTRICKS_make_palette(GpTermEntry * pThis, t_sm_palette * palette)
{
	GnuPlot * p_gp = pThis->P_Gp;
	// Query to determine palette size 
	if(palette == NULL) {
		return PSTRICKS_palette_size;
	}
	PSTRICKS_endline();
	if(PSTRICKS_palette_set == FALSE) {
		int i;
		// Create new palette 
		PSTRICKS_palette_set = TRUE;
		if(p_gp->SmPltt.colorMode == SMPAL_COLOR_MODE_GRAY) {
			// Grey palette 
			for(i = 0; i < p_gp->SmPltt.Colors; i++) {
				double g = i * 1.0 / (p_gp->SmPltt.Colors - 1);
				g = 1e-3 * (int)(g * 1000); /* round to 3 digits to use %g below */
				fprintf(GPT.P_GpOutFile, "\\newgray{PST@COLOR%d}{%g}\n", i, g);
			}
		}
		else {
			for(i = 0; i < p_gp->SmPltt.Colors; i++) {
				// round to 3 digits 
				double r = 1e-3 * (int)(palette->P_Color[i].r * 1000);
				double g = 1e-3 * (int)(palette->P_Color[i].g * 1000);
				double b = 1e-3 * (int)(palette->P_Color[i].b * 1000);
				fprintf(GPT.P_GpOutFile, "\\newrgbcolor{PST@COLOR%d}{%f %f %f}\n", i, r, g, b);
			}
		}
	}
	// use the following macro to shorten the file size 
	fputs("\\def\\polypmIIId#1{\\pspolygon[linestyle=none,fillstyle=solid,fillcolor=PST@COLOR#1]}\n\n", GPT.P_GpOutFile);
	return 0;
}

static void PSTRICKS_save_color(const char * color)
{
	if(color)
		strcpy(PSTRICKS_linecolor, color);
}

static void PSTRICKS_apply_linecolor(void)
{
	if(strcmp(PSTRICKS_linecolor, PSTRICKS_old_linecolor) != 0) {
		PSTRICKS_endline();
		strcpy(PSTRICKS_old_linecolor, PSTRICKS_linecolor);
		fprintf(GPT.P_GpOutFile, "\\psset{linecolor=%s}\n", PSTRICKS_linecolor);
	}
	if(PSTRICKS_alpha != PSTRICKS_alpha_old) {
		/* We set the opacity of lines and fills. Required for transparent symbols. */
		fprintf(GPT.P_GpOutFile, "\\psset{strokeopacity=%0.2f,opacity=%0.2f}\n", PSTRICKS_alpha, PSTRICKS_alpha);
		PSTRICKS_alpha_old = PSTRICKS_alpha;
	}
}

TERM_PUBLIC void PSTRICKS_set_color(GpTermEntry * pThis, const t_colorspec * colorspec)
{
	GnuPlot * p_gp = pThis->P_Gp;
	int new_color;
	double gray = colorspec->value;
	switch(colorspec->type) {
		case TC_FRAC:
		    PSTRICKS_color_type = colorspec->type;
		    if(p_gp->SmPltt.UseMaxColors != 0)
			    gray = p_gp->QuantizeGray(gray);
		    new_color = (gray <= 0) ? 0 : ((gray >= 1) ? (p_gp->SmPltt.Colors-1) : (int)(gray * p_gp->SmPltt.Colors));
		    if(new_color >= PSTRICKS_palette_size)
			    new_color = PSTRICKS_palette_size - 1;
		    if(PSTRICKS_palette_set == FALSE) {
			    fputs("pstricks: Palette used before set!\n", stderr);
			    fputs("% ERROR: Palette used before set!\n", GPT.P_GpOutFile);
		    }
		    PSTRICKS_color = new_color;
		    snprintf(PSTRICKS_color_str, sizeof(PSTRICKS_color_str), "PST@COLOR%d", new_color);
		    PSTRICKS_save_color(PSTRICKS_color_str);
		    PSTRICKS_alpha = 1.0;
		    break;
		case TC_RGB: {
		    int red, green, blue;
		    double r, g, b, alpha;
		    red   = (colorspec->lt >> 16) & 0xff;
		    green = (colorspec->lt >>  8) & 0xff;
		    blue  = (colorspec->lt      ) & 0xff;
		    alpha = (double)(255 - ((colorspec->lt >> 24) & 0xff)) / 255;
		    /* round to 3 digits */
		    r = 1e-3 * (int)(red   / 255.0 * 1000);
		    g = 1e-3 * (int)(green / 255.0 * 1000);
		    b = 1e-3 * (int)(blue  / 255.0 * 1000);
		    if((PSTRICKS_color_type != TC_RGB) || (PSTRICKS_rgbcolor.r != r) || (PSTRICKS_rgbcolor.g != g) || (PSTRICKS_rgbcolor.b != b)) {
			    PSTRICKS_endline();
			    fprintf(GPT.P_GpOutFile, "\\newrgbcolor{c}{%g %g %g}\n",  r, g, b);
			    PSTRICKS_old_linecolor[0] = 0; /* invalidate old color */
			    strcpy(PSTRICKS_linecolor, "c");
			    PSTRICKS_color_type = colorspec->type;
			    PSTRICKS_rgbcolor.r = r;
			    PSTRICKS_rgbcolor.g = g;
			    PSTRICKS_rgbcolor.b = b;
		    }
		    PSTRICKS_alpha = alpha;
		    break;
	    }
		case TC_LT:
		    if(colorspec->lt < 0) {
			    PSTRICKS_color_type = TC_DEFAULT;
			    PSTRICKS_save_color("black");
		    }
		    else {
			    PSTRICKS_color_type = TC_LT;
			    PSTRICKS_color = colorspec->lt % PSTRICKS_NUMCOLORS;
			    PSTRICKS_save_color(PSTRICKS_colors[PSTRICKS_color]);
		    }
		    PSTRICKS_alpha = 1.0;
		    break;
		default:
		    break;
	}
}

TERM_PUBLIC void PSTRICKS_fillbox(GpTermEntry * pThis, int style, uint x1, uint y1, uint width, uint height)
{
	int pattern = style >> 4;
	int frac = style >> 4;
	const char * stylestr = "solid";
	const char * colorstr = "fillcolor";
	char patfill[80];
	char opacity[80] = "";
	char fraction[80] = "";
	PSTRICKS_endline();
	switch(style & 0x0f) {
		case FS_SOLID:
		    // color already set
		    if(frac != 100)
			    snprintf(fraction, sizeof(fraction), "!%d", frac);
		    if(PSTRICKS_alpha != 1.)
			    snprintf(opacity, sizeof(opacity), ",opacity=%0.2f", PSTRICKS_alpha);
		    break;
		case FS_TRANSPARENT_SOLID:
		    // color already set
		    if(frac != 100)
			    snprintf(opacity, sizeof(opacity), ",opacity=%0.2f", (double)frac / 100.);
		    break;
		case FS_PATTERN:
		case FS_TRANSPARENT_PATTERN:
		    stylestr = (char *)PSTRICKS_patterns[pattern % PSTRICKS_NUMPATTERN];
		    if(pattern != 0)
			    colorstr = "hatchcolor";
		    snprintf(patfill, sizeof(patfill), stylestr, (style & 0x0f) == FS_PATTERN ? "*" : "");
		    if((style & 0x0f) == FS_PATTERN && PSTRICKS_have_bg)
			    strncat(patfill, ",fillcolor=PST@BGCOLOR", sizeof(patfill)-strlen(patfill)-1);
		    stylestr = patfill;
		    break;
		case FS_EMPTY:
		    break;
		case FS_DEFAULT:
		default:
		    break;
	}

	fprintf(GPT.P_GpOutFile, "\\psframe[linestyle=none,fillstyle=%s", stylestr);
	if(PSTRICKS_color_type == TC_FRAC) {
#ifdef PSTRICKS_SHORTER_FILE
		fprintf(GPT.P_GpOutFile, "%s,%s=PST@COLOR%d%s]", opacity, colorstr, PSTRICKS_color, fraction);
#else
		fprintf(GPT.P_GpOutFile, "%s,%s=%s%s]", opacity, colorstr, PSTRICKS_color_str, fraction);
#endif
	}
	else if(PSTRICKS_color_type == TC_RGB) {
		fprintf(GPT.P_GpOutFile, "%s,%s=c%s]", opacity, colorstr, fraction);
	}
	else if(PSTRICKS_color_type == TC_LT) {
		fprintf(GPT.P_GpOutFile, "%s,%s=%s%s]", opacity, colorstr, PSTRICKS_colors[PSTRICKS_color], fraction);
	}
	else { // e.g. TC_DEFAULT
		fprintf(GPT.P_GpOutFile, "%s]", opacity);
	}
	fprintf(GPT.P_GpOutFile, "(%.4g,%.4g)(%.4g,%.4g)\n",
	    PSTRICKS_map_x(x1), PSTRICKS_map_y(y1),
	    PSTRICKS_map_x(x1 + width), PSTRICKS_map_y(y1 + height));
}

TERM_PUBLIC void PSTRICKS_filled_polygon(GpTermEntry * pThis, int points, gpiPoint * corners)
{
	int style = corners->style;
	int pattern = style >> 4;
	int frac = style >> 4;
	const char * stylestr = "solid";
	const char * colorstr = "fillcolor";
	char patfill[80];
	char opacity[80] = "";
	char fraction[80] = "";
	int i;
	PSTRICKS_endline();
	switch(style & 0x0f) {
		case FS_SOLID:
		    // color already set
		    if(frac != 100)
			    snprintf(fraction, sizeof(fraction), "!%d", frac);
		    if(PSTRICKS_alpha != 1.)
			    snprintf(opacity, sizeof(opacity), ",opacity=%0.2f", PSTRICKS_alpha);
		    break;
		case FS_TRANSPARENT_SOLID:
		    // color already set
		    if(frac != 100)
			    snprintf(opacity, sizeof(opacity), ",opacity=%0.2f", (double)frac / 100.);
		    break;
		case FS_PATTERN:
		case FS_TRANSPARENT_PATTERN:
		    stylestr = (char *)PSTRICKS_patterns[pattern % PSTRICKS_NUMPATTERN];
		    if(pattern != 0)
			    colorstr = "hatchcolor";
		    snprintf(patfill, sizeof(patfill), stylestr, (style & 0x0f) == FS_PATTERN ? "*" : "");
		    if((style & 0x0f) == FS_PATTERN && PSTRICKS_have_bg)
			    strncat(patfill, ",fillcolor=PST@BGCOLOR", sizeof(patfill)-strlen(patfill)-1);
		    stylestr = patfill;
		    break;
		case FS_EMPTY:
		    break;
		case FS_DEFAULT:
		default:
		    break;
	}
	if(PSTRICKS_color_type == TC_FRAC) {
#ifdef PSTRICKS_SHORTER_FILE
		/* using a macro for an abbreviation */
		fprintf(GPT.P_GpOutFile, "\\polypmIIId{%d}", PSTRICKS_color);
#else
		fprintf(GPT.P_GpOutFile, "\\pspolygon[linestyle=none,fillstyle=%s%s,%s=%s%s]", stylestr, opacity, colorstr, PSTRICKS_color_str, fraction);
#endif
	}
	else if(PSTRICKS_color_type == TC_RGB) {
		fprintf(GPT.P_GpOutFile, "\\pspolygon[linestyle=none,fillstyle=%s%s,%s=c%s]", stylestr, opacity, colorstr, fraction);
	}
	else if(PSTRICKS_color_type == TC_LT) {
		fprintf(GPT.P_GpOutFile, "\\pspolygon[linestyle=none,fillstyle=%s%s,%s=%s%s]", stylestr, opacity, colorstr,
		    PSTRICKS_colors[PSTRICKS_color], fraction);
	}
	else {
		fprintf(GPT.P_GpOutFile, "\\pspolygon[linestyle=none,fillstyle=%s%s]", stylestr, opacity);
	}
	for(i = 0; i < points; i++) {
		if(i % 8 == 7) /* up to 8 corners per line */
			fputs("\n", GPT.P_GpOutFile);
		fprintf(GPT.P_GpOutFile, "(%.4g,%.4g)", PSTRICKS_map_x(corners[i].x), PSTRICKS_map_y(corners[i].y));
	}
	/* make sure that polygons are closed - avoids spurious artifacts */
	if((corners[0].x != corners[points-1].x) || (corners[0].y != corners[points-1].y))
		fprintf(GPT.P_GpOutFile, "(%.4g,%.4g)", PSTRICKS_map_x(corners[0].x), PSTRICKS_map_y(corners[0].y));
	fputs("\n\n", GPT.P_GpOutFile);
}

TERM_PUBLIC void PSTRICKS_boxed_text(GpTermEntry * pThis, uint x, uint y, int option)
{
	switch(option) {
		case TEXTBOX_INIT:
		    if(!PSTRICKS_in_textbox) {
			    // ignore second init call 
			    PSTRICKS_in_textbox = TRUE;
			    PSTRICKS_textbox_x = PSTRICKS_map_x(x);
			    PSTRICKS_textbox_y = PSTRICKS_map_y(y);
			    PSTRICKS_textbox_fill = FALSE;
			    PSTRICKS_textbox_frame = FALSE;
			    PSTRICKS_textbox_text  = NULL;
			    PSTRICKS_textbox_fillcolor = NULL;
		    }
		    //printf("textbox init ");
		    break;
		case TEXTBOX_OUTLINE:
		    PSTRICKS_textbox_frame = TRUE;
		    //printf(" outline ");
		    break;
		case TEXTBOX_BACKGROUNDFILL:
		    PSTRICKS_textbox_fill = TRUE;
		    if(PSTRICKS_color_type == TC_DEFAULT)
			    PSTRICKS_textbox_fillcolor = sstrdup("white");
		    else
			    PSTRICKS_textbox_fillcolor = sstrdup(PSTRICKS_linecolor);
		    //printf(" border ");
		    break;
		case TEXTBOX_MARGINS:
		    PSTRICKS_textbox_sep = PSTRICKS_map_x(pThis->ChrH * MAX(x, y) / 1000);
		    break;
		case TEXTBOX_FINISH:
		    //printf("... finish\n");
		    PSTRICKS_in_textbox = FALSE;
		    if(!PSTRICKS_textbox_text)
			    return;
		    if(PSTRICKS_textbox_fill && PSTRICKS_textbox_frame) {
			    fprintf(GPT.P_GpOutFile,
				"\\rput(%.4f,%.4f){"
				"\\psframebox[linecolor=%s,framesep=0]{"
				"\\psframebox*[fillcolor=%s,fillstyle=solid,framesep=%.4f]{%s}}}",
				PSTRICKS_textbox_x, PSTRICKS_textbox_y,
				PSTRICKS_linecolor, PSTRICKS_textbox_fillcolor,
				PSTRICKS_textbox_sep,
				PSTRICKS_textbox_text);
		    }
		    else if(PSTRICKS_textbox_frame) {
			    fprintf(GPT.P_GpOutFile, "\\rput(%.4f,%.4f){\\psframebox[linecolor=black,framesep=%.4f]{%s}}",
					PSTRICKS_textbox_x, PSTRICKS_textbox_y, PSTRICKS_textbox_sep, PSTRICKS_textbox_text);
		    }
		    else {
			    fprintf(GPT.P_GpOutFile, "\\rput(%.4f,%.4f){\\psframebox*[fillcolor=%s,fillstyle=solid,framesep=%.4f]{%s}}",
					PSTRICKS_textbox_x, PSTRICKS_textbox_y, PSTRICKS_textbox_fillcolor, PSTRICKS_textbox_sep, PSTRICKS_textbox_text);
		    }
		    SAlloc::F(PSTRICKS_textbox_text);
		    SAlloc::F(PSTRICKS_textbox_fillcolor);
		    break;
	}
}

#endif /* TERM_BODY */

#ifdef TERM_TABLE
TERM_TABLE_START(pstricks_driver)
	"pstricks", 
	"LaTeX picture environment with PSTricks macros",
	static_cast<uint>(PSTRICKS_XMAX), 
	static_cast<uint>(PSTRICKS_YMAX * 3.0/5.0),
	PSTRICKS_VCHAR, 
	PSTRICKS_HCHAR,
	PSTRICKS_VTIC, 
	PSTRICKS_HTIC, 
	PSTRICKS_options, 
	PSTRICKS_init, 
	PSTRICKS_reset,
	PSTRICKS_text, 
	GnuPlot::NullScale, 
	PSTRICKS_graphics, 
	PSTRICKS_move, 
	PSTRICKS_vector,
	PSTRICKS_linetype, 
	PSTRICKS_put_text, 
	PSTRICKS_text_angle,
	PSTRICKS_justify_text, 
	PSTRICKS_point, 
	PSTRICKS_arrow, 
	set_font_null, 
	PSTRICKS_pointsize,
	TERM_BINARY | TERM_IS_LATEX | TERM_CAN_DASH | TERM_ALPHA_CHANNEL | TERM_LINEWIDTH | TERM_POINTSCALE,
	0 /*suspend*/, 
	0 /*resume*/,
	PSTRICKS_fillbox, 
	PSTRICKS_linewidth,
	#ifdef USE_MOUSE
		0, 
		0, 
		0, 
		0, 
		0,
	#endif
	PSTRICKS_make_palette, 
	0, 
	PSTRICKS_set_color, 
	PSTRICKS_filled_polygon,
	0,     /* image */
	0, 
	0, 
	0,     /* enhanced text */
	0,     /* layer */
	0,     /* path */
	0.0,     /* scale (unused) */
	0,     /* hypertext */
	PSTRICKS_boxed_text,
	0,
	PSTRICKS_dashtype 
TERM_TABLE_END(pstricks_driver)

#undef LAST_TERM
#define LAST_TERM pstricks_driver

#endif /* TERM_TABLE */
#endif /* TERM_PROTO_ONLY */

#ifdef TERM_HELP
START_HELP(pstricks)
"1 pstricks",
"?commands set terminal pstricks",
"?set terminal pstricks",
"?set term pstricks",
"?terminal pstricks",
"?term pstricks",
"?pstricks",
" The `pstricks` driver is intended for use with the \"pstricks.sty\" macro",
" package for LaTeX.  It is an alternative to the `eepic` and `latex` drivers.",
" You need \"pstricks.sty\", and, of course, a printer that understands",
" PostScript, or a converter such as Ghostscript.",
"",
" PSTricks is available at",
"^ <a href=\"http://tug.org/PSTricks/\">",
"           http://tug.org/PSTricks/.",
"^ </a>",
" This driver definitely does not come close to using the full",
" capability of the PSTricks package.",
"",
" Syntax:",
"       set terminal pstricks",
"                      {unit | size <XX>{unit},<YY>{unit}}",
"                      {standalone | input}",
"                      {blacktext | colortext | colourtext}",
"                      {linewidth <lw>} {rounded | butt}",
"                      {pointscale <ps>}",
"                      {psarrows | gparrows}",
"                      {background <rgbcolor>}",
"",
" The `unit` option produces a plot with internal dimensions 1x1. The default",
" is a plot of `size 5in,3in`.",
"",
" `standalone` produces a LaTeX file with possibly multiple plots, ready",
" to be compiled.  The default is `input` to produce a TeX file which can",
" be included.",
"",
" `blacktext` forces all text to be written in black. `colortext` enables",
" colored text. The default is `blacktext`.",
"",
" `rounded` sets line caps and line joins to be rounded. `butt` sets butt",
" caps and mitered joins and is the default.",
"",
" `linewidth` and `pointscale` scale the width of lines and the size of point",
" symbols, respectively.",
"",
" `psarrows` draws `arrow`s using PSTricks commands which are shorter but do",
" not offer all options. `gparrows` selects drawing arrows using gnuplot's own",
" routine for full functionality instead.",
"",
" The old `hacktext` option has been replaced by the new default format (%h),",
" see `format specifiers`.",
"",
" Transparency requires support by Ghostscript or conversion to PDF."
END_HELP(pstricks)
#endif /* TERM_HELP */
