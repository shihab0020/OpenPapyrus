// GNUPLOT - metafont.trm 
// Copyright 1986 - 1993, 1998, 2004   Thomas Williams, Colin Kelley
//
/*
 *			  GNUPLOT -- mf.trm
 *
 *		    This terminal driver supports:
 *		       Metafont Plot Commands
 *
 * Written by : Pl Hedne
 *		Trondheim, Norway
 *		Pal.Hedne@termo.unit.no
 */
/*
 * Improvements and bug fixes by Carsten Steger:
 * - Set default plot size to 5 by 3 inches as in the latex- and eepic-
 *   drivers
 * - Fixed some bugs concerning resolution dependent output
 * - Added MF_scale function
 * - Added MF_justify_text function and modified MF_put_text function and
 *   put_text macro accordingly
 * - Modified MF_move and MF_vector to make output shorter and modified
 *   MF_text accordingly
 * - Added various linetypes by plotting dashed lines; had to modify
 *   MF_linetype and MF_vector for this
 * - Added MF_arrow function
 * - All global variables and #define'd names begin with MF_ now
 * As a consequence almost nothing of the original code by Pl Hedne remains
 * but credit goes to him for the ingenious trick of storing the character
 * images into picture variables, without which this driver would have been
 * impossible for me to write.
 *
 * 10/03/95: Converted to new terminal layout by Carsten Steger.
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
	register_term(mf)
#endif

//#ifdef TERM_PROTO
#define MF_DPI (300)
/* resolution of printer we expect to use; the value itself is not
 * particularly important... it is here only for compatibility to the
 * LaTeX-driver and to get the spacing right. */

/* 5 inches wide by 3 inches high (default) */
#define MF_XSIZE 5.0
#define MF_YSIZE 3.0
#define MF_XMAX (MF_XSIZE*MF_DPI)
#define MF_YMAX (MF_YSIZE*MF_DPI)

#define MF_HTIC (5*MF_DPI/72)
#define MF_VTIC (5*MF_DPI/72)
#define MF_HCHAR (MF_DPI*53/10/72)
#define MF_VCHAR (MF_DPI*11/72)

TERM_PUBLIC void MF_init(GpTermEntry * pThis);
TERM_PUBLIC void MF_graphics(GpTermEntry * pThis);
TERM_PUBLIC void MF_text(GpTermEntry * pThis);
TERM_PUBLIC int  MF_justify_text(GpTermEntry * pThis, enum JUSTIFY mode);
TERM_PUBLIC int  MF_text_angle(GpTermEntry * pThis, int ang);
TERM_PUBLIC void MF_linetype(GpTermEntry * pThis, int linetype);
TERM_PUBLIC void MF_move(GpTermEntry * pThis, uint x, uint y);
TERM_PUBLIC void MF_vector(GpTermEntry * pThis, uint x, uint y);
TERM_PUBLIC void MF_arrow(GpTermEntry * pThis, uint sx, uint sy, uint ex, uint ey, int head);
TERM_PUBLIC void MF_put_text(GpTermEntry * pThis, uint x, uint y, const char * str);
TERM_PUBLIC void MF_reset(GpTermEntry * pThis);

#define GOT_MF_PROTO
//#endif /* TERM_PROTO */

#ifndef TERM_PROTO_ONLY

#ifdef TERM_BODY

/* Plot size in inches */
static double MF_xsize = MF_XSIZE;
static double MF_ysize = MF_YSIZE;
static int MF_char_code;
static int MF_ang;
static int MF_line_type;
static enum JUSTIFY MF_justify;
static double MF_dist_left;
static int MF_is_solid;
static int MF_picked_up_pen;
/*
 * We keep track of where we are with respect to dashed lines by using
 * the next five variables. MF_dash_index indicates which element of
 * MF_lines[..].dashlen should be used. The MF_last.. variables keep
 * track of the position of the pen.
 */
static int MF_dash_index;
static uint MF_last_x, MF_last_y;

