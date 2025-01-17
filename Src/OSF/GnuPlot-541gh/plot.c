// GNUPLOT - plot.c 
// Copyright 1986 - 1993, 1998, 2004   Thomas Williams, Colin Kelley
//
#include <gnuplot.h>
#pragma hdrstop
#ifdef _WIN32
	#include "win/winmain.h"
	#include "win/wcommon.h"
#endif
// 
// GNU readline
// Only required by two files directly,
// so I don't put this into a header file. -lh
// 
#if defined(HAVE_LIBREADLINE) && !defined(MISSING_RL_TILDE_EXPANSION)
#include <readline/tilde.h>
extern int rl_complete_with_tilde_expansion;
#endif
// 
// BSD editline
// 
#ifdef HAVE_LIBEDITLINE
	#include <editline/readline.h>
#endif
// enable gnuplot history with readline 
#ifdef GNUPLOT_HISTORY
	#ifndef GNUPLOT_HISTORY_FILE
		#define GNUPLOT_HISTORY_FILE "~/.gnuplot_history"
	#endif
	// 
	// expanded_history_filename points to the value from 'tilde_expand()',
	// which expands '~' to the user's home directory, or $HOME.
	// Depending on your OS you have to make sure that the "$HOME" environment
	// variable exists.  You are responsible for valid values.
	// 
	static char * expanded_history_filename;
	static void wrapper_for_write_history();
#endif

static RETSIGTYPE inter(int anint);
#ifdef X11
	extern int X11_args(int, char **); /* FIXME: defined in term/x11.trm */
#endif

static RETSIGTYPE inter(int /*anint*/)
{
	signal(SIGINT, (sigfunc)inter);
	signal(SIGFPE, SIG_DFL); /* turn off FPE trapping */
#if defined(WGP_CONSOLE)
	// The Windows console Ctrl-C handler runs in another thread. So a
	// longjmp() would result in crash. Instead, we handle these
	// events asynchronously.
	ctrlc_flag = TRUE;
	// Interrupt ConsoleGetch. 
	SendMessage(P_GraphWin->hWndGraph, WM_NULL, 0, 0);
	SendMessage(GetConsoleWindow(), WM_CHAR, 0x20, 0);
#else
	{
		GPO.TermReset(GPT.P_Term);
		putc('\n', stderr);
		GPO.BailToCommandLine(); // return to prompt 
	}
#endif
}
// 
// a wrapper for longjmp so we can keep everything local 
// 
//void bail_to_command_line()
void GnuPlot::BailToCommandLine()
{
#ifdef _WIN32
	kill_pending_Pause_dialog();
	_Plt.ctrlc_flag = false;
#endif
	LONGJMP(_Plt.command_line_env, TRUE);
}

