// GNUPLOT - boundary.c 
// Copyright 1986 - 1993, 1998, 2004   Thomas Williams, Colin Kelley
//
#include <gnuplot.h>
#pragma hdrstop

#define ERRORBARTIC(terminalPtr) MAX(((terminalPtr)->TicH/2), 1)

struct GpBoundary {
	GpBoundary() :
		xlablin(0),
		x2lablin(0),
		ylablin(0),
		y2lablin(0),
		titlelin(0),
		xticlin(0),
		x2ticlin(0),
		key_sample_width(0),
		key_sample_height(0),
		key_sample_left(0),
		key_sample_right(0),
		key_text_left(0),
		key_text_right(0),
		key_size_left(0),
		key_size_right(0),
		key_xleft(0),
		max_ptitl_len(0),
		ptitl_cnt(0),
		key_width(0),
		key_height(0),
		key_title_height(0),
		key_title_extra(0),
		key_title_ypos(0),
		time_y(0),
		time_x(0),
		key_col_wth(0),
		yl_ref(0),
		xl(0),
		yl(0)
	{
	}
	//
	// stupid test used in only one place but it refers to our local variables 
	//
	bool AtLeftOfKey() const
	{
		return (yl == yl_ref);
	}
	int xlablin;
	int x2lablin;
	int ylablin;
	int y2lablin;
	int titlelin;
	int xticlin;
	int x2ticlin;
	//
	int key_sample_width;    // width of line sample 
	int key_sample_height;   // sample itself; does not scale with "set key spacing" 
	int key_sample_left;     // offset from x for left of line sample 
	int key_sample_right;    // offset from x for right of line sample 
	int key_text_left;       // offset from x for left-justified text 
	int key_text_right;      // offset from x for right-justified text 
	int key_size_left;       // size of left bit of key (text or sample, depends on key->reverse) 
	int key_size_right;      // size of right part of key (including padding) 
	int key_xleft;           // Amount of space on the left required by the key 
	int max_ptitl_len;       // max length of plot-titles (keys) 
	int ptitl_cnt;           // count keys with len > 0  
	int key_width;           // calculate once, then everyone uses it 
	int key_height;          // ditto 
	int key_title_height;    // nominal number of lines * character height 
	int key_title_extra;     // allow room for subscript/superscript 
	int key_title_ypos;      // offset from key->bounds.ytop 
	int time_y;
	int time_x;
	//
	int key_col_wth;
	int yl_ref;
	int xl;
	int yl;
};

static GpBoundary _Bry; // @global

bool at_left_of_key() { return _Bry.AtLeftOfKey(); }

//  local variables 
//static int xlablin;
//static int x2lablin;
//static int ylablin;
//static int y2lablin;
//static int titlelin;
//static int xticlin;
//static int x2ticlin;
// local and global variables 
//static int key_sample_width;    // width of line sample 
//static int key_sample_height;   // sample itself; does not scale with "set key spacing" 
//static int key_sample_left;     // offset from x for left of line sample 
//static int key_sample_right;    // offset from x for right of line sample 
//static int key_text_left;       // offset from x for left-justified text 
//static int key_text_right;      // offset from x for right-justified text 
//static int key_size_left;       // size of left bit of key (text or sample, depends on key->reverse) 
//static int key_size_right;      // size of right part of key (including padding) 
//static int key_xleft;           // Amount of space on the left required by the key 
//static int max_ptitl_len = 0;   // max length of plot-titles (keys) 
//static int ptitl_cnt;           // count keys with len > 0  
//static int key_width;           // calculate once, then everyone uses it 
//static int key_height;          // ditto 
//static int key_title_height;    // nominal number of lines * character height 
//static int key_title_extra;     // allow room for subscript/superscript 
//static int key_title_ypos;      // offset from key->bounds.ytop 
//static int time_y;
//static int time_x;

int title_x; // Used by boundary and by 2D graphics 
int title_y; 
//
// These quantities are needed in do_plot() e.g. for histogtram title layout
//
int key_entry_height; // bigger of t->ChrV, t->TicV 
int key_point_offset; // offset from x for point sample 
int ylabel_x;
int xlabel_y;
int y2label_x;
int x2label_y;
int x2label_yoffset;
int ylabel_y;
int y2label_y;
int xtic_y;
int x2tic_y;
int ytic_x;
int y2tic_x;
int key_rows;
int key_cols;
int key_count;

/*{{{  boundary() */
/* borders of plotting area
 * computed once on every call to do_plot
 *
 * The order in which things are done has become critical:
 *  GPO.V.BbPlot.ytop depends on title, x2label
 *  GPO.V.BbPlot.ybot depends on key, if "under"
 *  once we have these, we can setup the y1 and y2 tics and the
 *  only then can we calculate GPO.V.BbPlot.xleft and GPO.V.BbPlot.xright
 *  GPO.V.BbPlot.xright depends also on key RIGHT
 *  then we can do x and x2 tics
 *
 * For set size ratio ..., everything depends on everything else...
 * not really a lot we can do about that, so we lose if the plot has to
 * be reduced vertically. But the chances are the
 * change will not be very big, so the number of tics will not
 * change dramatically.
 *
 * Margin computation redone by Dick Crawford (rccrawford@lanl.gov) 4/98
 */