static struct {
	int solid; /* Is the line solid? */
	float thickness; /* Thickness of pen we are going to use */
	int dashlen[4]; /* Length of individual segments; even: line; odd: gap */
} MF_lines[10] =
{
	{
		1, 1.5, { 0, 0, 0, 0 }
	},
	{
		0, 1.0, { MF_DPI / 60, MF_DPI / 50, MF_DPI / 60, MF_DPI / 50 }
	},
	{
		1, 1.5, { 0, 0, 0, 0 }
	},
	{
		0, 1.5, { MF_DPI / 20, MF_DPI / 30, MF_DPI / 20, MF_DPI / 30 }
	},
	{
		0, 1.5, { MF_DPI / 30, MF_DPI / 20, MF_DPI / 30, MF_DPI / 20 }
	},
	{
		0, 1.5, { MF_DPI / 15, MF_DPI / 30, MF_DPI / 60, MF_DPI / 30 }
	},
	{
		0, 1.5, { MF_DPI / 30, MF_DPI / 50, MF_DPI / 30, MF_DPI / 50 }
	},
	{
		0, 1.5, { MF_DPI / 20, MF_DPI / 50, MF_DPI / 60, MF_DPI / 30 }
	},
	{
		0, 1.5, { MF_DPI / 30, MF_DPI / 50, MF_DPI / 30, MF_DPI / 30 }
	},
	{
		0, 1.5, { MF_DPI / 60, MF_DPI / 50, MF_DPI / 60, MF_DPI / 30 }
	}
	/* dash: line,     gap,      line,     gap      */
};

