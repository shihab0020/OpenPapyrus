// GNUPLOT - tabulate.c 
// Copyright 1986 - 1993, 1998, 2004   Thomas Williams, Colin Kelley
//
/*
 * EAM - April 2007
 * Collect the routines for tablular output into a separate file, and
 * extend support to newer plot styles (labels, vectors, ...).
 * These routines used to masquerade as a terminal type "set term table",
 * but since version 4.2 they are controlled by "set table <foo>" independent
 * of terminal settings.
 *
 * BM - April 2014
 * Support output to an in-memory named datablock.
 */
#include <gnuplot.h>
#pragma hdrstop

// File or datablock for output during 'set table' mode 
//FILE * table_outfile = NULL;
//udvt_entry * GPO.Tab.P_Var = NULL;
//bool   table_mode = FALSE;
//char * GPO.Tab.P_Sep = NULL;
//at_type * GPO.Tab.P_FilterAt = NULL;
//static FILE * GPO.Tab.P_OutFile;

#define OUTPUT_NUMBER(x, y) { /*GPO.*/OutputNumber(x, y, buffer); len = strappend(&line, &size, len, buffer); }
#define BUFFERSIZE 128

static char * expand_newline(const char * in)
{
	char * tmpstr = (char *)gp_alloc(2 * strlen(in) + 1, "enl");
	const char * s = in;
	char * t = tmpstr;
	do {
		if(*s == '\n') {
			*t++ = '\\';
			*t++ = 'n';
		}
		else
			*t++ = *s;
	} while(*s++);
	return tmpstr;
}

//static void output_number(double coord, int axIdx, char * pBuffer) 
void GnuPlot::OutputNumber(double coord, int axIdx, char * pBuffer)
{
	if(isnan(coord)) {
		sprintf(pBuffer, " NaN");
		// treat timedata and "%s" output format as a special case:
		// return a number.
		// "%s" in combination with any other character is treated
		// like a normal time format specifier and handled in time.c
	}
	else {
		GpAxis & r_ax = AxS[axIdx];
		if(r_ax.tictype == DT_TIMEDATE && strcmp(r_ax.formatstring, "%s") == 0) {
			GPrintf(pBuffer, BUFFERSIZE, "%.0f", 1.0, coord);
		}
		else if(r_ax.tictype == DT_TIMEDATE) {
			pBuffer[0] = '"';
			if(!strcmp(r_ax.formatstring, DEF_FORMAT))
				gstrftime(pBuffer+1, BUFFERSIZE-1, P_TimeFormat, coord);
			else
				gstrftime(pBuffer+1, BUFFERSIZE-1, r_ax.formatstring, coord);
			while(strchr(pBuffer, '\n')) {
				*(strchr(pBuffer, '\n')) = ' ';
			}
			strcat(pBuffer, "\"");
		}
		else
			GPrintf(pBuffer, BUFFERSIZE, r_ax.formatstring, 1.0, coord);
	}
	strcat(pBuffer, " ");
}

static void print_line(const char * str)
{
	if(!GPO.Tab.P_Var) {
		fputs(str, GPO.Tab.P_OutFile);
		fputc('\n', GPO.Tab.P_OutFile);
	}
	else {
		append_to_datablock(&GPO.Tab.P_Var->udv_value, sstrdup(str));
	}
}

static bool imploded(const curve_points * pPlot)
{
	switch(pPlot->plot_smooth) {
		// These smooth styles called cp_implode() 
		case SMOOTH_UNIQUE:
		case SMOOTH_FREQUENCY:
		case SMOOTH_FREQUENCY_NORMALISED:
		case SMOOTH_CUMULATIVE:
		case SMOOTH_CUMULATIVE_NORMALISED:
		case SMOOTH_CSPLINES:
		case SMOOTH_ACSPLINES:
		case SMOOTH_SBEZIER:
		case SMOOTH_MONOTONE_CSPLINE:
		    return TRUE;
		// These ones did not 
		case SMOOTH_NONE:
		case SMOOTH_BEZIER:
		case SMOOTH_KDENSITY:
		default:
		    break;
	}
	return FALSE;
}