//#if defined(_WIN32)
//int gnu_main(int argc_orig, char ** argv)
//#else
//int main(int argc_orig, char ** argv)
//#endif
int GnuPlot::ImplementMain(int argc_orig, char ** argv)
{
	int i;
	// We want the current value of argc to persist across a LONGJMP from GPO.IntError().
	// Without this the compiler may put it on the stack, which LONGJMP clobbers.
	// Here we try make it a volatile variable that optimization will not affect.
	// Why do we not have to do the same for argv?   I don't know.
	// But the test cases that broke with generic argc seem fine with generic argv.
	static volatile int argc;
	argc = argc_orig;
	// make sure that we really have revoked root access, this might happen if
	// gnuplot is compiled without vga support but is installed suid by mistake 
#ifdef __linux__
	if(setuid(getuid()) != 0) {
		fprintf(stderr, "gnuplot: refusing to run at elevated privilege\n");
		exit(EXIT_FAILURE);
	}
#endif
	// HBB: Seems this isn't needed any more for DJGPP V2? 
	// HBB: disable all floating point exceptions, just keep running...
#if defined(DJGPP) && (DJGPP!=2)
	_control87(MCW_EM, MCW_EM);
#endif
	// malloc large blocks, otherwise problems with fragmented mem 
#ifdef MALLOCDEBUG
	malloc_debug(7);
#endif
	// init progpath and get helpfile from executable directory 
#if (defined(PIPE_IPC) || defined(_WIN32)) && (defined(HAVE_LIBREADLINE) || (defined(HAVE_LIBEDITLINE) && defined(X11)))
	// Editline needs this to be set before the very first call to readline(). 
	// Support for rl_getc_function is broken for utf-8 in editline. Since it is only
	// really required for X11, disable this section when building without X11. 
	rl_getc_function = getc_wrapper;
#endif
#if defined(HAVE_LIBREADLINE) || defined(HAVE_LIBEDITLINE)
	// T.Walter 1999-06-24: 'rl_readline_name' must be this fix name.
	// It is used to parse a 'gnuplot' specific section in '~/.inputrc'
	// or gnuplot specific commands in '.editrc' (when using editline
	// instead of readline) 
	rl_readline_name = "Gnuplot";
	rl_terminal_name = getenv("TERM");
#if defined(HAVE_LIBREADLINE)
	using_history();
#else
	history_init();
#endif
#endif
#if defined(HAVE_LIBREADLINE) && !defined(MISSING_RL_TILDE_EXPANSION)
	rl_complete_with_tilde_expansion = 1;
#endif
	for(i = 1; i < argc; i++) {
		if(!argv[i])
			continue;
		if(sstreq(argv[i], "-V") || sstreq(argv[i], "--version")) {
			printf("gnuplot %s patchlevel %s\n", gnuplot_version, gnuplot_patchlevel);
			return 0;
		}
		else if(sstreq(argv[i], "-h") || sstreq(argv[i], "--help")) {
			printf("Usage: gnuplot [OPTION] ... [FILE]\n"
#ifdef X11
			    "for X11 options see 'help X11->command-line-options'\n"
#endif
			    "  -V, --version\n"
			    "  -h, --help\n"
			    "  -p  --persist\n"
			    "  -s  --slow\n"
			    "  -d  --default-settings\n"
			    "  -c  scriptfile ARG1 ARG2 ... \n"
			    "  -e  \"command1; command2; ...\"\n"
			    "gnuplot %s patchlevel %s\n",
			    gnuplot_version, gnuplot_patchlevel);
#ifdef DEVELOPMENT_VERSION
			printf(
#ifdef DIST_CONTACT
				"Report bugs to "DIST_CONTACT "\n"
				"            or %s\n",
#else
				"Report bugs to %s\n",
#endif
				bug_email);
#endif
			return 0;
		}
		else if(!strncmp(argv[i], "-persist", 2) || sstreq(argv[i], "--persist")
#ifdef _WIN32
		    || sstreqi_ascii(argv[i], "-noend") || sstreqi_ascii(argv[i], "/noend")
#endif
		    ) {
			_Plt.persist_cl = TRUE;
		}
		else if(!strncmp(argv[i], "-slow", 2) || sstreq(argv[i], "--slow")) {
			_Plt.slow_font_startup = TRUE;
		}
		else if(!strncmp(argv[i], "-d", 2) || sstreq(argv[i], "--default-settings")) {
			_Plt.skip_gnuplotrc = TRUE; // Skip local customization read from ~/.gnuplot 
		}
	}
#ifdef X11
	// the X11 terminal removes tokens that it recognizes from argv. 
	{
		int n = X11_args(argc, argv);
		argv += n;
		argc -= n;
	}
#endif
	setbuf(stderr, (char *)NULL);
#ifdef HAVE_SETVBUF
	// This was once setlinebuf(). Docs say this is
	// identical to setvbuf(,NULL,_IOLBF,0), but MS C
	// faults this (size out of range), so we try with
	// size of 1024 instead. [SAS/C does that, too. -lh]
	if(setvbuf(stdout, (char *)NULL, _IOLBF, (size_t)1024) != 0)
		fputs("Could not linebuffer stdout\n", stderr);
	// Switching to unbuffered mode causes all characters in the input
	// buffer to be lost. So the only safe time to do it is on program entry.
	// Do any non-X platforms suffer from this problem?
	// EAM - Jan 2013 YES.
	setvbuf(stdin, (char *)NULL, _IONBF, 0);
#endif
	GPT.P_GpOutFile = stdout;
	// Initialize pre-loaded user variables 
	// "pi" is hard-wired as the first variable 
	Ev.AddUdvByName("GNUTERM");
	Ev.AddUdvByName("I");
	Ev.AddUdvByName("NaN");
	Ev.InitConstants();
	Ev.PP_UdvUserHead = &(Ev.P_UdvNaN->next_udv);
	InitMemory();
	_Plt.interactive = false;
	// April 2017:  We used to call init_terminal() here, but now   
	// We defer initialization until error handling has been set up. 
#if defined(_WIN32) && !defined(WGP_CONSOLE)
	_Plt.interactive = true;
#else
	interactive = isatty(fileno(stdin));
#endif
	// Note: we want to know whether this is an interactive session so that we can
	// decide whether or not to write status information to stderr.  The old test
	// for this was to see if (argc > 1) but the addition of optional command line
	// switches broke this.  What we really wanted to know was whether any of the
	// command line arguments are file names or an explicit in-line "-e command".
	for(i = 1; i < argc; i++) {
#ifdef _WIN32
		if(sstreqi_ascii(argv[i], "/noend"))
			continue;
#endif
		if(argv[i][0] != '-' || oneof2(argv[i][1], 'e', 'c')) {
			_Plt.interactive = false;
			break;
		}
	}
	// Need this before show_version is called for the first time 
	ShowVersion(_Plt.interactive ? stderr : 0/* Only load GPVAL_COMPILE_OPTIONS */);
	UpdateGpvalVariables(/*GPT.P_Term*/0, 3); // update GPVAL_ variables available to user // ����� term ��� �� ���������
	if(!SETJMP(_Plt.command_line_env, 1)) {
		// first time 
		interrupt_setup();
		GetUserEnv();
		init_loadpath();
		LocaleHandler(ACTION_INIT, NULL);
		memzero(&SmPltt, sizeof(SmPltt));
		InitFit(); // Initialization of fitting module 
#ifdef READLINE
		// When using the built-in readline, we set the initial
		// encoding according to the locale as this is required
		// to properly handle keyboard input. 
		InitEncoding();
#endif
		InitGadgets();
		// April 2017: Now that error handling is in place, it is safe parse
		// GNUTERM during terminal initialization.
		// atexit processing is done in reverse order. We want
		// the generic terminal shutdown in term_reset to be executed before
		// any terminal specific cleanup requested by individual terminals.
		InitTerminal();
		PushTerminal(0); // remember the initial terminal 
		/* @sobolev term_reset �������� �� GnuPlot::TermReset(GpTermEntry *) ������ ������������ �� ����� ��� ������.
			gp_atexit(term_reset); 
		*/
		// Execute commands in ~/.gnuplot 
		InitSession();
		if(_Plt.interactive && GPT.P_Term) { // not unknown 
#ifdef GNUPLOT_HISTORY
#if (defined(HAVE_LIBREADLINE) || defined(HAVE_LIBEDITLINE)) && !defined(_WIN32)
			expanded_history_filename = tilde_expand(GNUPLOT_HISTORY_FILE);
#else
			expanded_history_filename = sstrdup(GNUPLOT_HISTORY_FILE);
			GpExpandTilde(&expanded_history_filename);
#endif
			ReadHistory(expanded_history_filename);
			// 
			// It is safe to ignore the return values of 'atexit()' and
			// 'on_exit()'. In the worst case, there is no history of your
			// current session and you have to type all again in your next session.
			// 
			gp_atexit(wrapper_for_write_history);
#endif /* GNUPLOT_HISTORY */

#if defined(READLINE) && defined(WGP_CONSOLE)
			fprintf(stderr, "Encoding set to '%s'.\n", encoding_names[encoding]);
#endif
		} // if(interactive && GPT.P_Term != 0) 
	}
	else {
		// come back here from IntError() 
		if(!_Plt.successful_initialization) {
			// Only print the warning once 
			_Plt.successful_initialization = true;
			fprintf(stderr, "WARNING: Error during initialization\n\n");
		}
		if(_Plt.interactive == FALSE)
			_Plt.exit_status = EXIT_FAILURE;
#ifdef HAVE_READLINE_RESET
		else {
			/* reset properly readline after a SIGINT+longjmp */
			rl_reset_after_signal();
		}
#endif
		LoadFileError(); /* if we were in load_file(), cleanup */
		SET_CURSOR_ARROW;
		// Why a goto?  Because we exited the loop below via int_error 
		// using LONGJMP.  The compiler was not expecting this, and    
		// "optimized" the handling of argc and argv such that simply  
		// entering the loop again from the top finds them messed up.  
		// If we reenter the loop via a goto then there is some hope   
		// that code reordering does not hurt us.                      
		if(_Plt.reading_from_dash && _Plt.interactive)
			goto RECOVER_FROM_ERROR_IN_DASH;
		_Plt.reading_from_dash = false;
		if(!_Plt.interactive && !_Plt.noinputfiles) {
			TermReset(GPT.P_Term);
			gp_exit(EXIT_FAILURE); /* exit on non-interactive error */
		}
	}
	// load filenames given as arguments 
	while(--argc > 0) {
		++argv;
		Pgm.CToken = 0;
		if(!strncmp(*argv, "-persist", 2) || sstreq(*argv, "--persist")
#ifdef _WIN32
		    || sstreqi_ascii(*argv, "-noend") || sstreqi_ascii(*argv, "/noend")
#endif
		    ) {
			FPRINTF((stderr, "'persist' command line option recognized\n"));
		}
		else if(sstreq(*argv, "-")) {
#if defined(_WIN32) && !defined(WGP_CONSOLE)
			TextShow(&_WinM.TxtWin);
			_Plt.interactive = true;
#else
			_Plt.interactive = isatty(fileno(stdin));
#endif
RECOVER_FROM_ERROR_IN_DASH:
			_Plt.reading_from_dash = true;
			while(!ComLine())
				;
			_Plt.reading_from_dash = false;
			_Plt.interactive = false;
			_Plt.noinputfiles = false;
		}
		else if(sstreq(*argv, "-e")) {
			int save_state = _Plt.interactive;
			--argc; ++argv;
			if(argc <= 0) {
				fprintf(stderr, "syntax:  gnuplot -e \"commands\"\n");
				return 0;
			}
			_Plt.interactive = false;
			_Plt.noinputfiles = false;
			DoString(*argv);
			_Plt.interactive = save_state;
		}
		else if(!strncmp(*argv, "-slow", 2) || sstreq(*argv, "--slow")) {
			_Plt.slow_font_startup = true;
		}
		else if(!strncmp(*argv, "-d", 2) || sstreq(*argv, "--default-settings")) {
			// Ignore this; it already had its effect 
			FPRINTF((stderr, "ignoring -d\n"));
		}
		else if(sstreq(*argv, "-c")) {
			// Pass command line arguments to the gnuplot script in the next
			// argument. This consumes the remainder of the command line
			_Plt.interactive = false;
			_Plt.noinputfiles = false;
			--argc; ++argv;
			if(argc <= 0) {
				fprintf(stderr, "syntax:  gnuplot -c scriptname args\n");
				gp_exit(EXIT_FAILURE);
			}
			call_argc = MIN(9, argc - 1);
			for(i = 0; i<=call_argc; i++) {
				call_args[i] = sstrdup(argv[i+1]); // Need to stash argv[i] somewhere visible to load_file() 
			}
			LoadFile(LoadPath_fopen(*argv, "r"), sstrdup(*argv), 5);
			gp_exit(EXIT_SUCCESS);
		}
		else if(*argv[0] == '-') {
			fprintf(stderr, "unrecognized option %s\n", *argv);
		}
		else {
			_Plt.interactive = false;
			_Plt.noinputfiles = false;
			LoadFile(LoadPath_fopen(*argv, "r"), sstrdup(*argv), 4);
		}
	}
	// take commands from stdin 
	if(_Plt.noinputfiles) {
		while(!ComLine())
			_Plt.ctrlc_flag = false; // reset asynchronous Ctrl-C flag 
	}
#ifdef _WIN32
	// On Windows, handle 'persist' by keeping the main input loop running (windows/wxt), 
	// but only if there are any windows open. Note that qt handles this properly. 
	if(_Plt.persist_cl) {
		if(WinAnyWindowOpen()) {
#ifdef WGP_CONSOLE
			if(!_Plt.interactive) {
				// no further input from pipe 
				while(WinAnyWindowOpen())
					win_sleep(GPT.P_Term, 100);
			}
			else
#endif
			{
				_Plt.interactive = true;
				while(!ComLine())
					_Plt.ctrlc_flag = false; // reset asynchronous Ctrl-C flag 
				_Plt.interactive = false;
			}
		}
	}
#endif
#if (defined(HAVE_LIBREADLINE) || defined(HAVE_LIBEDITLINE)) && defined(GNUPLOT_HISTORY)
#if !defined(HAVE_ATEXIT) && !defined(HAVE_ON_EXIT)
	// You should be here if you neither have 'atexit()' nor 'on_exit()' 
	wrapper_for_write_history();
#endif /* !HAVE_ATEXIT && !HAVE_ON_EXIT */
#endif /* (HAVE_LIBREADLINE || HAVE_LIBEDITLINE) && GNUPLOT_HISTORY */
	// HBB 20040223: Not all compilers like exit() to end main() 
	// exit(exit_status); 
#if !defined(_WIN32)
	gp_exit_cleanup(); // Windows does the cleanup later 
#endif
	return _Plt.exit_status;
}