TERM_PUBLIC void MF_init(GpTermEntry * pThis)
{
	MF_char_code = 0;
	MF_ang = 0;

	fputs("\
if unknown cmbase: input cmbase fi\n\n\
tracingstats:=1;\n\
picture r[];\n\
\ndef openit = openwindow currentwindow\n\
  from (0,0) to (400,800) at (-50,500) enddef;\n\
\nmode_setup;\n",
	    GPT.P_GpOutFile);

	fputs("\
\n%Include next eight lines if you have problems with the mode on your system..\n\
%proofing:=0;\n\
%fontmaking:=1;\n\
%tracingtitles:=0;\n\
%pixels_per_inch:=300;\n\
%blacker:=0;\n\
%fillin:=.2;\n\
%o_correction:=.6;\n\
%fix_units;\n",
	    GPT.P_GpOutFile);

	/* Next lines must be included if text support is needed (CM base used) */
	fputs("\
\ndef put_text(expr ts,xstart,ystart,rot,justification) =\n\
  begingroup\n\
    text_width:=0;text_height:=0;text_depth:=0;\n\
    for ind:=0 step 1 until length(ts)-1:\n\
      dec_num:=ASCII substring (ind,ind+1) of ts;\n\
      if unknown r[dec_num]: dec_num:=32; fi\n\
      if dec_num=32: \n\
        text_width:=text_width+wd[65];\n\
        text_height:=max(text_height,ht[65]);\n\
        text_depth:=max(text_depth,dp[65]);\n\
      elseif dec_num>=0: \n\
        text_width:=text_width+wd[dec_num];\n\
        text_height:=max(text_height,ht[dec_num]);\n\
        text_depth:=max(text_depth,dp[dec_num]);\n\
      fi\n\
    endfor\n\
    if rot=90:\n\
      if justification=1: ynext:=ystart;\n\
      elseif justification=2: ynext:=round(ystart-text_width/2);\n\
      else: ynext:=round(ystart-text_width);\n\
      fi\n\
      xnext:=xstart+(text_height-text_depth)/2;\n\
    else:\n\
      if justification=1: xnext:=xstart;\n\
      elseif justification=2: xnext:=round(xstart-text_width/2);\n\
      else: xnext:=round(xstart-text_width);\n\
      fi\n\
      ynext:=ystart-(text_height-text_depth)/2;\n\
    fi\n\
    for ind:=0 step 1 until length(ts)-1:\n\
      dec_num:=ASCII substring (ind,ind+1) of ts;\n\
      if unknown r[dec_num]: dec_num:=32; fi\n\
      if dec_num=32: \n\
        xnext:=xnext+wd[65]*cosd rot;\n\
        ynext:=ynext+wd[65]*sind rot;\n\
      elseif dec_num>=0: \n\
        currentpicture:=currentpicture+r[dec_num] shifted(xnext,ynext)\n\
          rotatedaround ((xnext,ynext),rot); \n\
        xnext:=xnext+wd[dec_num]*cosd rot;\n\
        ynext:=ynext+wd[dec_num]*sind rot;\n\
      fi\n\
    endfor\n\
  endgroup \n\
enddef;\n",
	    GPT.P_GpOutFile);

	fputs("\
\ndef endchar =\n\
  r[charcode]:=currentpicture;\n\
  wd[charcode]:=w;ht[charcode]:=h;dp[charcode]:=d;\n\
  message \"Picture of charcode no.\" & decimal charcode;\n\
  endgroup;\n\
enddef;\n\
let endchar_ = endchar;\n\
let generate = relax;\n\
let roman = relax;\n",
	    GPT.P_GpOutFile);

	fputs("\
input cmr10.mf\n\
if ligs>1: font_coding_scheme:=\"TeX text\";\n\
  spanish_shriek=oct\"074\"; spanish_query=oct\"076\";\n\
else: font_coding_scheme:=\n\
  if ligs=0: \"TeX typewriter text\"\n\
  else: \"TeX text without f-ligatures\" fi;\n\
  spanish_shriek=oct\"016\"; spanish_query=oct\"017\"; fi\n\
font_setup;\n\
input romanu.mf %Roman uppercase.\n\
input romanl.mf %Roman lowercase.\n\
input greeku.mf %Greek uppercase.\n\
input romand.mf %Numerals.\n\
input romanp.mf %Ampersand, question marks, currency sign.\n\
input romspl.mf %Lowercase specials (dotless \\i, ligature \\ae, etc.)\n\
input romspu.mf %Uppercase specials (\\AE, \\OE, \\O)\n\
input punct.mf %Punctuation symbols.\n\
\nminus=ASCII\"-\"; cmchar \"Minus sign\";\n\
 beginarithchar(minus); \n\
  pickup rule.nib;\n\
  lft x1=hround 1.5u-eps;\n\
  x2=w-x1; y1=y2=math_axis;\n\
  draw z1--z2;	 % bar\n\
  labels(1,2); \n\
endchar;\n",
	    GPT.P_GpOutFile);

	fputs("\
\ncmchar \"Period\";\n\
  numeric dot_diam#; dot_diam#:=if monospace: 5/4 fi\\ dot_size#;\n\
  define_whole_blacker_pixels(dot_diam);\n\
  beginchar(\".\",5u#,dot_diam#,0);\n\
  adjust_fit(0,0); pickup fine.nib;\n\
  pos1(dot_diam,0); pos2(dot_diam,90);\n\
  lft x1l=hround(.5w-.5dot_diam); bot y2l=0; z1=z2; dot(1,2);	% dot\n\
  penlabels(1,2);\n\
endchar;\n",
	    GPT.P_GpOutFile);

	fputs("\
\ndef endchar =\n\
  % Next line should probably be removed if CM base is used\n\
  l:=0; r:=w;\n\
  %Include the next two lines if you want to\n\
  %rotate the picture 90 deg.(Portrait to Landscape)\n\
  %currentpicture:=currentpicture rotated 90 shifted (h,0);\n\
  %tmp:=charht; charht:=charwd; charwd:=tmp;\n\
  scantokens extra_endchar;\n\
  if proofing>0: makebox(proofrule); fi\n\
  chardx:=w;\n\
  shipit;\n\
  if displaying>0: makebox(screenrule); showit; fi\n\
  endgroup \n\
enddef;\n\
let endchar_ = endchar;\n\
let generate = input;\n\
let roman = roman;\n",
	    GPT.P_GpOutFile);

	/* font_size must be bigger than em#/16 by METAFONT rules.
	 * Therefore make it pretty big so big figures will be
	 * handled correctly. Setting font_size to 72pt# lets us
	 * handle characters up to 15.94 by 15.94 inches. */
	fputs("\
\n\nfont_identifier:=\"GNUPLOT\";\n\
font_size 72pt#;\n\
th#=0.4pt#; define_whole_pixels(th);\n\
\npath arrowhead;\n\
arrowhead = (-7pt,-2pt){dir30}..(-6pt,0pt)..\
{dir150}(-7pt,2pt) &\n\
  (-7pt,2pt)--(0pt,0pt)--(-7pt,-2pt) & cycle;\n",
	    GPT.P_GpOutFile);
}

