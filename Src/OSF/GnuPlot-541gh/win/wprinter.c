// GNUPLOT - win/wprinter.c 
// Copyright 1992, 1993, 1998, 2004   Maurice Castro, Russell Lang
// Dump a file to the printer 
//
#include <gnuplot.h>
#pragma hdrstop
#define STRICT
#include <initguid.h>
#include <windowsx.h>
#include <unknwn.h>
#include <commdlg.h>
#include <commctrl.h>
#include <ocidl.h>
#include <tchar.h>
#include "wgnuplib.h"
#include "wresourc.h"
#include "wcommon.h"

static GP_LPPRINT prlist = NULL;
HGLOBAL hDevNames = NULL;
HGLOBAL hDevMode = NULL;

static GP_LPPRINT PrintFind(HDC hdc);
// 
// COM object for PrintDlgEx callbacks, implemented in C
// 
typedef struct {
#ifdef __cplusplus
	IPrintDialogCallback * callback;
	IObjectWithSite * site;
#else
	IPrintDialogCallback callback;
	IObjectWithSite site;
#endif
	IUnknown * pUnkSite_;
	IPrintDialogServices * services_;
	GP_LPPRINT lpr_;
} PrintingCallbackHandler;

// IUnknown
HRESULT STDMETHODCALLTYPE QueryInterface(IPrintDialogCallback * This, REFIID riid, void ** object)
{
#ifdef __cplusplus
	#define _is_eq_iid(iid1, iid2) IsEqualIID((iid1), (iid2))
#else
	#define _is_eq_iid(iid1, iid2) IsEqualIID((iid1), &(iid2))
#endif
	if(_is_eq_iid(riid, IID_IUnknown)) {
		*object = (void*)This;
	}
	else if(_is_eq_iid(riid, IID_IPrintDialogCallback)) {
		*object = (void*)&((PrintingCallbackHandler*)This)->callback;
	}
	else if(_is_eq_iid(riid, IID_IObjectWithSite)) {
		*object = (void*)&((PrintingCallbackHandler*)This)->site;
	}
	else {
		return E_NOINTERFACE;
	}
#undef _is_eq_iid
	return S_OK;
}

static ULONG STDMETHODCALLTYPE AddRef(IPrintDialogCallback * This)
{
	return 1;
}

static ULONG STDMETHODCALLTYPE Release(IPrintDialogCallback * This)
{
	return 1;
}

// IPrintDialogCallback
static HRESULT STDMETHODCALLTYPE InitDone(IPrintDialogCallback * This)
{
	/* the general page is initialised, but not shown yet */
	PrintingCallbackHandler * Base = (PrintingCallbackHandler*)((char*)This - offsetof(PrintingCallbackHandler, callback));
#if 0
	IPrintDialogServices * services = Base->services_;
	if(services != NULL) {
	}
#endif
	Base->lpr_->bDriverChanged = TRUE;
	/* always return false to enable default actions */
	return S_FALSE;
}

static HRESULT STDMETHODCALLTYPE SelectionChange(IPrintDialogCallback * This)
{
	/* the user has selected a different printer */
	PrintingCallbackHandler * Base = (PrintingCallbackHandler*)((char*)This - offsetof(PrintingCallbackHandler, callback));
#if 0
	IPrintDialogServices * services = Base->services_;
	if(services != NULL) {
	}
#endif
	Base->lpr_->bDriverChanged = TRUE;
	/* always return false to enable default actions */
	return S_FALSE;
}

static HRESULT STDMETHODCALLTYPE HandleMessage(IPrintDialogCallback * This, HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT * pResult)
{
	// WM_INITDIALOG: lParam points to PRINTDLGEX struct
	/* enable default message handling */
	return S_FALSE;
}