// Set up to catch interrupts 
void interrupt_setup()
{
	signal(SIGINT, (sigfunc)inter);
#ifdef SIGPIPE
	// ignore pipe errors, this might happen with set output "|head" 
	signal(SIGPIPE, SIG_IGN);
#endif
}
//
// Initialize 'constants' stored as variables (user could mangle these)
//
//void init_constants()
void GpEval::InitConstants()
{
	Gcomplex(&UdvPi.udv_value, SMathConst::Pi, 0.0);
	P_UdvNaN = GetUdvByName("NaN");
	Gcomplex(&(P_UdvNaN->udv_value), fgetnan(), 0.0);
	P_UdvI = GetUdvByName("I");
	Gcomplex(&P_UdvI->udv_value, 0.0, 1.0);
}
//
// Initialize graphics context, color palette, local preferences.
// Called both at program startup and by "reset session".
//
//void init_session()
void GnuPlot::InitSession()
{
	_Plt.successful_initialization = FALSE; /* Disable pipes and system commands during initialization */
	DelUdvByName("", TRUE); /* Undefine any previously-used variables */
	SetColorSequence(1); /* Restore default colors before loading local preferences */
	Ev.OverflowHandling = INT64_OVERFLOW_TO_FLOAT; /* Reset program variables not handled by 'reset' */
	InitVoxelSupport(); /* Reset voxel data structures if supported */
	// Make sure all variables start in the same state 'reset' would set them to. 
	ResetCommand(); /* FIXME: this does c_token++ */
	LoadRcFile(0); /* System-wide gnuplotrc if configured */
	LoadRcFile(1); /* ./.gnuplot if configured */
	// After this point we allow pipes and system commands 
	_Plt.successful_initialization = TRUE;
	LoadRcFile(2); /* ~/.gnuplot */
	LoadRcFile(3); /* ~/.config/gnuplot/gnuplotrc */
}
// 
// Read commands from an initialization file.
// where = 0: look for gnuplotrc in system shared directory
// where = 1: look for .gnuplot in current directory
// where = 2: look for .gnuplot in home directory
// 
//static void load_rcfile(int where)
void GnuPlot::LoadRcFile(int where)
{
	FILE * plotrc = NULL;
	char * rcfile = NULL;
	if(_Plt.skip_gnuplotrc)
		return;
	if(where == 0) {
#ifdef GNUPLOT_SHARE_DIR
#if defined(_WIN32)
		rcfile = RelativePathToGnuplot(GNUPLOT_SHARE_DIR "\\gnuplotrc");
#else
		rcfile = (char *)SAlloc::M(strlen(GNUPLOT_SHARE_DIR) + 1 + strlen("gnuplotrc") + 1);
		strcpy(rcfile, GNUPLOT_SHARE_DIR);
		PATH_CONCAT(rcfile, "gnuplotrc");
#endif
		plotrc = fopen(rcfile, "r");
#endif
	}
	else if(where == 1) {
#ifdef USE_CWDRC
		// Allow check for a .gnuplot init file in the current directory 
		// This is a security risk, as someone might leave a malicious   
		// init file in a shared directory.                              
		plotrc = fopen(PLOTRC, "r");
#endif
	}
	else if(where == 2 && _Plt.user_homedir) {
		// length of homedir + directory separator + length of file name + \0 
		int len = (_Plt.user_homedir ? strlen(_Plt.user_homedir) : 0) + 1 + strlen(PLOTRC) + 1;
		rcfile = static_cast<char *>(SAlloc::M(len));
		strcpy(rcfile, _Plt.user_homedir);
		PATH_CONCAT(rcfile, PLOTRC);
		plotrc = fopen(rcfile, "r");
	}
	else if(where == 3) {
#ifdef __unix__
		char * XDGConfigHome = xdg_get_var(kXDGConfigHome);
		size_t len = strlen(XDGConfigHome);
		rcfile = SAlloc::M(len + 1 + sizeof("gnuplot/gnuplotrc"));
		strcpy(rcfile, XDGConfigHome);
		PATH_CONCAT(rcfile, "gnuplot/gnuplotrc");
		plotrc = fopen(rcfile, "r");
		SAlloc::F(XDGConfigHome);
#endif
	}
	if(plotrc) {
		char * rc = sstrdup(rcfile ? rcfile : PLOTRC);
		LoadFile(plotrc, rc, 3);
		PushTerminal(0); // needed if terminal or its options were changed 
	}
	SAlloc::F(rcfile);
}