//void print_table(curve_points * pPlot, int plot_num)
void GnuPlot::PrintTable(curve_points * pPlot, int plotNum)
{
	int i;
	char * buffer = (char*)gp_alloc(BUFFERSIZE, "print_table: output buffer");
	size_t size = 2*BUFFERSIZE;
	char * line = (char*)gp_alloc(size, "print_table: line buffer");
	size_t len = 0;
	Tab.P_OutFile = NZOR(Tab.P_TabOutFile, gpoutfile);
	for(int curve = 0; curve < plotNum; curve++, pPlot = pPlot->next) {
		GpCoordinate * point = NULL;
		// "with table" already wrote the output 
		if(pPlot->plot_style == TABLESTYLE)
			continue;
		// two blank lines between tabulated plots by prepending an empty line here 
		print_line("");
		snprintf(line, size, "# Curve %d of %d, %d points", curve, plotNum, pPlot->p_count);
		print_line(line);
		if((pPlot->title) && (*pPlot->title)) {
			char * title = expand_newline(pPlot->title);
			snprintf(line, size, "# Curve title: \"%s\"", title);
			print_line(line);
			SAlloc::F(title);
		}
		len = snprintf(line, size, "# x y");
		switch(pPlot->plot_style) {
			case BOXES:
			case XERRORBARS:
			    len = strappend(&line, &size, len, " xlow xhigh");
			    break;
			case BOXERROR:
			case YERRORBARS:
			    len = strappend(&line, &size, len, " ylow yhigh");
			    break;
			case BOXXYERROR:
			case XYERRORBARS:
			    len = strappend(&line, &size, len, " xlow xhigh ylow yhigh");
			    break;
			case FILLEDCURVES:
			    len = strappend(&line, &size, len, "1 y2");
			    break;
			case FINANCEBARS:
			    len = strappend(&line, &size, len, " open ylow yhigh yclose");
			    break;
			case CANDLESTICKS:
			    len = strappend(&line, &size, len, " open ylow yhigh yclose width");
			    break;
			case LABELPOINTS:
			    len = strappend(&line, &size, len, " label");
			    break;
			case VECTOR:
			    len = strappend(&line, &size, len, " delta_x delta_y");
			    break;
			case LINES:
			case POINTSTYLE:
			case LINESPOINTS:
			case DOTS:
			case IMPULSES:
			case STEPS:
			case FSTEPS:
			case HISTEPS:
			    break;
			case IMAGE:
			    len = strappend(&line, &size, len, "  pixel");
			    break;
			case RGBIMAGE:
			case RGBA_IMAGE:
			    len = strappend(&line, &size, len, "  red green blue alpha");
			    break;

			default:
			    if(interactive)
				    fprintf(stderr, "Tabular output of %s plot style not fully implemented\n", pPlot->plot_style == HISTOGRAMS ? "histograms" : "this");
			    break;
		}
		if(pPlot->varcolor)
			len = strappend(&line, &size, len, "  color");
		strappend(&line, &size, len, " type");
		print_line(line);
		if(pPlot->plot_style == LABELPOINTS) {
			text_label * this_label;
			for(this_label = pPlot->labels->next; this_label; this_label = this_label->next) {
				char * label = expand_newline(this_label->text);
				line[0] = NUL;
				len = 0;
				OUTPUT_NUMBER(this_label->place.x, pPlot->AxIdx_X);
				OUTPUT_NUMBER(this_label->place.y, pPlot->AxIdx_Y);
				len = strappend(&line, &size, len, "\"");
				len = strappend(&line, &size, len, label);
				len = strappend(&line, &size, len, "\"");
				print_line(line);
				SAlloc::F(label);
			}
		}
		else {
			int plotstyle = pPlot->plot_style;
			int type;
			bool replace_undefined_with_blank = imploded(pPlot);
			if(plotstyle == HISTOGRAMS && pPlot->histogram->type == HT_ERRORBARS)
				plotstyle = YERRORBARS;
			for(i = 0, point = pPlot->points; i < pPlot->p_count; i++, point++) {
				// Reproduce blank lines read from original input file, if any 
				if(!memcmp(point, &blank_data_line, sizeof(GpCoordinate))) {
					print_line("");
					continue;
				}
				// FIXME HBB 20020405: had better use the real x/x2 axes of this plot 
				line[0] = NUL;
				len = 0;
				OUTPUT_NUMBER(point->x, pPlot->AxIdx_X);
				OUTPUT_NUMBER(point->y, pPlot->AxIdx_Y);
				switch(plotstyle) {
					case BOXES:
					case XERRORBARS:
					    OUTPUT_NUMBER(point->xlow, pPlot->AxIdx_X);
					    OUTPUT_NUMBER(point->xhigh, pPlot->AxIdx_X);
					    // Hmmm... shouldn't this write out width field of box plots, too, if stored? 
					    break;
					case BOXXYERROR:
					case XYERRORBARS:
					    OUTPUT_NUMBER(point->xlow, pPlot->AxIdx_X);
					    OUTPUT_NUMBER(point->xhigh, pPlot->AxIdx_X);
					/* FALLTHROUGH */
					case BOXERROR:
					case YERRORBARS:
					    OUTPUT_NUMBER(point->ylow, pPlot->AxIdx_Y);
					    OUTPUT_NUMBER(point->yhigh, pPlot->AxIdx_Y);
					    break;
					case IMAGE:
					    snprintf(buffer, BUFFERSIZE, "%g ", point->z);
					    len = strappend(&line, &size, len, buffer);
					    break;
					case RGBIMAGE:
					case RGBA_IMAGE:
					    snprintf(buffer, BUFFERSIZE, "%4d %4d %4d %4d ",
						(int)point->CRD_R, (int)point->CRD_G,
						(int)point->CRD_B, (int)point->CRD_A);
					    len = strappend(&line, &size, len, buffer);
					    break;
					case FILLEDCURVES:
					    OUTPUT_NUMBER(point->yhigh, pPlot->AxIdx_Y);
					    break;
					case FINANCEBARS:
					    OUTPUT_NUMBER(point->ylow, pPlot->AxIdx_Y);
					    OUTPUT_NUMBER(point->yhigh, pPlot->AxIdx_Y);
					    OUTPUT_NUMBER(point->z, pPlot->AxIdx_Y);
					    break;
					case CANDLESTICKS:
					    OUTPUT_NUMBER(point->ylow, pPlot->AxIdx_Y);
					    OUTPUT_NUMBER(point->yhigh, pPlot->AxIdx_Y);
					    OUTPUT_NUMBER(point->z, pPlot->AxIdx_Y);
					    OUTPUT_NUMBER(2. * (point->x - point->xlow), pPlot->AxIdx_X);
					    break;
					case VECTOR:
					    OUTPUT_NUMBER((point->xhigh - point->x), pPlot->AxIdx_X);
					    OUTPUT_NUMBER((point->yhigh - point->y), pPlot->AxIdx_Y);
					    break;
					case LINES:
					case POINTSTYLE:
					case LINESPOINTS:
					case DOTS:
					case IMPULSES:
					case STEPS:
					case FSTEPS:
					case HISTEPS:
					    break;
					default:
					    /* ? */
					    break;
				} /* switch(plot type) */
				if(pPlot->varcolor) {
					double colorval = pPlot->varcolor[i];
					if((pPlot->lp_properties.pm3d_color.value < 0.0) && (pPlot->lp_properties.pm3d_color.type == TC_RGB)) {
						snprintf(buffer, BUFFERSIZE, "0x%06x", (uint)(colorval));
						len = strappend(&line, &size, len, buffer);
					}
					else if(pPlot->lp_properties.pm3d_color.type == TC_Z) {
						OUTPUT_NUMBER(colorval, COLOR_AXIS);
					}
					else if(pPlot->lp_properties.l_type == LT_COLORFROMCOLUMN) {
						OUTPUT_NUMBER(colorval, COLOR_AXIS);
					}
				}
				type = pPlot->points[i].type;
				snprintf(buffer, BUFFERSIZE, " %c", type == INRANGE ? 'i' : type == OUTRANGE ? 'o' : 'u');
				strappend(&line, &size, len, buffer);
				// cp_implode() inserts dummy undefined point between curves 
				// but datafiles use a blank line for this purpose 
				if(type == UNDEFINED && replace_undefined_with_blank)
					print_line("");
				else
					print_line(line);
			} /* for(point i) */
		}
		print_line("");
	} /* for(curve) */
	if(Tab.P_OutFile)
		fflush(Tab.P_OutFile);
	SAlloc::F(buffer);
	SAlloc::F(line);
}