//void boundary(GpTermEntry * pTerm, curve_points * plots, int count)
void GnuPlot::Boundary(GpTermEntry * pTerm, const curve_points * pPlots, int count)
{
	int    yticlin = 0;
	int    y2ticlin = 0;
	legend_key * key = &keyT;
	const  int can_rotate = (pTerm->text_angle)(TEXT_VERTICAL);
	int    xtic_textheight = 0; /* height of xtic labels */
	int    x2tic_textheight = 0; /* height of x2tic labels */
	int    title_textheight = 0; /* height of title */
	int    xlabel_textheight = 0; /* height of xlabel */
	int    x2label_textheight = 0; /* height of x2label */
	int    ylabel_textwidth = 0; /* width of (rotated) ylabel */
	int    y2label_textwidth = 0; /* width of (rotated) y2label */
	int    timelabel_textwidth = 0; /* width of timestamp */
	int    timelabel_textheight = 0; /* height of timestamp */
	int    ytic_textwidth = 0; /* width of ytic labels */
	int    y2tic_textwidth = 0; /* width of y2tic labels */
	int    x2tic_height = 0;   /* 0 for TicIn or no x2tics, ticscale*TicV otherwise */
	int    xtic_textwidth = 0; /* amount by which the xtic label protrude to the right */
	int    xtic_height = 0;
	int    ytic_width = 0;
	int    y2tic_width = 0;
	int    ttic_textheight = 0; // vertical clearance for ttics 
	// figure out which rotatable items are to be rotated
	// (ylabel and y2label are rotated if possible) 
	const int vertical_timelabel = can_rotate ? Gg.LblTime.rotate : 0;
	const int vertical_xtics  = can_rotate ? AxS[FIRST_X_AXIS].tic_rotate : 0;
	const int vertical_x2tics = can_rotate ? AxS[SECOND_X_AXIS].tic_rotate : 0;
	const int vertical_ytics  = can_rotate ? AxS[FIRST_Y_AXIS].tic_rotate : 0;
	const int vertical_y2tics = can_rotate ? AxS[SECOND_Y_AXIS].tic_rotate : 0;
	bool shift_labels_to_border = FALSE;
	_Bry.xticlin = 0;
	_Bry.ylablin = 0;
	_Bry.y2lablin = 0;
	_Bry.xlablin = 0;
	_Bry.x2lablin = 0;
	_Bry.titlelin = 0;
	/*{{{  count lines in labels and tics */
	if(Gg.LblTitle.text)
		label_width(Gg.LblTitle.text, &_Bry.titlelin);
	if(AxS[FIRST_X_AXIS].label.text)
		label_width(AxS[FIRST_X_AXIS].label.text, &_Bry.xlablin);
	// This should go *inside* label_width(), but it messes up the key title 
	// Imperfect check for subscripts or superscripts 
	if((pTerm->flags & TERM_ENHANCED_TEXT) && AxS[FIRST_X_AXIS].label.text && strpbrk(AxS[FIRST_X_AXIS].label.text, "_^"))
		_Bry.xlablin++;
	if(AxS[SECOND_X_AXIS].label.text)
		label_width(AxS[SECOND_X_AXIS].label.text, &_Bry.x2lablin);
	if(AxS[FIRST_Y_AXIS].label.text)
		label_width(AxS[FIRST_Y_AXIS].label.text, &_Bry.ylablin);
	if(AxS[SECOND_Y_AXIS].label.text)
		label_width(AxS[SECOND_Y_AXIS].label.text, &_Bry.y2lablin);
	if(AxS[FIRST_X_AXIS].ticmode) {
		label_width(AxS[FIRST_X_AXIS].formatstring, &_Bry.xticlin);
		// Reserve room for user tic labels even if format of autoticks is "" 
		if(_Bry.xticlin == 0 && AxS[FIRST_X_AXIS].ticdef.def.user)
			_Bry.xticlin = 1;
	}
	if(AxS[SECOND_X_AXIS].ticmode)
		label_width(AxS[SECOND_X_AXIS].formatstring, &_Bry.x2ticlin);
	if(AxS[FIRST_Y_AXIS].ticmode)
		label_width(AxS[FIRST_Y_AXIS].formatstring, &yticlin);
	if(AxS[SECOND_Y_AXIS].ticmode)
		label_width(AxS[SECOND_Y_AXIS].formatstring, &y2ticlin);
	/*}}} */

	/*{{{  preliminary V.BbPlot.ytop  calculation */

	/*     first compute heights of things to be written in the margin */

	/* Title placement has been reworked for 5.4
	 * NOTE: title_textheight is _not_ the height of the title!
	 * It is the amount of space reserved for the title above the plot.
	 * A negative offset greater than the number of title lines means
	 * that the title will appear inside the boundary and no extra space
	 * needs to be reserved for it above the plot.
	 */
	title_textheight = 0;
	if(_Bry.titlelin) {
		title_textheight = pTerm->ChrV; /* Gap of one normal line height */
		if(Gg.LblTitle.font)
			pTerm->set_font(pTerm, Gg.LblTitle.font);
		title_y = _Bry.titlelin * pTerm->ChrV;
		if((_Bry.titlelin + Gg.LblTitle.offset.y) > 0)
			title_textheight += _Bry.titlelin * pTerm->ChrV;
		if(Gg.LblTitle.font)
			pTerm->set_font(pTerm, "");
		title_y += 0.5 * pTerm->ChrV; /* Approximate same placement as version 5.2 */
	}
	// Extra space at the top for spiderplot axis label 
	if(Gg.SpiderPlot)
		title_textheight += 1.5 * pTerm->ChrV;
	// x2label 
	if(_Bry.x2lablin) {
		double tmpx, tmpy;
		MapPositionR(pTerm, &(AxS[SECOND_X_AXIS].label.offset), &tmpx, &tmpy, "x2label");
		if(AxS[SECOND_X_AXIS].label.font)
			pTerm->set_font(pTerm, AxS[SECOND_X_AXIS].label.font);
		x2label_textheight = (int)(_Bry.x2lablin * pTerm->ChrV);
		x2label_yoffset = static_cast<int>(tmpy);
		if(AxS[SECOND_X_AXIS].label.font)
			pTerm->set_font(pTerm, "");
	}
	else
		x2label_textheight = 0;
	// tic labels 
	if(AxS[SECOND_X_AXIS].ticmode & TICS_ON_BORDER) {
		// ought to consider tics on axes if axis near border 
		x2tic_textheight = (int)(_Bry.x2ticlin * pTerm->ChrV);
	}
	else
		x2tic_textheight = 0;
	// tics 
	if(AxS[SECOND_X_AXIS].ticmode & TICS_ON_BORDER) {
		x2tic_height = static_cast<int>(pTerm->TicV * AxS[SECOND_X_AXIS].ticscale);
		if(AxS[SECOND_X_AXIS].TicIn)
			x2tic_height = -x2tic_height;
	}
	else
		x2tic_height = 0;
	// Polar (theta) tic labels need space at top and bottom of plot 
	if(AxS.Theta().ticmode) {
		// FIXME:  Really 5% of polar grid radius, but we don't know that yet 
		ttic_textheight = 2 * pTerm->ChrV;
	}
	// timestamp 
	if(Gg.LblTime.text) {
		int timelin;
		timelabel_textwidth = label_width(Gg.LblTime.text, &timelin);
		if(vertical_timelabel) {
			timelabel_textheight = timelabel_textwidth * pTerm->ChrV;
			timelabel_textwidth = static_cast<int>((timelin + 1.5) * pTerm->ChrH);
			Gg.LblTime.place.y = 0;
		}
		else {
			timelabel_textheight = timelin * pTerm->ChrV;
			timelabel_textwidth = timelabel_textwidth * pTerm->ChrH;
			// save textheight for use in do_key_bounds() 
			Gg.LblTime.place.y = timelabel_textheight;
		}
	}
	// ylabel placement 
	if(AxS[FIRST_Y_AXIS].label.text) {
		if(can_rotate && AxS[FIRST_Y_AXIS].label.rotate != 0) {
			ylabel_textwidth = _Bry.ylablin * pTerm->ChrV;
		}
		else {
			// Trying to estimate this length caused more problems than it solved.
			// For one thing it comes out wrong for text passed to TeX terminals.
			// Assume the common case is roughly 3 character widths and let the
			// user adjust lmargin and offset for longer non-rotated ylabels.
			ylabel_textwidth = 3 * pTerm->ChrH;
		}
	}
	// y2label placement 
	if(AxS[SECOND_Y_AXIS].label.text) {
		if(can_rotate && AxS[SECOND_Y_AXIS].label.rotate != 0) {
			y2label_textwidth = _Bry.y2lablin * pTerm->ChrV;
			if(!AxS[SECOND_Y_AXIS].ticmode)
				y2label_textwidth += 0.5 * pTerm->ChrV;
		}
		else {
			// See above. Estimating true text length causes problems 
			y2label_textwidth = 3 * pTerm->ChrH;
		}
	}
	// compute V.BbPlot.ytop from the various components unless tmargin is explicitly specified
	V.BbPlot.ytop = (int)(0.5 + (V.Size.y + V.Offset.Y) * (pTerm->MaxY-1));
	// Sanity check top and bottom margins, in case the user got confused 
	if(V.MarginB.scalex == screen && V.MarginT.scalex == screen) {
		ExchangeToOrder(&V.MarginB.x, &V.MarginT.x);
	}
	if(V.MarginT.scalex == screen) {
		V.BbPlot.ytop = static_cast<int>((V.MarginT.x) * (float)(pTerm->MaxY-1)); // Specified as absolute position on the canvas 
	}
	else if(V.MarginT.x >=0) {
		V.BbPlot.ytop -= (int)(V.MarginT.x * (float)pTerm->ChrV + 0.5); // Specified in terms of character height 
	}
	else {
		// Auto-calculation of space required 
		int top_margin = title_textheight;
		if(x2label_textheight + x2label_yoffset > 0)
			top_margin += x2label_textheight;
		if(timelabel_textheight > top_margin && !Gg.TimeLabelBottom && !vertical_timelabel)
			top_margin = timelabel_textheight;
		top_margin += x2tic_textheight;
		top_margin += pTerm->ChrV;
		if(x2tic_height > 0)
			top_margin += x2tic_height;
		top_margin += ttic_textheight;
		V.BbPlot.ytop -= top_margin;
		if(V.BbPlot.ytop == (int)(0.5 + (V.Size.y + V.Offset.Y) * (pTerm->MaxY-1))) {
			V.BbPlot.ytop -= (int)(pTerm->ChrH * 2); // make room for the end of rotated ytics or y2tics 
		}
	}
	// end of preliminary V.BbPlot.ytop calculation }}} 
	// {{{  preliminary V.BbPlot.xleft, needed for "under" 
	if(V.MarginL.scalex == screen)
		V.BbPlot.xleft = static_cast<int>(V.MarginL.x * pTerm->MaxX);
	else
		V.BbPlot.xleft = static_cast<int>(V.Offset.X * pTerm->MaxX + pTerm->ChrH * (V.MarginL.x >= 0 ? V.MarginL.x : 1.0));
	// }}} 
	// {{{  tentative V.BbPlot.xright, needed for "under" 
	if(V.MarginR.scalex == screen)
		V.BbPlot.xright = static_cast<int>(V.MarginR.x * (float)(pTerm->MaxX-1));
	else
		V.BbPlot.xright = static_cast<int>((V.Size.x + V.Offset.X) * (pTerm->MaxX-1) - pTerm->ChrH * (V.MarginR.x >= 0 ? V.MarginR.x : 2.0));
	// }}} 
	// {{{  preliminary V.BbPlot.ybot calculation first compute heights of labels and tics 
	// tic labels 
	shift_labels_to_border = FALSE;
	if(AxS[FIRST_X_AXIS].ticmode & TICS_ON_AXIS) {
		/* FIXME: This test for how close the axis is to the border does not match */
		/*        the tests in axis_output_tics(), and assumes FIRST_Y_AXIS.       */
		if(!AxS[FIRST_Y_AXIS].InRange(0.0))
			shift_labels_to_border = TRUE;
		if(0.05 > fabs(AxS[FIRST_Y_AXIS].min / (AxS[FIRST_Y_AXIS].max - AxS[FIRST_Y_AXIS].min)))
			shift_labels_to_border = TRUE;
	}
	if((AxS[FIRST_X_AXIS].ticmode & TICS_ON_BORDER) || shift_labels_to_border) {
		xtic_textheight = (int)(pTerm->ChrV * (_Bry.xticlin + 1));
	}
	else
		xtic_textheight =  0;
	// tics 
	if(!AxS[FIRST_X_AXIS].TicIn && ((AxS[FIRST_X_AXIS].ticmode & TICS_ON_BORDER) || 
		((AxS[SECOND_X_AXIS].ticmode & TICS_MIRROR) && (AxS[SECOND_X_AXIS].ticmode & TICS_ON_BORDER))))
		xtic_height = (int)(pTerm->TicV * AxS[FIRST_X_AXIS].ticscale);
	else
		xtic_height = 0;
	// xlabel 
	if(_Bry.xlablin) {
		double tmpx, tmpy;
		MapPositionR(pTerm, &(AxS[FIRST_X_AXIS].label.offset), &tmpx, &tmpy, "boundary");
		// offset is subtracted because if > 0, the margin is smaller 
		// textheight is inflated by 0.2 to allow descenders to clear bottom of canvas 
		xlabel_textheight = static_cast<int>((_Bry.xlablin + 0.2f) * pTerm->ChrV - tmpy);
		if(!AxS[FIRST_X_AXIS].ticmode)
			xlabel_textheight += 0.5 * pTerm->ChrV;
	}
	else
		xlabel_textheight = 0;
	// compute V.BbPlot.ybot from the various components unless bmargin is explicitly specified  
	V.BbPlot.ybot = static_cast<int>(V.Offset.Y * pTerm->MaxY);
	if(V.MarginB.scalex == screen) {
		// Absolute position for bottom of plot 
		V.BbPlot.ybot = static_cast<int>(V.MarginB.x * pTerm->MaxY);
	}
	else if(V.MarginB.x >= 0) {
		// Position based on specified character height 
		V.BbPlot.ybot += V.MarginB.x * (float)pTerm->ChrV + 0.5;
	}
	else {
		V.BbPlot.ybot += xtic_height + xtic_textheight;
		if(xlabel_textheight > 0)
			V.BbPlot.ybot += xlabel_textheight;
		if(!vertical_timelabel && Gg.TimeLabelBottom && timelabel_textheight > 0)
			V.BbPlot.ybot += timelabel_textheight;
		if(V.BbPlot.ybot == (int)(pTerm->MaxY * V.Offset.Y)) {
			// make room for the end of rotated ytics or y2tics 
			V.BbPlot.ybot += (int)(pTerm->ChrH * 2);
		}
		if(Gg.SpiderPlot) // Extra space at the bottom for spiderplot axis label 
			V.BbPlot.ybot += 2 * pTerm->ChrH;
		// Last chance for better estimate of space required for ttic labels 
		// It is too late to go back and adjust positions relative to ytop 
		if(ttic_textheight > 0) {
			ttic_textheight = static_cast<int>(0.05f * (V.BbPlot.ytop - V.BbPlot.ybot));
			V.BbPlot.ybot += ttic_textheight;
		}
	}
	//  end of preliminary V.BbPlot.ybot calculation }}} 
	// Determine the size and position of the key box 
	if(key->visible) {
		// Count max_len key and number keys with len > 0 
		_Bry.max_ptitl_len = find_maxl_keys(pPlots, count, &_Bry.ptitl_cnt);
		DoKeyLayout(pTerm, key);
	}
	// Adjust range of dependent axes y and y2 
	if(AxS[FIRST_Y_AXIS].IsNonLinear())
		ExtendPrimaryTicRange(&AxS[FIRST_Y_AXIS]);
	if(AxS[SECOND_Y_AXIS].IsNonLinear())
		ExtendPrimaryTicRange(&AxS[SECOND_Y_AXIS]);
	setup_tics(&AxS[FIRST_Y_AXIS], 20);
	setup_tics(&AxS[SECOND_Y_AXIS], 20);
	// Adjust color axis limits if necessary. 
	if(is_plot_with_palette()) {
		AxisCheckedExtendEmptyRange(COLOR_AXIS, "All points of color axis undefined.");
		if(Gg.ColorBox.where != SMCOLOR_BOX_NO)
			setup_tics(&AxS[COLOR_AXIS], 20);
	}
	/*{{{  recompute V.BbPlot.xleft based on widths of ytics, ylabel etc
	   unless it has been explicitly set by lmargin */

	/* tic labels */
	shift_labels_to_border = FALSE;
	if(AxS[FIRST_Y_AXIS].ticmode & TICS_ON_AXIS) {
		// FIXME: This test for how close the axis is to the border does not match 
		//        the tests in axis_output_tics(), and assumes FIRST_X_AXIS.       
		if(!AxS[FIRST_X_AXIS].InRange(0.0))
			shift_labels_to_border = TRUE;
		if(0.1 > fabs(AxS[FIRST_X_AXIS].min / (AxS[FIRST_X_AXIS].max - AxS[FIRST_X_AXIS].min)))
			shift_labels_to_border = TRUE;
	}
	if((AxS[FIRST_Y_AXIS].ticmode & TICS_ON_BORDER) || shift_labels_to_border) {
		if(vertical_ytics)
			// HBB: we will later add some white space as part of this, so
			// reserve two more rows (one above, one below the text ...).
			// Same will be done to similar calc.'s elsewhere 
			ytic_textwidth = (int)(pTerm->ChrV * (yticlin + 2));
		else {
			widest_tic_strlen = 0; /* reset the global variable ... */
			/* get gen_tics to call widest_tic_callback with all labels
			 * the latter sets widest_tic_strlen to the length of the widest
			 * one ought to consider tics on axis if axis near border...
			 */
			GenTics(pTerm, &AxS[FIRST_Y_AXIS], &GnuPlot::WidestTicCallback);
			ytic_textwidth = (int)(pTerm->ChrH * (widest_tic_strlen + 2));
		}
	}
	else if(AxS[FIRST_Y_AXIS].label.text) {
		// substitutes for extra space added to left of ytix labels 
		ytic_textwidth = 2 * pTerm->ChrH;
	}
	else {
		ytic_textwidth = 0;
	}
	// tics 
	if(!AxS[FIRST_Y_AXIS].TicIn && ((AxS[FIRST_Y_AXIS].ticmode & TICS_ON_BORDER) || ((AxS[SECOND_Y_AXIS].ticmode & TICS_MIRROR) && (AxS[SECOND_Y_AXIS].ticmode & TICS_ON_BORDER))))
		ytic_width = static_cast<int>(pTerm->TicH * AxS[FIRST_Y_AXIS].ticscale);
	else
		ytic_width = 0;
	if(V.MarginL.x < 0) {
		// Auto-calculation 
		int space_to_left = _Bry.key_xleft;
		if(space_to_left < timelabel_textwidth && vertical_timelabel)
			space_to_left = timelabel_textwidth;
		SETMAX(space_to_left, ylabel_textwidth);
		V.BbPlot.xleft = static_cast<int>(V.Offset.X * pTerm->MaxX);
		V.BbPlot.xleft += space_to_left;
		V.BbPlot.xleft += ytic_width + ytic_textwidth;
		if(V.BbPlot.xleft - ytic_width - ytic_textwidth < 0)
			V.BbPlot.xleft = ytic_width + ytic_textwidth;
		if(V.BbPlot.xleft == pTerm->MaxX * V.Offset.X)
			V.BbPlot.xleft += pTerm->ChrH * 2;
		// DBT 12-3-98  extra margin just in case 
		V.BbPlot.xleft += 0.5 * pTerm->ChrH;
	}
	/* Note: we took care of explicit 'set lmargin foo' at line 492 */
	/*  end of V.BbPlot.xleft calculation }}} */

	/*{{{  recompute V.BbPlot.xright based on widest y2tic. y2labels, key "outside"
	   unless it has been explicitly set by rmargin */
	// tic labels 
	if(AxS[SECOND_Y_AXIS].ticmode & TICS_ON_BORDER) {
		if(vertical_y2tics)
			y2tic_textwidth = (int)(pTerm->ChrV * (y2ticlin + 2));
		else {
			widest_tic_strlen = 0; /* reset the global variable ... */
			// get gen_tics to call widest_tic_callback with all labels
			// the latter sets widest_tic_strlen to the length of the widest
			// one ought to consider tics on axis if axis near border...
			GenTics(pTerm, &AxS[SECOND_Y_AXIS], &GnuPlot::WidestTicCallback);
			y2tic_textwidth = (int)(pTerm->ChrH * (widest_tic_strlen + 2));
		}
	}
	else {
		y2tic_textwidth = 0;
	}
	// EAM May 2009
	// Check to see if any xtic labels are so long that they extend beyond
	// the right boundary of the plot. If so, allow extra room in the margin.
	// If the labels are too long to fit even with a big margin, too bad.
	if(AxS[FIRST_X_AXIS].ticdef.def.user) {
		ticmark * tic = AxS[FIRST_X_AXIS].ticdef.def.user;
		int maxrightlabel = V.BbPlot.xright;
		// We don't really know the plot layout yet, but try for an estimate 
		axis_set_scale_and_range(&AxS[FIRST_X_AXIS], V.BbPlot.xleft, V.BbPlot.xright);
		while(tic) {
			if(tic->label) {
				double xx;
				int length = static_cast<int>(estimate_strlen(tic->label, NULL) * cos(DEG2RAD * (double)(AxS[FIRST_X_AXIS].tic_rotate)) * pTerm->ChrH);
				if(inrange(tic->position, AxS[FIRST_X_AXIS].set_min, AxS[FIRST_X_AXIS].set_max)) {
					xx = AxisLogValueChecked(FIRST_X_AXIS, tic->position, "xtic");
					xx = AxS.MapiX(xx);
					xx += (AxS[FIRST_X_AXIS].tic_rotate) ? length : length /2;
					if(maxrightlabel < xx)
						maxrightlabel = static_cast<int>(xx);
				}
			}
			tic = tic->next;
		}
		xtic_textwidth = maxrightlabel - V.BbPlot.xright;
		if(xtic_textwidth > pTerm->MaxX/4) {
			xtic_textwidth = pTerm->MaxX/4;
			IntWarn(NO_CARET, "difficulty making room for xtic labels");
		}
	}
	// tics 
	if(!AxS[SECOND_Y_AXIS].TicIn && ((AxS[SECOND_Y_AXIS].ticmode & TICS_ON_BORDER) || ((AxS[FIRST_Y_AXIS].ticmode & TICS_MIRROR) && (AxS[FIRST_Y_AXIS].ticmode & TICS_ON_BORDER))))
		y2tic_width = (int)(pTerm->TicH * AxS[SECOND_Y_AXIS].ticscale);
	else
		y2tic_width = 0;
	// Make room for the color box if needed. 
	if(V.MarginR.scalex != screen) {
		if(is_plot_with_colorbox()) {
#define COLORBOX_SCALE 0.100
#define WIDEST_COLORBOX_TICTEXT 3
			if((Gg.ColorBox.where != SMCOLOR_BOX_NO) && (Gg.ColorBox.where != SMCOLOR_BOX_USER)) {
				V.BbPlot.xright -= (int)(V.BbPlot.xright-V.BbPlot.xleft)*COLORBOX_SCALE;
				V.BbPlot.xright -= (int)((pTerm->ChrH) * WIDEST_COLORBOX_TICTEXT);
			}
			Gg.ColorBox.xoffset = 0;
		}
		if(V.MarginR.x < 0) {
			Gg.ColorBox.xoffset = V.BbPlot.xright;
			V.BbPlot.xright -= y2tic_width + y2tic_textwidth;
			if(y2label_textwidth > 0)
				V.BbPlot.xright -= y2label_textwidth;
			if(V.BbPlot.xright > (V.Size.x+V.Offset.X)*(pTerm->MaxX-1) - (pTerm->ChrH * 2))
				V.BbPlot.xright = (V.Size.x+V.Offset.X)*(pTerm->MaxX-1) - (pTerm->ChrH * 2);
			Gg.ColorBox.xoffset -= V.BbPlot.xright;
			// EAM 2009 - protruding xtic labels 
			if((static_cast<int>(pTerm->MaxX) - V.BbPlot.xright) < xtic_textwidth)
				V.BbPlot.xright = pTerm->MaxX - xtic_textwidth;
			// DBT 12-3-98  extra margin just in case 
			V.BbPlot.xright -= 1.0 * pTerm->ChrH;
		}
		// Note: we took care of explicit 'set rmargin foo' at line 502 
	}
	//  end of V.BbPlot.xright calculation }}} 
	//
	// Set up x and x2 tics 
	// we should base the guide on the width of the xtics, but we cannot
	// use widest_tics until tics are set up. Bit of a downer - let us
	// assume tics are 5 characters wide
	//
	setup_tics(&AxS[FIRST_X_AXIS], 20);
	setup_tics(&AxS[SECOND_X_AXIS], 20);
	// Make sure that if polar grid is shown on a cartesian axis plot
	// the rtics match up with the primary x tics.                    
	if(AxS.__R().ticmode && (Gg.Polar || raxis)) {
		if(AxS.__R().BadRange() || (!Gg.Polar && AxS.__R().min != 0)) {
			SetExplicitRange(&AxS.__R(), 0.0, AxS.__X().max);
			AxS.__R().min = 0;
			AxS.__R().max = AxS[FIRST_X_AXIS].max;
			IntWarn(NO_CARET, "resetting rrange");
		}
		setup_tics(&AxS[POLAR_AXIS], 10);
	}
	// Modify the bounding box to fit the aspect ratio, if any was given 
	if(V.AspectRatio != 0.0f) {
		double current_aspect_ratio;
		double current, required;
		if(V.AspectRatio < 0.0f && (AxS.__X().max - AxS.__X().min) != 0.0) {
			current_aspect_ratio = -V.AspectRatio * fabs((AxS.__Y().max - AxS.__Y().min) / (AxS.__X().max - AxS.__X().min));
		}
		else
			current_aspect_ratio = V.AspectRatio;
		if(current_aspect_ratio < 0.005 || current_aspect_ratio > 2000.0)
			IntWarn(NO_CARET, "extreme aspect ratio");
		current = ((double)(V.BbPlot.ytop - V.BbPlot.ybot)) / ((double)(V.BbPlot.xright - V.BbPlot.xleft));
		required = (current_aspect_ratio * pTerm->TicV) / pTerm->TicH;
		// Fixed borders take precedence over centering 
		if(current > required) {
			// too tall 
			int old_height = V.BbPlot.ytop - V.BbPlot.ybot;
			int new_height = static_cast<int>(required * (V.BbPlot.xright - V.BbPlot.xleft));
			if(V.MarginB.scalex == screen)
				V.BbPlot.ytop = V.BbPlot.ybot + new_height;
			else if(V.MarginT.scalex == screen)
				V.BbPlot.ybot = V.BbPlot.ytop - new_height;
			else {
				V.BbPlot.ybot += (old_height - new_height) / 2;
				V.BbPlot.ytop -= (old_height - new_height) / 2;
			}
		}
		else {
			// too wide 
			int old_width = V.BbPlot.xright - V.BbPlot.xleft;
			int new_width = static_cast<int>((V.BbPlot.ytop - V.BbPlot.ybot) / required);
			if(V.MarginL.scalex == screen)
				V.BbPlot.xright = V.BbPlot.xleft + new_width;
			else if(V.MarginR.scalex == screen)
				V.BbPlot.xleft = V.BbPlot.xright - new_width;
			else {
				V.BbPlot.xleft += (old_width - new_width) / 2;
				V.BbPlot.xright -= (old_width - new_width) / 2;
			}
		}
	}
	// 
	// Calculate space needed for tic label rotation.
	// If [tb]margin is auto, move the plot boundary.
	// Otherwise use textheight to adjust placement of various titles.
	// 
	if(AxS[SECOND_X_AXIS].ticmode & TICS_ON_BORDER && vertical_x2tics) {
		/* Assuming left justified tic labels. Correction below if they aren't */
		double projection = sin((double)AxS[SECOND_X_AXIS].tic_rotate*DEG2RAD);
		if(AxS[SECOND_X_AXIS].tic_pos == RIGHT)
			projection *= -1;
		else if(AxS[SECOND_X_AXIS].tic_pos == CENTRE)
			projection = 0.5*fabs(projection);
		widest_tic_strlen = 0;  /* reset the global variable ... */
		GenTics(pTerm, &AxS[SECOND_X_AXIS], &GnuPlot::WidestTicCallback);
		if(V.MarginT.x < 0) /* Undo original estimate */
			V.BbPlot.ytop += x2tic_textheight;
		// Adjust spacing for rotation 
		if(projection > 0.0)
			x2tic_textheight += (int)(pTerm->ChrH * (widest_tic_strlen)) * projection;
		if(V.MarginT.x < 0)
			V.BbPlot.ytop -= x2tic_textheight;
	}
	if(AxS[FIRST_X_AXIS].ticmode & TICS_ON_BORDER && vertical_xtics) {
		double projection;
		// This adjustment will happen again in axis_output_tics but we need it now 
		if(AxS[FIRST_X_AXIS].tic_rotate == TEXT_VERTICAL && !AxS[FIRST_X_AXIS].manual_justify)
			AxS[FIRST_X_AXIS].tic_pos = RIGHT;
		if(AxS[FIRST_X_AXIS].tic_rotate == 90)
			projection = -1.0;
		else if(AxS[FIRST_X_AXIS].tic_rotate == TEXT_VERTICAL)
			projection = -1.0;
		else
			projection = -sin((double)AxS[FIRST_X_AXIS].tic_rotate*DEG2RAD);
		if(AxS[FIRST_X_AXIS].tic_pos == RIGHT)
			projection *= -1;
		widest_tic_strlen = 0;  /* reset the global variable ... */
		GenTics(pTerm, &AxS[FIRST_X_AXIS], &GnuPlot::WidestTicCallback);
		if(V.MarginB.x < 0)
			V.BbPlot.ybot -= xtic_textheight;
		if(projection > 0.0)
			xtic_textheight = static_cast<int>((int)(pTerm->ChrH * widest_tic_strlen) * projection + pTerm->ChrV);
		if(V.MarginB.x < 0)
			V.BbPlot.ybot += xtic_textheight;
	}
	// 
	// Notwithstanding all these fancy calculations,
	// V.BbPlot.ytop must always be above V.BbPlot.ybot
	// 
	if(V.BbPlot.ytop < V.BbPlot.ybot) {
		int i = V.BbPlot.ytop;
		V.BbPlot.ytop = V.BbPlot.ybot;
		V.BbPlot.ybot = i;
		FPRINTF((stderr, "boundary: Big problems! V.BbPlot.ybot > V.BbPlot.ytop\n"));
	}
	// compute coordinates for axis labels, title etc 
	x2label_y = V.BbPlot.ytop + x2label_textheight;
	x2label_y += 0.5 * pTerm->ChrV;
	if(x2label_textheight + x2label_yoffset >= 0) {
		x2label_y += 1.5 * x2tic_textheight;
		// Adjust for the tics themselves 
		if(x2tic_height > 0)
			x2label_y += x2tic_height;
	}
	title_x = (V.BbPlot.xleft + V.BbPlot.xright) / 2;
	// title_y was previously set to the actual title height.
	// Further corrections to this placement only if it is above the plot
	title_y += V.BbPlot.ytop;
	if(_Bry.titlelin + Gg.LblTitle.offset.y > 0) {
		title_y += x2tic_textheight;
		title_y += ttic_textheight;
		if(x2label_y + x2label_yoffset > V.BbPlot.ytop)
			title_y += x2label_textheight;
		if(x2tic_height > 0)
			title_y += x2tic_height;
	}
	// Shift upward by 0.2 line to allow for descenders in xlabel text 
	xlabel_y  = static_cast<int>(V.BbPlot.ybot - xtic_height - xtic_textheight - xlabel_textheight + (_Bry.xlablin+0.2f) * pTerm->ChrV);
	xlabel_y -= ttic_textheight;
	ylabel_x = V.BbPlot.xleft - ytic_width - ytic_textwidth;
	ylabel_x -= ylabel_textwidth/2;
	y2label_x = V.BbPlot.xright + y2tic_width + y2tic_textwidth;
	y2label_x += y2label_textwidth/2;
	// Nov 2016  - simplify placement of timestamp
	// Stamp the same place on the page regardless of plot margins
	if(vertical_timelabel) {
		_Bry.time_x = static_cast<int>(1.5f * pTerm->ChrH);
		if(Gg.TimeLabelBottom)
			_Bry.time_y = pTerm->ChrV;
		else
			_Bry.time_y = pTerm->MaxY - pTerm->ChrV;
	}
	else {
		_Bry.time_x = static_cast<int>(1.0f * pTerm->ChrH);
		if(Gg.TimeLabelBottom)
			_Bry.time_y = static_cast<int>(timelabel_textheight - 0.5f * pTerm->ChrV);
		else
			_Bry.time_y = pTerm->MaxY;
	}
	xtic_y = V.BbPlot.ybot - xtic_height - (int)(vertical_xtics ? pTerm->ChrH : pTerm->ChrV);
	x2tic_y = V.BbPlot.ytop + (x2tic_height > 0 ? x2tic_height : 0) + (vertical_x2tics ? (int)pTerm->ChrH : pTerm->ChrV);
	ytic_x = V.BbPlot.xleft - ytic_width - (vertical_ytics ? (ytic_textwidth - (int)pTerm->ChrV) : (int)pTerm->ChrH);
	y2tic_x = V.BbPlot.xright + y2tic_width + (int)(vertical_y2tics ? pTerm->ChrV : pTerm->ChrH);
	// restore text to horizontal [we tested rotation above] 
	(pTerm->text_angle)(0);
	// needed for map_position() below 
	axis_set_scale_and_range(&AxS[FIRST_X_AXIS], V.BbPlot.xleft, V.BbPlot.xright);
	axis_set_scale_and_range(&AxS[SECOND_X_AXIS], V.BbPlot.xleft, V.BbPlot.xright);
	axis_set_scale_and_range(&AxS[FIRST_Y_AXIS], V.BbPlot.ybot, V.BbPlot.ytop);
	axis_set_scale_and_range(&AxS[SECOND_Y_AXIS], V.BbPlot.ybot, V.BbPlot.ytop);
	DoKeyBounds(pTerm, key); // Calculate limiting bounds of the key 
	V.P_ClipArea = &V.BbPlot; // Set default clipping to the plot boundary 
	// Sanity checks 
	if(V.BbPlot.xright < V.BbPlot.xleft || V.BbPlot.ytop < V.BbPlot.ybot)
		IntWarn(NO_CARET, "Terminal canvas area too small to hold plot.\n\t    Check plot boundary and font sizes.");
}