static HRESULT STDMETHODCALLTYPE SetSite(IObjectWithSite * This, IUnknown * pUnkSite)
{
	PrintingCallbackHandler * Base = (PrintingCallbackHandler*)((char*)This - offsetof(PrintingCallbackHandler, site));
#ifdef __cplusplus
	if(Base->pUnkSite_) {
		Base->pUnkSite_->Release();
		Base->pUnkSite_ = NULL;
	}
	if(pUnkSite == NULL) {
		if(Base->services_) {
			Base->services_->Release();
			Base->services_ = NULL;
		}
	}
	else {
		Base->pUnkSite_ = pUnkSite;
		pUnkSite->AddRef();
		if(Base->services_ == NULL) {
			pUnkSite->QueryInterface(IID_IPrintDialogServices, (void**)&(Base->services_));
			Base->lpr_->services = Base->services_;
		}
	}
#else
	if(Base->pUnkSite_) {
		Base->pUnkSite_->lpVtbl->Release(Base->pUnkSite_);
		Base->pUnkSite_ = NULL;
	}
	if(pUnkSite == NULL) {
		if(Base->services_) {
			Base->services_->lpVtbl->Release(Base->services_);
			Base->services_ = NULL;
		}
	}
	else {
		Base->pUnkSite_ = pUnkSite;
		pUnkSite->lpVtbl->AddRef(pUnkSite);
		if(Base->services_ == NULL) {
			pUnkSite->lpVtbl->QueryInterface(pUnkSite, &IID_IPrintDialogServices, (void**)&(Base->services_));
			Base->lpr_->services = Base->services_;
		}
	}
#endif
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE GetSite(IObjectWithSite * This, REFIID riid, void ** ppvSite)
{
	/* according to documentation this _must_ be implemented */
	PrintingCallbackHandler * Base = (PrintingCallbackHandler*)((char*)This - offsetof(PrintingCallbackHandler, site));
	*ppvSite = Base->pUnkSite_;
	return (Base->pUnkSite_ != NULL) ? S_OK : E_FAIL;
}

#ifndef __cplusplus
	static IPrintDialogCallbackVtbl IPrintDialogCallback_Vtbl = {
		QueryInterface, AddRef, Release, InitDone, SelectionChange, HandleMessage
	};

	static IObjectWithSiteVtbl IObjectWithSite_Vtbl = {
		(HRESULT(STDMETHODCALLTYPE *)(IObjectWithSite *, const IID *const, void **))QueryInterface,
		(ULONG(STDMETHODCALLTYPE *)(IObjectWithSite *))AddRef,
		(ULONG(STDMETHODCALLTYPE *)(IObjectWithSite *))Release,
		SetSite,
		GetSite
	};
#endif // __cplusplus

static void PrintingCallbackInit(PrintingCallbackHandler * This, GP_LPPRINT lpr)
{
	memzero(This, sizeof(PrintingCallbackHandler));
#ifndef __cplusplus
	This->callback.lpVtbl = &IPrintDialogCallback_Vtbl;
	This->site.lpVtbl = &IObjectWithSite_Vtbl;
#endif
	This->lpr_ = lpr;
}

static void PrintingCallbackFini(PrintingCallbackHandler * This)
{
	if(This->services_) {
#ifdef __cplusplus
		This->services_->Release();
#else
		This->services_->lpVtbl->Release(This->services_);
#endif
	}
}

void * PrintingCallbackCreate(GP_LPPRINT lpr)
{
	PrintingCallbackHandler * callback = (PrintingCallbackHandler*)malloc(sizeof(PrintingCallbackHandler));
	/* initialize COM object for the printing dialog callback */
	PrintingCallbackInit(callback, lpr);
	return callback;
}

void PrintingCallbackFree(void * callback)
{
	PrintingCallbackFini((PrintingCallbackHandler*)callback);
	SAlloc::F(callback);
}

void PrintingCleanup(void)
{
	if(hDevNames) GlobalFree(hDevNames);
	if(hDevMode) GlobalFree(hDevMode);
}

INT_PTR CALLBACK PrintSizeDlgProc(HWND hdlg, UINT wmsg, WPARAM wparam, LPARAM lparam)
{
	TCHAR buf[8];
	HWND hPropSheetDlg = GetParent(hdlg);
	GP_LPPRINT lpr = (GP_LPPRINT)GetWindowLongPtr(hdlg, GWLP_USERDATA);
	switch(wmsg) {
		case WM_INITDIALOG:
		    lpr = (GP_LPPRINT)((PROPSHEETPAGE*)lparam)->lParam;
		    SetWindowLongPtr(hdlg, GWLP_USERDATA, (LONG_PTR)lpr);
		    wsprintf(buf, TEXT("%d"), lpr->pdef.x);
		    SetDlgItemText(hdlg, PSIZE_DEFX, buf);
		    wsprintf(buf, TEXT("%d"), lpr->pdef.y);
		    SetDlgItemText(hdlg, PSIZE_DEFY, buf);
		    wsprintf(buf, TEXT("%d"), lpr->poff.x);
		    SetDlgItemText(hdlg, PSIZE_OFFX, buf);
		    wsprintf(buf, TEXT("%d"), lpr->poff.y);
		    SetDlgItemText(hdlg, PSIZE_OFFY, buf);
		    wsprintf(buf, TEXT("%d"), lpr->psize.x);
		    SetDlgItemText(hdlg, PSIZE_X, buf);
		    wsprintf(buf, TEXT("%d"), lpr->psize.y);
		    SetDlgItemText(hdlg, PSIZE_Y, buf);
		    CheckDlgButton(hdlg, PSIZE_DEF, TRUE);
		    EnableWindow(GetDlgItem(hdlg, PSIZE_X), FALSE);
		    EnableWindow(GetDlgItem(hdlg, PSIZE_Y), FALSE);
		    return TRUE;
		case WM_COMMAND:
		    switch(LOWORD(wparam)) {
			    case PSIZE_DEF:
				if(HIWORD(wparam) == BN_CLICKED) {
					EnableWindow(GetDlgItem(hdlg, PSIZE_X), FALSE);
					EnableWindow(GetDlgItem(hdlg, PSIZE_Y), FALSE);
					PropSheet_Changed(hPropSheetDlg, hdlg);
				}
				return FALSE;
			    case PSIZE_OTHER:
				if(HIWORD(wparam) == BN_CLICKED) {
					EnableWindow(GetDlgItem(hdlg, PSIZE_X), TRUE);
					EnableWindow(GetDlgItem(hdlg, PSIZE_Y), TRUE);
					PropSheet_Changed(hPropSheetDlg, hdlg);
				}
				return FALSE;
			    case PSIZE_X:
			    case PSIZE_Y:
			    case PSIZE_OFFX:
			    case PSIZE_OFFY:
				if(HIWORD(wparam) == EN_UPDATE)
					PropSheet_Changed(hPropSheetDlg, hdlg);
				return FALSE;
		    } /* switch (wparam) */
		    break;
		case WM_NOTIFY:
		    switch(((LPNMHDR)lparam)->code) {
			    case PSN_APPLY: /* apply changes */
				/* FIXME: Need to check for valid input.
				 */
				if(SendDlgItemMessage(hdlg, PSIZE_OTHER, BM_GETCHECK, 0, 0L)) {
					SendDlgItemMessage(hdlg, PSIZE_X, WM_GETTEXT, 7, (LPARAM)buf);
					GetInt(buf, (LPINT)&lpr->psize.x);
					SendDlgItemMessage(hdlg, PSIZE_Y, WM_GETTEXT, 7, (LPARAM)buf);
					GetInt(buf, (LPINT)&lpr->psize.y);
				}
				else {
					lpr->psize.x = lpr->pdef.x;
					lpr->psize.y = lpr->pdef.y;
				}
				SendDlgItemMessage(hdlg, PSIZE_OFFX, WM_GETTEXT, 7, (LPARAM)buf);
				GetInt(buf, (LPINT)&lpr->poff.x);
				SendDlgItemMessage(hdlg, PSIZE_OFFY, WM_GETTEXT, 7, (LPARAM)buf);
				GetInt(buf, (LPINT)&lpr->poff.y);
				if(lpr->psize.x <= 0)
					lpr->psize.x = lpr->pdef.x;
				if(lpr->psize.y <= 0)
					lpr->psize.y = lpr->pdef.y;
				PropSheet_UnChanged(hPropSheetDlg, hdlg);
				SetWindowLongPtr(hdlg, DWLP_MSGRESULT, PSNRET_NOERROR);
				return TRUE;
		    }
		case PSN_SETACTIVE: /* display: initialize according to printer */
		    if(lpr->psize.x < 0 || lpr->bDriverChanged) {
			    /* FIXME: also if settings changed (paper size, orientation) */
			    IPrintDialogServices * services = (IPrintDialogServices*)lpr->services;
			    /* Set size to full paper size of current printer */
			    if(services) {
				    LPWSTR lpPrinterName = NULL;
				    LPWSTR lpPortName = NULL;
				    LPDEVMODE lpDevMode = NULL;
				    UINT size = 0;
				    /* Note:  The Windows 8.1 SDK says that these functions expect LPWSTR
				              arguments, in contrast to the MSDN documentation, MinGW, and
				              what was actually seen in a debugger on Windows 10.
				              So warnings about type mismatch can be safely ignored.
				     */
#ifdef __cplusplus
				    HRESULT hr = services->GetCurrentPrinterName(NULL, &size);
				    if(SUCCEEDED(hr) && size > 0) {
					    lpPrinterName = (LPWSTR)malloc(size * sizeof(WCHAR));
					    hr = services->GetCurrentPrinterName(lpPrinterName, &size);
				    }
				    size = 0;
				    hr = services->GetCurrentPortName(NULL, &size);
				    if(SUCCEEDED(hr) && size > 0) {
					    lpPortName = (LPWSTR)malloc(size * sizeof(WCHAR));
					    hr = services->GetCurrentPortName(lpPortName, &size);
				    }
				    size = 0;
				    hr = services->GetCurrentDevMode(NULL, &size);
				    if(SUCCEEDED(hr) && size > 0) {
					    lpDevMode = (LPDEVMODE)malloc(size);
					    hr = services->GetCurrentDevMode(lpDevMode, &size);
				    }
#else
				    HRESULT hr = services->lpVtbl->GetCurrentPrinterName(services, NULL, &size);
				    if(SUCCEEDED(hr) && size > 0) {
					    lpPrinterName = (LPTSTR)malloc(size * sizeof(TCHAR));
					    hr = services->lpVtbl->GetCurrentPrinterName(services, lpPrinterName, &size);
				    }
				    size = 0;
				    hr = services->lpVtbl->GetCurrentPortName(services, NULL, &size);
				    if(SUCCEEDED(hr) && size > 0) {
					    lpPortName = (LPTSTR)malloc(size * sizeof(TCHAR));
					    hr = services->lpVtbl->GetCurrentPortName(services, lpPortName, &size);
				    }
				    size = 0;
				    hr = services->lpVtbl->GetCurrentDevMode(services, NULL, &size);
				    if(SUCCEEDED(hr) && size > 0) {
					    lpDevMode = (LPDEVMODE)malloc(size * sizeof(TCHAR));
					    hr = services->lpVtbl->GetCurrentDevMode(services, lpDevMode, &size);
				    }
#endif
				    if(SUCCEEDED(hr) && size > 0 && lpPortName != NULL && lpPrinterName != NULL) {
					    HDC printer = CreateDCW(L"WINSPOOL", lpPrinterName, lpPortName, reinterpret_cast<const DEVMODEW *>(lpDevMode));
					    lpr->psize.x = GetDeviceCaps(printer, HORZSIZE);
					    lpr->psize.y = GetDeviceCaps(printer, VERTSIZE);
					    DeleteDC(printer);
				    }
				    SAlloc::F(lpPrinterName);
				    SAlloc::F(lpPortName);
				    SAlloc::F(lpDevMode);
			    }
		    }
		    if(lpr->psize.x < 0) {
			    /* something went wrong */
			    lpr->psize.x = lpr->pdef.x;
			    lpr->psize.y = lpr->pdef.y;
		    }
		    /* apply changes */
		    wsprintf(buf, TEXT("%d"), lpr->psize.x);
		    SetDlgItemText(hdlg, PSIZE_X, buf);
		    wsprintf(buf, TEXT("%d"), lpr->psize.y);
		    SetDlgItemText(hdlg, PSIZE_Y, buf);
		    lpr->bDriverChanged = FALSE;

		    SetWindowLongPtr(hdlg, DWLP_MSGRESULT, 0); /* accept activation */
		    return TRUE;
		    break;
	} /* switch (msg) */
	return FALSE;
}

/* Win32 doesn't support OpenJob() etc. so we must use some old code
 * which attempts to sneak the output through a Windows printer driver */
void PrintRegister(GP_LPPRINT lpr)
{
	GP_LPPRINT next = prlist;
	prlist = lpr;
	lpr->next = next;
}

static GP_LPPRINT PrintFind(HDC hdc)
{
	GP_LPPRINT current = prlist;
	while(current && (current->hdcPrn != hdc)) {
		current = current->next;
	}
	return current;
}

void PrintUnregister(GP_LPPRINT lpr)
{
	GP_LPPRINT prev = NULL;
	GP_LPPRINT current = prlist;
	while(current && (current != lpr)) {
		prev = current;
		current = current->next;
	}
	if(current && (current == lpr)) {
		/* unhook it */
		if(prev)
			prev->next = current->next;
		else
			prlist = current->next;
	}
}

INT_PTR CALLBACK PrintDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	GP_LPPRINT lpr = (GP_LPPRINT)GetWindowLongPtr(hDlg, GWLP_USERDATA);
	switch(message) {
		case WM_INITDIALOG:
		    lpr = (GP_LPPRINT)lParam;
		    lpr->hDlgPrint = hDlg;
		    SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)lpr);
		    SetWindowText(hDlg, lpr->szTitle);
		    EnableMenuItem(GetSystemMenu(hDlg, FALSE), SC_CLOSE, MF_GRAYED);
		    SetFocus(GetDlgItem(hDlg, IDCANCEL));
		    return TRUE;
		case WM_COMMAND:
		    lpr->bUserAbort = TRUE;
		    lpr->hDlgPrint = 0;
		    EnableWindow(GetParent(hDlg), TRUE);
		    EndDialog(hDlg, FALSE);
		    return TRUE;
	}
	return FALSE;
}