//void get_user_env()
void GnuPlot::GetUserEnv()
{
	if(!_Plt.user_homedir) {
		const char * env_home = 0;
		if((env_home = getenv(HOME)) ||
#ifdef _WIN32
		    (env_home = appdata_directory()) || (env_home = getenv("USERPROFILE")) ||
#endif
		    (env_home = getenv("HOME")))
			_Plt.user_homedir = (const char *)sstrdup(env_home);
		else if(_Plt.interactive)
			IntWarn(NO_CARET, "no HOME found");
	}
	if(!_Plt.user_shell) {
		const char * env_shell = 0;
		if((env_shell = getenv("SHELL")) == NULL)
#if defined(_WIN32)
			if((env_shell = getenv("COMSPEC")) == NULL)
#endif
			env_shell = SHELL;
		_Plt.user_shell = (const char *)sstrdup(env_shell);
	}
}
// 
// expand tilde in path
// path cannot be a static array!
// tilde must be the first character in *pathp;
// we may change that later
// 
//void gp_expand_tilde(char ** ppPath)
void GnuPlot::GpExpandTilde(char ** ppPath)
{
	if(!*ppPath)
		IntError(NO_CARET, "Cannot expand empty path");
	if((*ppPath)[0] == '~' && (*ppPath)[1] == DIRSEP1) {
		if(_Plt.user_homedir) {
			size_t n = strlen(*ppPath);
			*ppPath = (char *)SAlloc::R(*ppPath, n + strlen(_Plt.user_homedir));
			// include null at the end ... 
			memmove(*ppPath + strlen(_Plt.user_homedir) - 1, *ppPath, n + 1);
			memcpy(*ppPath, _Plt.user_homedir, strlen(_Plt.user_homedir));
		}
		else
			IntWarn(NO_CARET, "HOME not set - cannot expand tilde");
	}
}