/*}}} */

//void do_key_bounds(GpTermEntry * pTerm, legend_key * key)
void GnuPlot::DoKeyBounds(GpTermEntry * pTerm, legend_key * pKey)
{
	_Bry.key_height = static_cast<int>(_Bry.key_title_height + _Bry.key_title_extra + key_rows * key_entry_height + pKey->height_fix * key_entry_height);
	_Bry.key_width = _Bry.key_col_wth * key_cols;
	// Key inside plot boundaries 
	if(pKey->region == GPKEY_AUTO_INTERIOR_LRTBC || (pKey->region == GPKEY_AUTO_EXTERIOR_LRTBC && pKey->vpos == JUST_CENTRE && pKey->hpos == CENTRE)) {
		if(pKey->vpos == JUST_TOP) {
			pKey->bounds.ytop = V.BbPlot.ytop - pTerm->TicV;
			pKey->bounds.ybot = pKey->bounds.ytop - _Bry.key_height;
		}
		else if(pKey->vpos == JUST_BOT) {
			pKey->bounds.ybot = V.BbPlot.ybot + pTerm->TicV;
			pKey->bounds.ytop = pKey->bounds.ybot + _Bry.key_height;
		}
		else { // (key->vpos == JUST_CENTRE) 
			pKey->bounds.ybot = ((V.BbPlot.ybot + V.BbPlot.ytop) - _Bry.key_height) / 2;
			pKey->bounds.ytop = ((V.BbPlot.ybot + V.BbPlot.ytop) + _Bry.key_height) / 2;
		}
		if(pKey->hpos == LEFT) {
			pKey->bounds.xleft = V.BbPlot.xleft + pTerm->ChrH;
			pKey->bounds.xright = pKey->bounds.xleft + _Bry.key_width;
		}
		else if(pKey->hpos == RIGHT) {
			pKey->bounds.xright = V.BbPlot.xright - pTerm->ChrH;
			pKey->bounds.xleft = pKey->bounds.xright - _Bry.key_width;
		}
		else { // (key->hpos == CENTER) 
			pKey->bounds.xleft = ((V.BbPlot.xright + V.BbPlot.xleft) - _Bry.key_width) / 2;
			pKey->bounds.xright = ((V.BbPlot.xright + V.BbPlot.xleft) + _Bry.key_width) / 2;
		}
		// Key outside plot boundaries 
	}
	else if(pKey->region == GPKEY_AUTO_EXTERIOR_LRTBC || pKey->region == GPKEY_AUTO_EXTERIOR_MARGIN) {
		// Vertical alignment 
		if(pKey->margin == GPKEY_TMARGIN) {
			// align top first since tmargin may be manual 
			pKey->bounds.ytop = static_cast<int>((V.Size.y + V.Offset.Y) * pTerm->MaxY - pTerm->TicV);
			pKey->bounds.ybot = pKey->bounds.ytop - _Bry.key_height;
		}
		else if(pKey->margin == GPKEY_BMARGIN) {
			// align bottom first since bmargin may be manual 
			pKey->bounds.ybot = static_cast<int>(V.Offset.Y * pTerm->MaxY + pTerm->TicV);
			if(Gg.LblTime.rotate == 0 && Gg.TimeLabelBottom && Gg.LblTime.place.y > 0)
				pKey->bounds.ybot += (int)(Gg.LblTime.place.y);
			pKey->bounds.ytop = pKey->bounds.ybot + _Bry.key_height;
		}
		else {
			if(pKey->vpos == JUST_TOP) {
				// align top first since tmargin may be manual 
				pKey->bounds.ytop = V.BbPlot.ytop;
				pKey->bounds.ybot = pKey->bounds.ytop - _Bry.key_height;
			}
			else if(pKey->vpos == JUST_CENTRE) {
				pKey->bounds.ybot = ((V.BbPlot.ybot + V.BbPlot.ytop) - _Bry.key_height) / 2;
				pKey->bounds.ytop = ((V.BbPlot.ybot + V.BbPlot.ytop) + _Bry.key_height) / 2;
			}
			else {
				// align bottom first since bmargin may be manual 
				pKey->bounds.ybot = V.BbPlot.ybot;
				pKey->bounds.ytop = pKey->bounds.ybot + _Bry.key_height;
			}
		}
		// Horizontal alignment 
		if(pKey->margin == GPKEY_LMARGIN) {
			// align left first since lmargin may be manual 
			pKey->bounds.xleft = static_cast<int>(V.Offset.X * pTerm->MaxX + pTerm->ChrH);
			pKey->bounds.xright = pKey->bounds.xleft + _Bry.key_width;
		}
		else if(pKey->margin == GPKEY_RMARGIN) {
			// align right first since rmargin may be manual 
			pKey->bounds.xright = static_cast<int>((V.Size.x + V.Offset.X) * (pTerm->MaxX-1) - pTerm->ChrH);
			pKey->bounds.xleft = pKey->bounds.xright - _Bry.key_width;
		}
		else {
			if(pKey->hpos == LEFT) {
				// align left first since lmargin may be manual 
				pKey->bounds.xleft = V.BbPlot.xleft;
				pKey->bounds.xright = pKey->bounds.xleft + _Bry.key_width;
			}
			else if(pKey->hpos == CENTRE) {
				pKey->bounds.xleft  = ((V.BbPlot.xright + V.BbPlot.xleft) - _Bry.key_width) / 2;
				pKey->bounds.xright = ((V.BbPlot.xright + V.BbPlot.xleft) + _Bry.key_width) / 2;
			}
			else {
				// align right first since rmargin may be manual 
				pKey->bounds.xright = V.BbPlot.xright;
				pKey->bounds.xleft = pKey->bounds.xright - _Bry.key_width;
			}
		}
		// Key at explicit position specified by user 
	}
	else {
		int x, y;
		// FIXME!!!
		// pm 22.1.2002: if key->user_pos.scalex or scaley == first_axes or second_axes,
		// then the graph scaling is not yet known and the box is positioned incorrectly;
		// you must do "replot" to avoid the wrong plot ... bad luck if output does not
		// go to screen
		MapPosition(pTerm, &pKey->user_pos, &x, &y, "key");
		// Here top, bottom, left, right refer to the alignment with respect to point. 
		pKey->bounds.xleft = x;
		if(pKey->hpos == CENTRE)
			pKey->bounds.xleft -= _Bry.key_width/2;
		else if(pKey->hpos == RIGHT)
			pKey->bounds.xleft -= _Bry.key_width;
		pKey->bounds.xright = pKey->bounds.xleft + _Bry.key_width;
		pKey->bounds.ytop = y;
		if(pKey->vpos == JUST_CENTRE)
			pKey->bounds.ytop += _Bry.key_height/2;
		else if(pKey->vpos == JUST_BOT)
			pKey->bounds.ytop += _Bry.key_height;
		pKey->bounds.ybot = pKey->bounds.ytop - _Bry.key_height;
	}
}
//
// Calculate positioning of components that make up the key box 
//
//void do_key_layout(GpTermEntry * pTerm, legend_key * key)
void GnuPlot::DoKeyLayout(GpTermEntry * pTerm, legend_key * pKey)
{
	bool key_panic = FALSE;
	// If there is a separate font for the key, use it for space calculations.	
	if(pKey->font)
		pTerm->set_font(pTerm, pKey->font);
	// Is it OK to initialize these here rather than in do_plot? 
	key_count = 0;
	_Bry.key_xleft = 0;
	_Bry.xl = _Bry.yl = 0;
	_Bry.key_sample_width = static_cast<int>((pKey->swidth >= 0) ? (pKey->swidth * pTerm->ChrH + pTerm->TicH) : 0.0);
	_Bry.key_sample_height = static_cast<int>(MAX(1.25 * pTerm->TicV, pTerm->ChrV));
	key_entry_height = static_cast<int>(_Bry.key_sample_height * pKey->vert_factor);
	// HBB 20020122: safeguard to prevent division by zero later 
	SETIFZ(key_entry_height, 1);
	// Key title length and height, adjusted for font size and markup 
	_Bry.key_title_height = 0;
	_Bry.key_title_extra = 0;
	_Bry.key_title_ypos = 0;
	if(pKey->title.text) {
		double est_height;
		int est_lines;
		if(pKey->title.font)
			pTerm->set_font(pTerm, pKey->title.font);
		label_width(pKey->title.text, &est_lines);
		estimate_strlen(pKey->title.text, &est_height);
		_Bry.key_title_height = static_cast<int>(est_height * pTerm->ChrV);
		_Bry.key_title_ypos = (_Bry.key_title_height/2);
		if(pKey->title.font)
			pTerm->set_font(pTerm, "");
		// FIXME: empirical tweak. I don't know why this is needed 
		_Bry.key_title_ypos -= (est_lines-1) * pTerm->ChrV/2;
	}
	if(pKey->reverse) {
		_Bry.key_sample_left = -_Bry.key_sample_width;
		_Bry.key_sample_right = 0;
		// if key width is being used, adjust right-justified text 
		_Bry.key_text_left  = pTerm->ChrH;
		_Bry.key_text_right = static_cast<int>(pTerm->ChrH * (_Bry.max_ptitl_len + 1 + pKey->width_fix));
		_Bry.key_size_left  = pTerm->ChrH - _Bry.key_sample_left; // sample left is -ve 
		_Bry.key_size_right = _Bry.key_text_right;
	}
	else {
		_Bry.key_sample_left = 0;
		_Bry.key_sample_right = _Bry.key_sample_width;
		// if key width is being used, adjust left-justified text 
		_Bry.key_text_left = -(int)(pTerm->ChrH * (_Bry.max_ptitl_len + 1 + pKey->width_fix));
		_Bry.key_text_right = -(int)pTerm->ChrH;
		_Bry.key_size_left = -_Bry.key_text_left;
		_Bry.key_size_right = _Bry.key_sample_right + pTerm->ChrH;
	}
	key_point_offset = (_Bry.key_sample_left + _Bry.key_sample_right) / 2;
	// advance width for cols 
	_Bry.key_col_wth = _Bry.key_size_left + _Bry.key_size_right;
	key_rows = _Bry.ptitl_cnt;
	key_cols = 1;
	// calculate rows and cols for key 
	if(pKey->stack_dir == GPKEY_HORIZONTAL) {
		// maximise no cols, limited by label-length 
		key_cols = (int)(V.BbPlot.xright - V.BbPlot.xleft) / _Bry.key_col_wth;
		if(pKey->maxcols > 0 && key_cols > pKey->maxcols)
			key_cols = pKey->maxcols;
		// EAM Dec 2004 - Rather than turn off the key, try to squeeze 
		if(key_cols == 0) {
			key_cols = 1;
			key_panic = TRUE;
			_Bry.key_col_wth = (V.BbPlot.xright - V.BbPlot.xleft);
		}
		key_rows = (_Bry.ptitl_cnt + key_cols - 1) / key_cols;
		// now calculate actual no cols depending on no rows 
		key_cols = (key_rows == 0) ? 1 : (_Bry.ptitl_cnt + key_rows - 1) / key_rows;
		SETIFZ(key_cols, 1);
	}
	else {
		// maximise no rows, limited by V.BbPlot.ytop-V.BbPlot.ybot 
		int i = static_cast<int>((V.BbPlot.ytop - V.BbPlot.ybot - pKey->height_fix * key_entry_height - _Bry.key_title_height - _Bry.key_title_extra) / key_entry_height);
		if(pKey->maxrows > 0 && i > pKey->maxrows)
			i = pKey->maxrows;
		if(i == 0) {
			i = 1;
			key_panic = TRUE;
		}
		if(_Bry.ptitl_cnt > i) {
			key_cols = (_Bry.ptitl_cnt + i - 1) / i;
			// now calculate actual no rows depending on no cols 
			if(key_cols == 0) {
				key_cols = 1;
				key_panic = TRUE;
			}
			key_rows = (_Bry.ptitl_cnt + key_cols - 1) / key_cols;
		}
	}
	// If the key title is wider than the contents, try to make room for it 
	if(pKey->title.text) {
		int ytlen = static_cast<int>(label_width(pKey->title.text, NULL) - pKey->swidth + 2);
		if(pKey->title.font)
			pTerm->set_font(pTerm, pKey->title.font);
		ytlen *= pTerm->ChrH;
		if(ytlen > key_cols * _Bry.key_col_wth)
			_Bry.key_col_wth = ytlen / key_cols;
		if(pKey->title.font)
			pTerm->set_font(pTerm, "");
	}
	// Adjust for outside key, leave manually set margins alone 
	if((pKey->region == GPKEY_AUTO_EXTERIOR_LRTBC && (pKey->vpos != JUST_CENTRE || pKey->hpos != CENTRE)) || pKey->region == GPKEY_AUTO_EXTERIOR_MARGIN) {
		int more = 0;
		if(pKey->margin == GPKEY_BMARGIN && V.MarginB.x < 0) {
			more = static_cast<int>(key_rows * key_entry_height + _Bry.key_title_height + _Bry.key_title_extra + pKey->height_fix * key_entry_height);
			if(V.BbPlot.ybot + more > V.BbPlot.ytop)
				key_panic = TRUE;
			else
				V.BbPlot.ybot += more;
		}
		else if(pKey->margin == GPKEY_TMARGIN && V.MarginT.x < 0) {
			more = static_cast<int>(key_rows * key_entry_height + _Bry.key_title_height + _Bry.key_title_extra + pKey->height_fix * key_entry_height);
			if(V.BbPlot.ytop - more < V.BbPlot.ybot)
				key_panic = TRUE;
			else
				V.BbPlot.ytop -= more;
		}
		else if(pKey->margin == GPKEY_LMARGIN && V.MarginL.x < 0) {
			more = _Bry.key_col_wth * key_cols;
			if(V.BbPlot.xleft + more > V.BbPlot.xright)
				key_panic = TRUE;
			else
				_Bry.key_xleft = more;
			V.BbPlot.xleft += _Bry.key_xleft;
		}
		else if(pKey->margin == GPKEY_RMARGIN && V.MarginR.x < 0) {
			more = _Bry.key_col_wth * key_cols;
			if(V.BbPlot.xright - more < V.BbPlot.xleft)
				key_panic = TRUE;
			else
				V.BbPlot.xright -= more;
		}
	}
	// Restore default font 
	if(pKey->font)
		pTerm->set_font(pTerm, "");
	// warn if we had to punt on key size calculations 
	if(key_panic)
		IntWarn(NO_CARET, "Warning - difficulty fitting plot titles into key");
}