//void print_3dtable(int pcount)
void GnuPlot::Print3DTable(int pcount)
{
	GpSurfacePoints * this_plot;
	int i, surface;
	GpCoordinate * point;
	GpCoordinate * tail;
	char * buffer = (char*)gp_alloc(BUFFERSIZE, "print_3dtable: output buffer");
	size_t size = 2*BUFFERSIZE;
	char * line = (char*)gp_alloc(size, "print_3dtable: line buffer");
	size_t len = 0;
	Tab.P_OutFile = NZOR(Tab.P_TabOutFile, gpoutfile);
	for(surface = 0, this_plot = first_3dplot; surface < pcount; this_plot = this_plot->next_sp, surface++) {
		print_line("");
		snprintf(line, size, "# Surface %d of %d surfaces", surface, pcount);
		print_line(line);
		if((this_plot->title) && (*this_plot->title)) {
			char * title = expand_newline(this_plot->title);
			print_line("");
			snprintf(line, size, "# Curve title: \"%s\"", title);
			print_line(line);
			SAlloc::F(title);
		}
		switch(this_plot->plot_style) {
			case LABELPOINTS:
				{
					text_label * this_label;
					for(this_label = this_plot->labels->next; this_label != NULL; this_label = this_label->next) {
						char * label = expand_newline(this_label->text);
						line[0] = NUL;
						len = 0;
						OUTPUT_NUMBER(this_label->place.x, FIRST_X_AXIS);
						OUTPUT_NUMBER(this_label->place.y, FIRST_Y_AXIS);
						OUTPUT_NUMBER(this_label->place.z, FIRST_Z_AXIS);
						len = strappend(&line, &size, len, "\"");
						len = strappend(&line, &size, len, label);
						len = strappend(&line, &size, len, "\"");
						print_line(line);
						SAlloc::F(label);
					}
				}
			    continue;
			case LINES:
			case POINTSTYLE:
			case IMPULSES:
			case DOTS:
			case VECTOR:
			    break;
			case IMAGE:
			case RGBIMAGE:
			case RGBA_IMAGE:
			    break;
			default:
			    fprintf(stderr, "Tabular output of this 3D plot style not implemented\n");
			    continue;
		}
		if(draw_surface) {
			iso_curve * icrvs;
			int curve;
			// only the curves in one direction 
			for(curve = 0, icrvs = this_plot->iso_crvs; icrvs && curve < this_plot->num_iso_read; icrvs = icrvs->next, curve++) {
				print_line("");
				snprintf(line, size, "# IsoCurve %d, %d points", curve, icrvs->p_count);
				print_line(line);
				len = sprintf(line, "# x y z");
				tail = NULL; /* Just to shut up a compiler warning */
				switch(this_plot->plot_style) {
					case VECTOR:
					    tail = icrvs->next->points;
					    len = strappend(&line, &size, len, " delta_x delta_y delta_z");
					    break;
					case IMAGE:
					    len = strappend(&line, &size, len, "  pixel");
					    break;
					case RGBIMAGE:
					case RGBA_IMAGE:
					    len = strappend(&line, &size, len, "  red green blue alpha");
					    break;
					default:
					    break;
				}
				strappend(&line, &size, len, " type");
				print_line(line);
				for(i = 0, point = icrvs->points; i < icrvs->p_count; i++, point++) {
					line[0] = NUL;
					len = 0;
					OUTPUT_NUMBER(point->x, FIRST_X_AXIS);
					OUTPUT_NUMBER(point->y, FIRST_Y_AXIS);
					OUTPUT_NUMBER(point->z, FIRST_Z_AXIS);
					if(this_plot->plot_style == VECTOR) {
						OUTPUT_NUMBER((tail->x - point->x), FIRST_X_AXIS);
						OUTPUT_NUMBER((tail->y - point->y), FIRST_Y_AXIS);
						OUTPUT_NUMBER((tail->z - point->z), FIRST_Z_AXIS);
						tail++;
					}
					else if(this_plot->plot_style == IMAGE) {
						snprintf(buffer, BUFFERSIZE, "%g ", point->CRD_COLOR);
						len = strappend(&line, &size, len, buffer);
					}
					else if(oneof2(this_plot->plot_style, RGBIMAGE, RGBA_IMAGE)) {
						snprintf(buffer, BUFFERSIZE, "%4d %4d %4d %4d ", (int)point->CRD_R, (int)point->CRD_G, (int)point->CRD_B, (int)point->CRD_A);
						len = strappend(&line, &size, len, buffer);
					}
					snprintf(buffer, BUFFERSIZE, "%c", point->type == INRANGE ? 'i' : point->type == OUTRANGE ? 'o' : 'u');
					strappend(&line, &size, len, buffer);
					print_line(line);
				} /* for (point) */
			} /* for (icrvs) */
			print_line("");
		} /* if (draw_surface) */
		if(draw_contour) {
			int number = 0;
			gnuplot_contours * c = this_plot->contours;
			while(c) {
				int count = c->num_pts;
				GpCoordinate * point = c->coords;
				if(c->isNewLevel) {
					/* don't display count - contour split across chunks */
					/* put # in case user wants to use it for a plot */
					/* double blank line to allow plot ... index ... */
					print_line("");
					snprintf(line, size, "# Contour %d, label: %s", number++, c->label);
					print_line(line);
				}
				for(; --count >= 0; ++point) {
					line[0] = NUL;
					len = 0;
					OUTPUT_NUMBER(point->x, FIRST_X_AXIS);
					OUTPUT_NUMBER(point->y, FIRST_Y_AXIS);
					OUTPUT_NUMBER(point->z, FIRST_Z_AXIS);
					print_line(line);
				}
				/* blank line between segments of same contour */
				print_line("");
				c = c->next;
			} /* while (contour) */
		} /* if (draw_contour) */
	} /* for(surface) */
	if(Tab.P_OutFile)
		fflush(Tab.P_OutFile);
	SAlloc::F(buffer);
	SAlloc::F(line);
}
//
// Called from plot2d.c (get_data) for "plot with table"
//
//bool tabulate_one_line(double v[MAXDATACOLS], GpValue str[MAXDATACOLS], int ncols)
bool GnuPlot::TabulateOneLine(double v[MAXDATACOLS], GpValue str[MAXDATACOLS], int ncols)
{
	int col;
	FILE * f_out = NZOR(Tab.P_TabOutFile, gpoutfile);
	GpValue keep;
	if(Tab.P_FilterAt) {
		evaluate_inside_using = TRUE;
		EvaluateAt(Tab.P_FilterAt, &keep);
		evaluate_inside_using = FALSE;
		if(Ev.IsUndefined_ || isnan(real(&keep)) || real(&keep) == 0)
			return FALSE;
	}
	if(Tab.P_Var == NULL) {
		char sep = (Tab.P_Sep && *Tab.P_Sep) ? *Tab.P_Sep : '\t';
		for(col = 0; col < ncols; col++) {
			if(str[col].type == STRING)
				fprintf(f_out, " %s", str[col].v.string_val);
			else
				fprintf(f_out, " %g", v[col]);
			if(col < ncols-1)
				fprintf(f_out, "%c", sep);
		}
		fprintf(f_out, "\n");
	}
	else {
		char buf[64]; /* buffer large enough to hold %g + 2 extra chars */
		char sep = (Tab.P_Sep && *Tab.P_Sep) ? *Tab.P_Sep : '\t';
		size_t size = sizeof(buf);
		char * line = (char*)gp_alloc(size, "");
		size_t len = 0;
		line[0] = NUL;
		for(col = 0; col < ncols; col++) {
			if(str[col].type == STRING) {
				len = strappend(&line, &size, 0, str[col].v.string_val);
			}
			else {
				snprintf(buf, sizeof(buf), " %g", v[col]);
				len = strappend(&line, &size, len, buf);
			}
			if(col < ncols-1) {
				snprintf(buf, sizeof(buf), " %c", sep);
				len = strappend(&line, &size, len, buf);
			}
		}
		append_to_datablock(&Tab.P_Var->udv_value, line);
	}
	return TRUE;
}