TERM_PUBLIC void MF_graphics(GpTermEntry * pThis)
{
	fprintf(GPT.P_GpOutFile, "\n\nbeginchar(%d,%gin#,%gin#,0);\n", MF_char_code, MF_xsize, MF_ysize);
	MF_char_code++;
	fprintf(GPT.P_GpOutFile, "a:=w/%d;b:=h/%d;\n", pThis->MaxX, pThis->MaxY);
	MF_picked_up_pen = 0;
}

TERM_PUBLIC void MF_text(GpTermEntry * pThis)
{
	fputs("endchar;\n", GPT.P_GpOutFile);
}

TERM_PUBLIC int MF_justify_text(GpTermEntry * pThis, enum JUSTIFY mode)
{
	MF_justify = mode;
	return TRUE;
}

TERM_PUBLIC int MF_text_angle(GpTermEntry * pThis, int ang)
{
	MF_ang = (ang > 0) ? 90 : 0;
	return TRUE;
}

TERM_PUBLIC void MF_linetype(GpTermEntry * pThis, int linetype)
{
	if(linetype >= 8)
		linetype %= 8;
	linetype += 2;
	if(linetype < 0)
		linetype = 0;
	/* Only output change in pens if it actually affects the pen used */
	if((MF_lines[linetype].thickness != MF_lines[MF_line_type].thickness) ||
	    (!MF_picked_up_pen)) {
		fprintf(GPT.P_GpOutFile, "pickup pencircle scaled %gth;\n",
		    MF_lines[linetype].thickness);
		MF_picked_up_pen = 1;
	}
	MF_line_type = linetype;
	MF_dash_index = 0;
	MF_dist_left = MF_lines[MF_line_type].dashlen[MF_dash_index];
	MF_is_solid = MF_lines[MF_line_type].solid;
}

TERM_PUBLIC void MF_move(GpTermEntry * pThis, uint x, uint y)
{
	MF_last_x = x;
	MF_last_y = y;
	MF_dash_index = 0;
	MF_dist_left = MF_lines[MF_line_type].dashlen[MF_dash_index];
}

TERM_PUBLIC void MF_vector(GpTermEntry * pThis, uint x, uint y)
{
	if(MF_is_solid) {
		if(x == MF_last_x && y == MF_last_y)
			fprintf(GPT.P_GpOutFile, "drawdot (%da,%db);\n", x, y);
		else
			fprintf(GPT.P_GpOutFile, "draw (%da,%db)--(%da,%db);\n", MF_last_x, MF_last_y, x, y);
	}
	else {
		double dist_to_go, delta_x, delta_y, inc_x, inc_y;
		double last_x_d, last_y_d, next_x_d, next_y_d;
		uint next_x, next_y;
		if(x == MF_last_x && y == MF_last_y) {
			if(!(MF_dash_index & 1))
				fprintf(GPT.P_GpOutFile, "drawdot (%da,%db);\n", x, y);
		}
		else {
			last_x_d = MF_last_x;
			last_y_d = MF_last_y;
			delta_x = x - last_x_d;
			delta_y = y - last_y_d;
			dist_to_go = sqrt(delta_x * delta_x + delta_y * delta_y);
			inc_x = delta_x / dist_to_go;
			inc_y = delta_y / dist_to_go;
			while(MF_dist_left < dist_to_go) {
				next_x_d = last_x_d + inc_x * MF_dist_left;
				next_y_d = last_y_d + inc_y * MF_dist_left;
				next_x = ffloori(next_x_d + 0.5);
				next_y = ffloori(next_y_d + 0.5);
				// MF_dash_index & 1 == 0 means: draw a line; otherwise just move 
				if(!(MF_dash_index & 1))
					fprintf(GPT.P_GpOutFile, "draw (%da,%db)--(%da,%db);\n", MF_last_x, MF_last_y, next_x, next_y);
				MF_last_x = next_x;
				MF_last_y = next_y;
				last_x_d = next_x_d;
				last_y_d = next_y_d;
				dist_to_go -= MF_dist_left;
				MF_dash_index = (MF_dash_index + 1) & 3;
				MF_dist_left = MF_lines[MF_line_type].dashlen[MF_dash_index];
			}
			delta_x = x - last_x_d;
			delta_y = y - last_y_d;
			MF_dist_left -= sqrt(delta_x * delta_x + delta_y * delta_y);
			if(!(MF_dash_index & 1)) {
				if(x == MF_last_x && y == MF_last_y)
					fprintf(GPT.P_GpOutFile, "drawdot (%da,%db);\n", x, y);
				else
					fprintf(GPT.P_GpOutFile, "draw (%da,%db)--(%da,%db);\n", MF_last_x, MF_last_y, x, y);
			}
		}
	}
	MF_last_x = x;
	MF_last_y = y;
}