int find_maxl_keys(const curve_points * pPlots, int count, int * kcnt)
{
	int mlen = 0;
	int len;
	int curve;
	int cnt = 0;
	int previous_plot_style = 0;
	const curve_points * p_plot = pPlots;
	for(curve = 0; curve < count; p_plot = p_plot->next, curve++) {
		if(p_plot->plot_style == PARALLELPLOT)
			continue;
		if(p_plot->title && !p_plot->title_is_suppressed &&  !p_plot->title_position) {
			if(p_plot->plot_style == SPIDERPLOT && p_plot->plot_type != KEYENTRY)
				; /* Nothing */
			else {
				ignore_enhanced(p_plot->title_no_enhanced);
				len = estimate_strlen(p_plot->title, NULL);
				if(len != 0) {
					cnt++;
					if(len > mlen)
						mlen = len;
				}
				ignore_enhanced(FALSE);
			}
		}
		/* Check for new histogram here and save space for divider */
		if(p_plot->plot_style == HISTOGRAMS &&  previous_plot_style == HISTOGRAMS && p_plot->histogram_sequence == 0 && cnt > 1)
			cnt++;
		// Check for column-stacked histogram with key entries.
		// Same thing for spiderplots.
		// This is needed for 'plot ... using col:key(1)'
		if(p_plot->labels && (p_plot->plot_style == HISTOGRAMS || p_plot->plot_style == SPIDERPLOT)) {
			text_label * key_entry = p_plot->labels->next;
			for(; key_entry; key_entry = key_entry->next) {
				cnt++;
				len = key_entry->text ? estimate_strlen(key_entry->text, NULL) : 0;
				if(len > mlen)
					mlen = len;
			}
		}
		previous_plot_style = p_plot->plot_style;
	}
	ASSIGN_PTR(kcnt, cnt);
	return (mlen);
}
// 
// Make the key sample code a subroutine so that it can eventually be
// shared by the 3d code also. As of now the two code sections are not very parallel.  EAM Nov 2003
//
//void do_key_sample(GpTermEntry * pTerm, const curve_points * pPlot, legend_key * pKey, char * title, coordval var_color)
void GnuPlot::DoKeySample(GpTermEntry * pTerm, const curve_points * pPlot, legend_key * pKey, char * title, coordval var_color)
{
	int xl_save = _Bry.xl;
	int yl_save = _Bry.yl;
	// Clip key box against canvas 
	BoundingBox * clip_save = V.P_ClipArea;
	V.P_ClipArea = (term->flags & TERM_CAN_CLIP) ? NULL : &V.BbCanvas;
	// If the plot this title belongs to specified a non-standard place 
	// for the key sample to appear, use that to override xl, yl.       
	if(pPlot->title_position && pPlot->title_position->scalex != character) {
		MapPosition(pTerm, pPlot->title_position, &_Bry.xl, &_Bry.yl, "key sample");
		_Bry.xl -=  (pKey->just == GPKEY_LEFT) ? _Bry.key_text_left : _Bry.key_text_right;
	}
	(pTerm->layer)(pTerm, TERM_LAYER_BEGIN_KEYSAMPLE);
	if(pKey->textcolor.type == TC_VARIABLE)
		; /* Draw key text in same color as plot */
	else if(pKey->textcolor.type != TC_DEFAULT)
		ApplyPm3DColor(pTerm, &pKey->textcolor); /* Draw key text in same color as key title */
	else
		(pTerm->linetype)(pTerm, LT_BLACK); // Draw key text in black 
	if(pPlot->title_is_automated && (pTerm->flags & TERM_IS_LATEX)) {
		title = texify_title(title, pPlot->plot_type);
	}
	if(pKey->just == GPKEY_LEFT) {
		write_multiline(pTerm, _Bry.xl + _Bry.key_text_left, _Bry.yl, title, LEFT, JUST_CENTRE, 0, pKey->font);
	}
	else {
		if((pTerm->justify_text)(RIGHT)) {
			write_multiline(pTerm, _Bry.xl + _Bry.key_text_right, _Bry.yl, title, RIGHT, JUST_CENTRE, 0, pKey->font);
		}
		else {
			int x = _Bry.xl + _Bry.key_text_right - pTerm->ChrH * estimate_strlen(title, NULL);
			if(oneof2(pKey->region, GPKEY_AUTO_EXTERIOR_LRTBC, GPKEY_AUTO_EXTERIOR_MARGIN) || inrange((x), (V.BbPlot.xleft), (V.BbPlot.xright)))
				write_multiline(pTerm, x, _Bry.yl, title, LEFT, JUST_CENTRE, 0, pKey->font);
		}
	}
	// Draw sample in same style and color as the corresponding plot  
	// The variable color case uses the color of the first data point 
	if(!CheckForVariableColor(pTerm, pPlot, &var_color))
		TermApplyLpProperties(pTerm, &pPlot->lp_properties);
	// draw sample depending on bits set in plot_style 
	if(pPlot->plot_style & PLOT_STYLE_HAS_FILL && pTerm->fillbox) {
		const fill_style_type * fs = &pPlot->fill_properties;
		int style = style_from_fill(fs);
		int x = _Bry.xl + _Bry.key_sample_left;
		int y = _Bry.yl - _Bry.key_sample_height/4;
		int w = _Bry.key_sample_right - _Bry.key_sample_left;
		int h = _Bry.key_sample_height/2;
		if(pPlot->plot_style == CIRCLES && w > 0) {
			DoArc(pTerm, _Bry.xl + key_point_offset, _Bry.yl, _Bry.key_sample_height/4, 0.0, 360.0, style, FALSE);
			// Retrace the border if the style requests it 
			if(NeedFillBorder(pTerm, fs))
				DoArc(pTerm, _Bry.xl + key_point_offset, _Bry.yl, _Bry.key_sample_height/4, 0.0, 360.0, 0, FALSE);
		}
		else if(pPlot->plot_style == ELLIPSES && w > 0) {
			t_ellipse * key_ellipse = (t_ellipse *)gp_alloc(sizeof(t_ellipse), "cute little ellipse for the key sample");
			key_ellipse->center.x = _Bry.xl + key_point_offset;
			key_ellipse->center.y = _Bry.yl;
			key_ellipse->extent.x = w * 2/3;
			key_ellipse->extent.y = h;
			key_ellipse->orientation = 0.0;
			// already in term coords, no need to map 
			DoEllipse(pTerm, 2, key_ellipse, style, FALSE);
			// Retrace the border if the style requests it 
			if(NeedFillBorder(pTerm, fs)) {
				DoEllipse(pTerm, 2, key_ellipse, 0, FALSE);
			}
			SAlloc::F(key_ellipse);
		}
		else if(w > 0) { /* All other plot types with fill */
			if(style != FS_EMPTY)
				(pTerm->fillbox)(pTerm, style, x, y, w, h);
			// need_fill_border will set the border linetype, but candlesticks don't want it 
			if((pPlot->plot_style == CANDLESTICKS && fs->border_color.type == TC_LT && fs->border_color.lt == LT_NODRAW) || style == FS_EMPTY || NeedFillBorder(pTerm, fs)) {
				newpath(pTerm);
				DrawClipLine(pTerm, _Bry.xl + _Bry.key_sample_left,  _Bry.yl - _Bry.key_sample_height/4, _Bry.xl + _Bry.key_sample_right, _Bry.yl - _Bry.key_sample_height/4);
				DrawClipLine(pTerm, _Bry.xl + _Bry.key_sample_right, _Bry.yl - _Bry.key_sample_height/4, _Bry.xl + _Bry.key_sample_right, _Bry.yl + _Bry.key_sample_height/4);
				DrawClipLine(pTerm, _Bry.xl + _Bry.key_sample_right, _Bry.yl + _Bry.key_sample_height/4, _Bry.xl + _Bry.key_sample_left,  _Bry.yl + _Bry.key_sample_height/4);
				DrawClipLine(pTerm, _Bry.xl + _Bry.key_sample_left,  _Bry.yl + _Bry.key_sample_height/4, _Bry.xl + _Bry.key_sample_left,  _Bry.yl - _Bry.key_sample_height/4);
				closepath(pTerm);
			}
			if(fs->fillstyle != FS_EMPTY && fs->fillstyle != FS_DEFAULT && !(fs->border_color.type == TC_LT && fs->border_color.lt == LT_NODRAW)) {
				// need_fill_border() might have changed our original linetype 
				TermApplyLpProperties(pTerm, &pPlot->lp_properties);
			}
		}
	}
	else if((pPlot->plot_style & PLOT_STYLE_HAS_VECTOR) && pTerm->arrow) {
		double x1 = _Bry.xl + _Bry.key_sample_left;
		double y1 = _Bry.yl;
		double x2 = _Bry.xl + _Bry.key_sample_right;
		double y2 = _Bry.yl;
		apply_head_properties(&(pPlot->arrow_properties));
		DrawClipArrow(pTerm, x1, y1, x2, y2, pPlot->arrow_properties.head);
	}
	else if(pPlot->lp_properties.l_type == LT_NODRAW) {
		;
	}
	else if((pPlot->plot_style & PLOT_STYLE_HAS_ERRORBAR) && pPlot->plot_type != FUNC) {
		// errors for data plots only 
		if((bar_lp.flags & LP_ERRORBAR_SET) != 0)
			TermApplyLpProperties(pTerm, &bar_lp);
		DrawClipLine(pTerm, _Bry.xl + _Bry.key_sample_left, _Bry.yl, _Bry.xl + _Bry.key_sample_right, _Bry.yl);
		// Even if error bars are dotted, the end lines are always solid 
		if((bar_lp.flags & LP_ERRORBAR_SET) != 0)
			pTerm->dashtype(pTerm, DASHTYPE_SOLID, NULL);
	}
	else if((pPlot->plot_style & PLOT_STYLE_HAS_LINE)) {
		DrawClipLine(pTerm, _Bry.xl + _Bry.key_sample_left, _Bry.yl, _Bry.xl + _Bry.key_sample_right, _Bry.yl);
	}
	if((pPlot->plot_type == DATA || pPlot->plot_type == KEYENTRY) && (pPlot->plot_style & PLOT_STYLE_HAS_ERRORBAR) && (pPlot->plot_style != CANDLESTICKS) && (bar_size > 0.0)) {
		const uint error_bar_tic = ERRORBARTIC(pTerm);
		DrawClipLine(pTerm, _Bry.xl + _Bry.key_sample_left,  _Bry.yl + error_bar_tic, _Bry.xl + _Bry.key_sample_left, _Bry.yl - error_bar_tic);
		DrawClipLine(pTerm, _Bry.xl + _Bry.key_sample_right, _Bry.yl + error_bar_tic, _Bry.xl + _Bry.key_sample_right, _Bry.yl - error_bar_tic);
	}
	/* oops - doing the point sample now would break the postscript
	 * terminal for example, which changes current line style
	 * when drawing a point, but does not restore it. We must wait to
	 * draw the point sample at the end of do_plot (comment KEY SAMPLES).
	 */
	(pTerm->layer)(pTerm, TERM_LAYER_END_KEYSAMPLE);
	// Restore original linetype for the main plot if we changed it 
	if(pPlot->plot_type != FUNC && (pPlot->plot_style & PLOT_STYLE_HAS_ERRORBAR) && (bar_lp.flags & LP_ERRORBAR_SET) != 0) {
		TermApplyLpProperties(pTerm, &pPlot->lp_properties);
	}
	// Restore previous clipping area 
	V.P_ClipArea = clip_save;
	_Bry.xl = xl_save;
	_Bry.yl = yl_save;
}