//static void init_memory()
void GnuPlot::InitMemory()
{
	ExtendInputLine();
	Pgm.ExtendTokenTable();
	Pgm.replot_line = sstrdup("");
}

#ifdef GNUPLOT_HISTORY
// 
// cancel_history() can be called by terminals that fork a helper process
// to make sure that the helper doesn't trash the history file on exit.
// 
void cancel_history()
{
	expanded_history_filename = NULL;
}

#if defined(HAVE_LIBREADLINE) || defined(HAVE_LIBEDITLINE)
	static void wrapper_for_write_history()
	{
		if(expanded_history_filename) {
			if(history_is_stifled())
				unstifle_history();
			if(GPO.Hist.HistorySize >= 0)
				stifle_history(GPO.Hist.HistorySize);
			// returns 0 on success 
			if(GPO.WriteHistory(expanded_history_filename))
				fprintf(stderr, "Warning:  Could not write history file!!!\n");
			unstifle_history();
		}
	}
#else /* HAVE_LIBREADLINE || HAVE_LIBEDITLINE */
	//
	// version for gnuplot's own write_history 
	//
	static void wrapper_for_write_history()
	{
		// What we really want to do is truncate(expanded_history_filename), but this is only available on BSD compatible systems 
		if(expanded_history_filename) {
			remove(expanded_history_filename);
			if(GPO.Hist.HistorySize < 0)
				GPO.WriteHistory(expanded_history_filename);
			else
				GPO.WriteHistoryN(GPO.Hist.HistorySize, expanded_history_filename, "w");
		}
	}
#endif /* HAVE_LIBREADLINE || HAVE_LIBEDITLINE */
#endif /* GNUPLOT_HISTORY */

//void restrict_popen()
void GnuPlot::RestrictPOpen()
{
	if(!_Plt.successful_initialization)
		IntError(NO_CARET, "Pipes and shell commands not permitted during initialization");
}