BOOL CALLBACK PrintAbortProc(HDC hdcPrn, int code)
{
	MSG msg;
	GP_LPPRINT lpr = PrintFind(hdcPrn);
	while(!lpr->bUserAbort && PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
		if(!lpr->hDlgPrint || !IsDialogMessage(lpr->hDlgPrint, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return !lpr->bUserAbort;
}

void DumpPrinter(HWND hwnd, LPTSTR szAppName, LPTSTR szFileName)
{
	PRINTDLGEX pd;
	DEVNAMES * pDevNames;
	LPCTSTR szDevice;
	GP_PRINT pr;
	HANDLE printer;
	DOC_INFO_1 di;
	DWORD jobid;
	HRESULT hr;
	LPSTR buf;
	int count;
	FILE * f;
	long lsize;
	long ldone;
	TCHAR pcdone[10];
	if((f = _tfopen(szFileName, TEXT("rb"))) == NULL)
		return;
	fseek(f, 0L, SEEK_END);
	lsize = static_cast<long>(ftell(f));
	if(lsize <= 0)
		lsize = 1;
	fseek(f, 0L, SEEK_SET);
	ldone = 0;

	/* Print Property Sheet  */
	/* See http://support.microsoft.com/kb/240082 */
	memzero(&pd, sizeof(pd));
	pd.lStructSize = sizeof(pd);
	pd.hwndOwner = hwnd;
	pd.Flags = PD_NOPAGENUMS | PD_NOSELECTION | PD_NOCURRENTPAGE | PD_USEDEVMODECOPIESANDCOLLATE;
	pd.hDevNames = hDevNames;
	pd.hDevMode = hDevMode;
	pd.hDevNames = NULL;
	pd.hDevMode = NULL;
	pd.nCopies = 1;
	pd.nStartPage = START_PAGE_GENERAL;

	/* Replace the additional options in the lower part of the dialog with
	 * a hint to change print options via terminal options.
	 */
	pd.lpPrintTemplateName = TEXT("PrintDlgExSelect");
	pd.hInstance = graphwin->hInstance;
	pd.Flags |= PD_ENABLEPRINTTEMPLATE;

	if((hr = PrintDlgEx(&pd)) != S_OK) {
		DWORD error = CommDlgExtendedError();
		fprintf(stderr, "\nError:  Opening the print dialog failed with error code %04x (%04x).\n", hr, error);
	}

	if(pd.dwResultAction == PD_RESULT_PRINT) {
		pDevNames = (DEVNAMES*)GlobalLock(pd.hDevNames);
		szDevice = (LPCTSTR)pDevNames + pDevNames->wDeviceOffset;
		if(!OpenPrinter((LPTSTR)szDevice, &printer, NULL))
			printer = NULL;
		GlobalUnlock(pd.hDevNames);
		/* We no longer free these structures, but preserve them for the next time
		   GlobalFree(pd.hDevMode);
		   GlobalFree(pd.hDevNames);
		 */
		hDevNames = pd.hDevNames;
		hDevMode = pd.hDevMode;

		if(printer == NULL)
			return; /* abort */

		pr.hdcPrn = (HDC)printer;
		PrintRegister(&pr);
		if((buf = (LPSTR)malloc(4096)) != NULL) {
			EnableWindow(hwnd, FALSE);
			pr.bUserAbort = FALSE;
			pr.szTitle = szAppName;
			pr.hDlgPrint = CreateDialogParam(hdllInstance, TEXT("CancelDlgBox"), hwnd, PrintDlgProc, (LPARAM)&pr);
			SendMessage(GetDlgItem(pr.hDlgPrint, CANCEL_PROGRESS), PBM_SETRANGE32, 0, lsize);
			di.pDocName = szAppName;
			di.pOutputFile = NULL;
			di.pDatatype = TEXT("RAW");
			if((jobid = StartDocPrinter(printer, 1, (LPBYTE)&di)) > 0) {
				while(pr.hDlgPrint && !pr.bUserAbort && (count = fread(buf, 1, 4096, f)) != 0) {
					DWORD dwBytesWritten;
					int ret = WritePrinter(printer, buf, count, &dwBytesWritten);
					ldone += count;
					if(dwBytesWritten > 0) {
						wsprintf(pcdone, TEXT("%d%% done"), (int)(ldone * 100 / lsize));
						SetWindowText(GetDlgItem(pr.hDlgPrint, CANCEL_PCDONE), pcdone);
						SendMessage(GetDlgItem(pr.hDlgPrint, CANCEL_PROGRESS), PBM_SETPOS, ldone, 0);
					}
					else if(ret == 0) {
						SetWindowText(GetDlgItem(pr.hDlgPrint, CANCEL_PCDONE), TEXT("Error writing to printer!"));
						pr.bUserAbort  = TRUE;
					}
					/* handle window messages */
					PrintAbortProc((HDC)printer, 0);
				}
				if(pr.bUserAbort) {
					if(SetJob(printer, jobid, 0, NULL, JOB_CONTROL_DELETE) == 0) {
						SetWindowText(GetDlgItem(pr.hDlgPrint, CANCEL_PCDONE), TEXT("Error: Failed to cancel print job!"));
						fprintf(stderr, "Error: Failed to cancel print job!\n");
					}
				}
				EndDocPrinter(printer);
				if(!pr.bUserAbort) {
					EnableWindow(hwnd, TRUE);
					DestroyWindow(pr.hDlgPrint);
				}
				SAlloc::F(buf);
			}
		}
		ClosePrinter(printer);
		PrintUnregister(&pr);
	}
	fclose(f);
}