//void do_key_sample_point(curve_points * this_plot, legend_key * key)
void GnuPlot::DoKeySamplePoint(GpTermEntry * pTerm, curve_points * pPlot, legend_key * pKey)
{
	int xl_save = _Bry.xl;
	int yl_save = _Bry.yl;
	// If the plot this title belongs to specified a non-standard place
	// for the key sample to appear, use that to override xl, yl.
	// For "at end|beg" do nothing at all.
	if(pPlot->title_position) {
		if(pPlot->title_position->scalex == character)
			return;
		MapPosition(pTerm, pPlot->title_position, &_Bry.xl, &_Bry.yl, "key sample");
		_Bry.xl -=  (pKey->just == GPKEY_LEFT) ? _Bry.key_text_left : _Bry.key_text_right;
	}
	(pTerm->layer)(pTerm, TERM_LAYER_BEGIN_KEYSAMPLE);
	if(pPlot->plot_style == LINESPOINTS &&  pPlot->lp_properties.p_interval < 0) {
		t_colorspec background_fill = BACKGROUND_COLORSPEC;
		(pTerm->set_color)(pTerm, &background_fill);
		(pTerm->pointsize)(Gg.PointSize * Gg.PointIntervalBox);
		(pTerm->point)(pTerm, _Bry.xl + key_point_offset, _Bry.yl, 6);
		TermApplyLpProperties(pTerm, &pPlot->lp_properties);
	}
	if(pPlot->plot_style == BOXPLOT) {
		; // Don't draw a sample point in the key 
	}
	else if(pPlot->plot_style == DOTS) {
		if(on_page(pTerm, _Bry.xl + key_point_offset, _Bry.yl))
			(pTerm->point)(pTerm, _Bry.xl + key_point_offset, _Bry.yl, -1);
	}
	else if(pPlot->plot_style & PLOT_STYLE_HAS_POINT) {
		if(pPlot->lp_properties.PtSize == PTSZ_VARIABLE)
			(pTerm->pointsize)(Gg.PointSize);
		if(on_page(pTerm, _Bry.xl + key_point_offset, _Bry.yl)) {
			if(pPlot->lp_properties.PtType == PT_CHARACTER) {
				if(pPlot->labels->textcolor.type != TC_DEFAULT)
					ApplyPm3DColor(pTerm, &(pPlot->labels->textcolor));
				(pTerm->put_text)(pTerm, _Bry.xl + key_point_offset, _Bry.yl, pPlot->lp_properties.p_char);
				ApplyPm3DColor(pTerm, &(pPlot->lp_properties.pm3d_color));
			}
			else {
				(pTerm->point)(pTerm, _Bry.xl + key_point_offset, _Bry.yl, pPlot->lp_properties.PtType);
			}
		}
	}
	else if(pPlot->plot_style == LABELPOINTS) {
		text_label * label = pPlot->labels;
		if(label->lp_properties.flags & LP_SHOW_POINTS) {
			TermApplyLpProperties(pTerm, &label->lp_properties);
			(pTerm->point)(pTerm, _Bry.xl + key_point_offset, _Bry.yl, label->lp_properties.PtType);
		}
	}
	_Bry.xl = xl_save;
	_Bry.yl = yl_save;
	(pTerm->layer)(pTerm, TERM_LAYER_END_KEYSAMPLE);
}
// 
// Graph legend is now optionally done in two passes. The first pass calculates
// and reserves the necessary space.  Next the individual plots in the graph
// are drawn. Then the reserved space for the legend is blanked out, and 
// finally the second pass through this code draws the legend.			
// 
//void draw_key(legend_key * key, bool key_pass)
void GnuPlot::DrawKey(GpTermEntry * pTerm, legend_key * pKey, bool keyPass)
{
	(pTerm->layer)(pTerm, TERM_LAYER_KEYBOX);
	// In two-pass mode (set key opaque) we blank out the key box after	
	// the graph is drawn and then redo the key in the blank area.	
	if(keyPass && pTerm->fillbox && !(pTerm->flags & TERM_NULL_SET_COLOR)) {
		(pTerm->set_color)(pTerm, &pKey->fillcolor);
		(pTerm->fillbox)(pTerm, FS_OPAQUE, pKey->bounds.xleft, pKey->bounds.ybot, _Bry.key_width, _Bry.key_height);
	}
	if(pKey->title.text) {
		int title_anchor;
		if(pKey->title.pos == CENTRE)
			title_anchor = (pKey->bounds.xleft + pKey->bounds.xright) / 2;
		else if(pKey->title.pos == RIGHT)
			title_anchor = pKey->bounds.xright - pTerm->ChrH;
		else
			title_anchor = pKey->bounds.xleft + pTerm->ChrH;
		// Only draw the title once 
		if(keyPass || !pKey->front) {
			WriteLabel(pTerm, title_anchor, pKey->bounds.ytop - _Bry.key_title_ypos, &pKey->title);
			(pTerm->linetype)(pTerm, LT_BLACK);
		}
	}
	if(pKey->box.l_type > LT_NODRAW) {
		BoundingBox * clip_save = V.P_ClipArea;
		V.P_ClipArea = (pTerm->flags & TERM_CAN_CLIP) ? NULL : &V.BbCanvas;
		TermApplyLpProperties(pTerm, &pKey->box);
		newpath(pTerm);
		DrawClipLine(pTerm, pKey->bounds.xleft, pKey->bounds.ybot, pKey->bounds.xleft, pKey->bounds.ytop);
		DrawClipLine(pTerm, pKey->bounds.xleft, pKey->bounds.ytop, pKey->bounds.xright, pKey->bounds.ytop);
		DrawClipLine(pTerm, pKey->bounds.xright, pKey->bounds.ytop, pKey->bounds.xright, pKey->bounds.ybot);
		DrawClipLine(pTerm, pKey->bounds.xright, pKey->bounds.ybot, pKey->bounds.xleft, pKey->bounds.ybot);
		closepath(pTerm);
		// draw a horizontal line between key title and first entry 
		if(pKey->title.text)
			DrawClipLine(pTerm, pKey->bounds.xleft, pKey->bounds.ytop - (_Bry.key_title_height + _Bry.key_title_extra), pKey->bounds.xright, pKey->bounds.ytop - (_Bry.key_title_height + _Bry.key_title_extra));
		V.P_ClipArea = clip_save;
	}
	_Bry.yl_ref = pKey->bounds.ytop - (_Bry.key_title_height + _Bry.key_title_extra);
	_Bry.yl_ref -= ((pKey->height_fix + 1) * key_entry_height) / 2;
	_Bry.xl = pKey->bounds.xleft + _Bry.key_size_left;
	_Bry.yl = _Bry.yl_ref;
}
// 
// This routine draws the plot title, the axis labels, and an optional time stamp.
// 
//void draw_titles()
void GnuPlot::DrawTitles(GpTermEntry * pTerm)
{
	// YLABEL 
	if(AxS[FIRST_Y_AXIS].label.text) {
		int x = ylabel_x;
		int y = (V.BbPlot.ytop + V.BbPlot.ybot) / 2;
		// There has been much argument about the optimal ylabel position 
		x += pTerm->ChrH / 4.0;
		WriteLabel(pTerm, x, y, &(AxS[FIRST_Y_AXIS].label));
		ResetTextColor(pTerm, &(AxS[FIRST_Y_AXIS].label.textcolor));
	}
	// Y2LABEL 
	if(AxS[SECOND_Y_AXIS].label.text) {
		int x = y2label_x;
		int y = (V.BbPlot.ytop + V.BbPlot.ybot) / 2;
		WriteLabel(pTerm, x, y, &(AxS[SECOND_Y_AXIS].label));
		ResetTextColor(pTerm, &(AxS[SECOND_Y_AXIS].label.textcolor));
	}
	// XLABEL 
	if(AxS[FIRST_X_AXIS].label.text) {
		text_label * label = &AxS[FIRST_X_AXIS].label;
		double tmpx, tmpy;
		MapPositionR(pTerm, &(label->offset), &tmpx, &tmpy, "xlabel");
		int x = (V.BbPlot.xright + V.BbPlot.xleft) / 2;
		int y = xlabel_y - pTerm->ChrV / 2;
		y -= tmpy; // xlabel_y already contained tmpy 
		WriteLabel(pTerm, x, y, label);
		ResetTextColor(pTerm, &(label->textcolor));
	}
	// X2LABEL 
	if(AxS[SECOND_X_AXIS].label.text) {
		// we worked out y-coordinate in boundary() 
		int x = (V.BbPlot.xright + V.BbPlot.xleft) / 2;
		int y = x2label_y - pTerm->ChrV / 2;
		WriteLabel(pTerm, x, y, &(AxS[SECOND_X_AXIS].label));
		ResetTextColor(pTerm, &(AxS[SECOND_X_AXIS].label.textcolor));
	}
	// RLABEL 
	if(AxS[POLAR_AXIS].label.text) {
		// This assumes we always have a horizontal R axis 
		int x = AxS.MapiX(PolarRadius(AxS.__R().max) / 2.0);
		int y = AxS.MapiY(0.0) + pTerm->ChrV;
		WriteLabel(pTerm, x, y, &(AxS[POLAR_AXIS].label));
		ResetTextColor(pTerm, &(AxS[POLAR_AXIS].label.textcolor));
	}
	// PLACE TIMELABEL 
	if(Gg.LblTime.text)
		do_timelabel(_Bry.time_x, _Bry.time_y);
}
//
// advance current position in the key in preparation for next key entry 
//
void advance_key(bool only_invert)
{
	legend_key * p_key = &keyT;
	if(p_key->invert)
		_Bry.yl = p_key->bounds.ybot + _Bry.yl_ref + key_entry_height/2 - _Bry.yl;
	if(!only_invert) {
		if(key_count >= key_rows) {
			_Bry.yl = _Bry.yl_ref;
			_Bry.xl += _Bry.key_col_wth;
			key_count = 0;
		}
		else
			_Bry.yl = _Bry.yl - key_entry_height;
	}
}