TERM_PUBLIC void MF_arrow(GpTermEntry * pThis, uint sx, uint sy, uint ex, uint ey, int head)
{
	int delta_x, delta_y;
	MF_move(pThis, sx, sy);
	MF_vector(pThis, ex, ey);
	if(head) {
		delta_x = ex - sx;
		delta_y = ey - sy;
		fprintf(GPT.P_GpOutFile, "fill arrowhead rotated angle(%d,%d) shifted (%da,%db);\n", delta_x, delta_y, ex, ey);
	}
}

TERM_PUBLIC void MF_put_text(GpTermEntry * pThis, uint x, uint y, const char * str)
{
	// ignore empty strings 
	if(isempty(str)) {
		// F***. why do drivers need to modify string args? 
		char * text = sstrdup(str);
		int    j = 0;
		{
			const size_t tl = strlen(text);
			for(size_t i = 0; i < tl; i++)
				if(text[i] == '"')
					text[i] = '\''; // Replace " with ' 
		}
		switch(MF_justify) {
			case LEFT: j = 1; break;
			case CENTRE: j = 2; break;
			case RIGHT: j = 3; break;
		}
		fprintf(GPT.P_GpOutFile, "put_text(\"%s\",%da,%db,%d,%d);\n", text, x, y, MF_ang, j);
		SAlloc::F(text);
	}
}

TERM_PUBLIC void MF_reset(GpTermEntry * pThis)
{
	fputs("end.\n", GPT.P_GpOutFile);
}

#endif /* TERM_BODY */

#ifdef TERM_TABLE

TERM_TABLE_START(mf_driver)
	"mf", 
	"Metafont plotting standard",
	static_cast<uint>(MF_XMAX),
	static_cast<uint>(MF_YMAX),
	MF_VCHAR, 
	MF_HCHAR,
	MF_VTIC, 
	MF_HTIC, 
	GnuPlot::OptionsNull, 
	MF_init, 
	MF_reset,
	MF_text, 
	GnuPlot::NullScale, 
	MF_graphics, 
	MF_move, 
	MF_vector,
	MF_linetype, 
	MF_put_text, 
	MF_text_angle,
	MF_justify_text, 
	GnuPlot::LineAndPoint, 
	MF_arrow, 
	set_font_null 
TERM_TABLE_END(mf_driver)

#undef LAST_TERM
#define LAST_TERM mf_driver

#endif /* TERM_TABLE */
#endif /* TERM_PROTO_ONLY */

#ifdef TERM_HELP
START_HELP(mf)
"1 mf",
"?commands set terminal mf",
"?set terminal mf",
"?set term mf",
"?terminal mf",
"?term mf",
"?mf",
"?metafont",
" The `mf` terminal driver creates an input file to the METAFONT program.  Thus a",
" figure may be used in the TeX document in the same way as is a character.",
"",
" To use a picture in a document, the METAFONT program must be run with the",
" output file from `gnuplot` as input.  Thus, the user needs a basic knowledge",
" of the font creating process and the procedure for including a new font in a",
" document.  However, if the METAFONT program is set up properly at the local",
" site, an unexperienced user could perform the operation without much trouble.",
"",
" The text support is based on a METAFONT character set.  Currently the",
" Computer Modern Roman font set is input, but the user is in principal free to",
" choose whatever fonts he or she needs.  The METAFONT source files for the",
" chosen font must be available.  Each character is stored in a separate",
" picture variable in METAFONT.  These variables may be manipulated (rotated,",
" scaled etc.) when characters are needed.  The drawback is the interpretation",
" time in the METAFONT program.  On some machines (i.e. PC) the limited amount",
" of memory available may also cause problems if too many pictures are stored.",
"",
" The `mf` terminal has no options.",
"2 METAFONT Instructions",
"?commands set terminal mf detailed",
"?set terminal mf detailed",
"?set term mf detailed",
"?mf detailed",
"?metafont detailed",
"",
" - Set your terminal to METAFONT:",
"   set terminal mf",
" - Select an output-file, e.g.:",
"   set output \"myfigures.mf\"",
" - Create your pictures. Each picture will generate a separate character. Its",
" default size will be 5*3 inches. You can change the size by saying `set size",
" 0.5,0.5` or whatever fraction of the default size you want to have.",
"",
" - Quit `gnuplot`.",
"",
" - Generate a TFM and GF file by running METAFONT on the output of `gnuplot`.",
" Since the picture is quite large (5*3 in), you will have to use a version of",
" METAFONT that has a value of at least 150000 for memmax.  On Unix systems",
" these are conventionally installed under the name bigmf.  For the following",
" assume that the command virmf stands for a big version of METAFONT.  For",
" example:",
"",
" - Invoke METAFONT:",
"     virmf '&plain'",
" - Select the output device: At the METAFONT prompt ('*') type:",
"     \\mode:=CanonCX;     % or whatever printer you use",
" - Optionally select a magnification:",
"     mag:=1;             % or whatever you wish",
" - Input the `gnuplot`-file:",
"     input myfigures.mf",
" On a typical Unix machine there will usually be a script called \"mf\" that",
" executes virmf '&plain', so you probably can substitute mf for virmf &plain.",
" This will generate two files: mfput.tfm and mfput.$$$gf (where $$$ indicates",
" the resolution of your device).  The above can be conveniently achieved by",
" typing everything on the command line, e.g.:",
" virmf '&plain' '\\mode:=CanonCX; mag:=1; input myfigures.mf'",
" In this case the output files will be named myfigures.tfm and",
" myfigures.300gf.",
"",
" - Generate a PK file from the GF file using gftopk:",
"   gftopk myfigures.300gf myfigures.300pk",
" The name of the output file for gftopk depends on the DVI driver you use.",
" Ask your local TeX administrator about the naming conventions.  Next, either",
" install the TFM and PK files in the appropriate directories, or set your",
" environment variables properly.  Usually this involves setting TEXFONTS to",
" include the current directory and doing the same thing for the environment",
" variable that your DVI driver uses (no standard name here...).  This step is",
" necessary so that TeX will find the font metric file and your DVI driver will",
" find the PK file.",
"",
" - To include your pictures in your document you have to tell TeX the font:",
"   \\font\\gnufigs=myfigures",
" Each picture you made is stored in a single character.  The first picture is",
" character 0, the second is character 1, and so on...  After doing the above",
" step, you can use the pictures just like any other characters.  Therefore, to",
" place pictures 1 and 2 centered in your document, all you have to do is:",
"   \\centerline{\\gnufigs\\char0}",
"   \\centerline{\\gnufigs\\char1}",
" in plain TeX.  For LaTeX you can, of course, use the picture environment and",
" place the picture wherever you wish by using the \\makebox and \\put macros.",
"",
" This conversion saves you a lot of time once you have generated the font;",
" TeX handles the pictures as characters and uses minimal time to place them,",
" and the documents you make change more often than the pictures do.  It also",
" saves a lot of TeX memory.  One last advantage of using the METAFONT driver",
" is that the DVI file really remains device independent, because no \\special",
" commands are used as in the eepic and tpic drivers."
END_HELP(mf)
#endif /* TERM_HELP */
