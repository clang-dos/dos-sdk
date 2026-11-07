/* TCXL 5.5 compatible names over the CXL compatible library.
 *
 * TCXL 5.5 renamed most CXL 5.2 functions (its manual's Appendix H lists
 * the pairs). This header maps those names onto this library, adds the
 * TCXL types and constants, and a few TCXL-only functions. Functions that
 * return an error code return ERR_NUL (0) on success and ERR_ARG (-1) or
 * ERR_MEM (-2) on failure, with the detail in TcxlErr. TCXL's internal
 * records (WdfT, MdfT, ...) and the functions that take VposT positions
 * are not provided.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef __TCXL_H
#define __TCXL_H

#include "cxldef.h"
#include "cxlvid.h"
#include "cxlkey.h"
#include "cxlmou.h"
#include "cxlwin.h"
#include "cxlstr.h"
#include "cxlprn.h"
#include "cxlems.h"
#include "tcxlcod.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Types. */
#ifndef VOID
#define VOID void
#endif
#define NOARG void
#define CTYP
#define PAS
#define CDC
#define TYP typedef

typedef void *AnyP;
typedef char ChrT, *ChrP, **ChrI;
typedef char *StrP;
typedef unsigned char BytT, *BytP;
typedef unsigned char FlgT, *FlgP;
typedef int IntT, *IntP;
typedef unsigned WrdT, *WrdP;
typedef long LngT, *LngP;
typedef unsigned long DwdT, *DwdP;
typedef float FltT;
typedef double DblT;
typedef unsigned LenT;
typedef int ArgT;
typedef unsigned KeyT, *KeyP;
typedef unsigned char AtrT, *AtrP;
typedef int TagT, *TagP;
typedef int WndT, *WndP;
typedef int *VcelP;
typedef void (*VfvCP)(void);
typedef void (*VFvCP)(void);
typedef void (*VfiCP)(int);
typedef int (*IfcpCP)(char *);
typedef unsigned (*WfipCP)(int *);
typedef struct _onkey_t *KbndP;
typedef struct _wrec_t *WdfP;
typedef struct _menu_t *MdfP;
typedef struct _item_t *IdfP;
typedef struct _field_t *FdfP;
typedef struct _form_t *EdfP;
typedef struct _help_t *HdfP;

/* Return values. */
#define ERR_MEM (-2)
#define ERR_ARG (-1)
#define ERR_NUL 0

#define TcxlErr (_winfo.errno)
#define WctlAct (_winfo.active)
#define VidSeg  (_vinfo.videoseg)
#define VidPag  0
#define VidWid  (_vinfo.numcols)
#define VidDep  (_vinfo.numrows)

static inline IntT _TcxlRet(int e)
{
    return e == W_NOERROR ? ERR_NUL : e == W_ALLOCERR ? ERR_MEM : ERR_ARG;
}

/* Attributes. */
#ifndef _DGREY
#define _DGREY    0x80
#define _LBLUE    0x90
#define _LGREEN   0xa0
#define _LCYAN    0xb0
#define _LRED     0xc0
#define _LMAGENTA 0xd0
#define _YELLOW   0xe0
#define _WHITE    0xf0
#endif
#define INTENSE  0x08
#define _BLINK   0x80
#define MONO_N   0x07
#define MONO_I   0x0f
#define MONO_U   0x01
#define MONO_IU  0x09
#define MONO_R   0x70
#define MONO_NB  0x87
#define MONO_IB  0x8f
#define MONO_UB  0x81
#define MONO_IUB 0x89
#define MONO_RB  0xf0
#define MONO_INT 0x80

/* Menus. */
#define ITM_PUL M_HASPD
#define ITM_NSL M_NOSEL
#define ITM_AFT M_CLOSE
#define ITM_ALL M_CLALL
#define ITM_BEF M_CLOSB
#define ITM_DSP 0x20
#define MNU_HOR M_HORZ
#define MNU_VER M_VERT
#define MNU_OMN M_OMNI
#define MNU_PUL M_PD
#define MNU_NQS M_NOQS
#define MNU_SAV M_SAVE
#define MNU_DSP 0x40
#define MNU_USE 0x80

/* Entry fields: EntFld() feature bits and modes. */
#define FLD_NUL 0x0000
#define FLD_LWR 0x0001
#define FLD_UPR 0x0002
#define FLD_MIX 0x0004
#define FLD_PAS 0x0008
#define FLD_NUM 0x0010
#define FDF_INI 0x10
#define FDF_CHG 0x20
#define FDF_CND 0x40

/* Windows. */
#define BRD_TOP TP_BORD
#define BRD_BOT BT_BORD
#define BRD_LFT LT_BORD
#define BRD_RGT RT_BORD
#define BOX_SNG 0
#define BOX_DBL 1
#define BOX_VER 2
#define BOX_HOR 3
#define BOX_HVY 4
#define BOX_SPA 5
#define CNT_HOR 0x01
#define CNT_VER 0x02
#define CNT_CNT 0x03
#define DIR_DWN D_DOWN
#define DIR_UP  D_UP
#define DIR_LFT D_LEFT
#define DIR_RGT D_RIGHT
#define TTL_LFT TLEFT
#define TTL_CNT TCENTER
#define TTL_RGT TRIGHT

/* Video parameters. */
#define VP_BIO VP_BIOS
#define VP_MON VP_MONO
#define VP_COL VP_COLOR

/* Keyboard shift state. */
#define KEY_RSH RSHIFT
#define KEY_LSH LSHIFT
#define KEY_CTL CTRL
#define KEY_ALT ALT
#define KEY_SCR SCRLOCK
#define KEY_NUM NUMLOCK
#define KEY_CAP CAPSLOCK
#define KEY_INS INS

/* Machine IDs added by TCXL. */
#define IBMPS30 0xfa
#define IBMPS80 0xf8
#define CPQPORT 0x2d
#define CPQPLUS 0x9a
#define HP110   0xb6

/* Renamed CXL functions. */
#define Beep beep
#define BiosVer biosver
#define ClockCal clockcal
static inline IntT EntDef(int fieldattr, int textattr) { return _TcxlRet(winpbeg(fieldattr, textattr)); }
static inline IntT EntKey(unsigned (*getkey)(int *), unsigned *termkey) { return _TcxlRet(winpkey(getkey, termkey)); }
static inline IntT EntSep(char *sepstr) { return _TcxlRet(winpwordsep(sepstr)); }
static inline IntT EntShow(void) { return _TcxlRet(winpread()); }
#define FldCur winpfcurr
static inline IntT FldFun(void (*before)(void), void (*after)(void)) { return _TcxlRet(winpfba(before, after)); }
#define FmtChar cvaltype
#define HdwDrv numflop
#define HdwGam gameport
#define HdwNdp mathchip
#define HdwPar numpar
#define HdwSer numser
static inline IntT HlpAdd(int cat) { return _TcxlRet(whelppcat(cat)); }
static inline IntT HlpClr(void) { return _TcxlRet(whelpclr()); }
static inline IntT HlpCur(void) { return _TcxlRet(whelpush()); }
static inline IntT HlpDrop(void) { return _TcxlRet(whelpopc()); }
#define HlpOff whelpundef
static inline IntT HlpPop(void) { return _TcxlRet(whelpop()); }
static inline IntT HlpPush(int cat) { return _TcxlRet(whelpushc(cat)); }
static inline IntT HlpSet(int cat) { return _TcxlRet(whelpcat(cat)); }
static inline void HlpShow(void) { whelpget(); }
static inline IntT HlpWind(int srow, int scol, int erow, int ecol, int btype, int title) { return _TcxlRet(whelpwin(srow, scol, erow, ecol, btype, title)); }
#define ItmCur wmenuicurr
static inline IntT ItmDis(int tagid) { return _TcxlRet(wmenuidsab(tagid)); }
static inline IntT ItmDsp(int tagid) { return _TcxlRet(wmenuiredisp(tagid)); }
static inline IntT ItmEna(int tagid) { return _TcxlRet(wmenuienab(tagid)); }
static inline IntT ItmFun(void (*before)(void), void (*after)(void)) { return _TcxlRet(wmenuiba(before, after)); }
#define ItmGet wmenuigetxt
#define ItmLoc wmenuifind
static inline IntT ItmNxt(int tagid) { return _TcxlRet(wmenuinext(tagid)); }
static inline IntT ItmSet(int tagid, char *str) { return _TcxlRet(wmenuisetxt(tagid, str)); }
static inline IntT ItmTxt(int wrow, int wcol, int attr, char *str) { return _TcxlRet(wmenuitxt(wrow, wcol, attr, str)); }
#define KbndChg chgonkey
#define KbndFre freonkey
#define KbndIdle setkbloop
#define KbndSet setonkey
#define KcapOff capsoff
#define KcapOn capson
#define KeyClear clearkeys
#define KeyEvent kbmhit
#define KeyGetc getxch
#define KeyStat kbstat
#define KeyTime waitkeyt
#define KeyWait waitkey
#define KnumOff numoff
#define KnumOn numon
#define KqueClr kbclear
#define KquePut kbput
#define KqueStr kbputs
#define KscnCod scancode
#define KvGetCf getchf
#define KvGetFmt inputsf
#define KvGetSn getns
#define KwGetCf wgetchf
#define KwGetCh wgetc
static inline IntT KwGetFmt(char *str, char *fmt) { return _TcxlRet(winputsf(str, fmt)); }
static inline IntT KwGetSn(char *str, int maxchars) { return _TcxlRet(wgetns(str, maxchars)); }
static inline IntT KwGetSt(char *str) { return _TcxlRet(wgets(str)); }
#define KwGetYn wgetyn
#define KwScanf wscanf
#define MSclear msbclear
#define MScurs mscursor
#define MSgoto msgotoxy
#define MShide mshidecur
#define MSinit msinit
#define MSpush mssupport
#define MSshow msshowcur
#define MSspeed msspeed
#define MSupdat mscondoff
static inline IntT MnuAct(void) { return _TcxlRet(wmenubegc()); }
static inline IntT MnuBeg(int srow, int scol, int erow, int ecol, int btype, int battr, int wattr, void (*open)(void)) { return _TcxlRet(wmenubeg(srow, scol, erow, ecol, btype, battr, wattr, open)); }
#define MnuCur wmenumcurr
static inline IntT MnuEnd(int taginit, int menutype, int barwidth, int textpos, int textattr, int scharattr, int noselattr, int barattr) { return _TcxlRet(wmenuend(taginit, menutype, barwidth, textpos, textattr, scharattr, noselattr, barattr)); }
static inline IntT MnuItm(int wrow, int wcol, char *str, int schar, int tagid, int fmask, void (*select)(void), unsigned hotkey, int help) { return _TcxlRet(wmenuitem(wrow, wcol, str, schar, tagid, fmask, select, hotkey, help)); }
#define MnuShow wmenuget
#define RandFile randfile
#define SelAttr wpickattr
#define SelFile wpickfile
#define SelStr wpickstr
#define SelTxt wselstr
#define Sound sound_
#define SysDate sysdate
#define SysTime systime
#define TabNxt tabstop
#define TcxlVer cxlver
#define VatrInv revsattr
#define VatrMap mapattr
#define VatrRev revattr
#define VatrSet setattr
#define VatrTxt attr2text
#define Vattrib attrib
#define Vclear cclrscrn
#define VclrBox clrwin
#define VclrEol clreol_
#define VclrScr clrscrn
#define Vgotoxy gotoxy_
#define VidInit videoinit
#define VidParm setvparam
#define VidType vidtype
#define VlodBox disktowin
#define VlodScr disktoscrn
#define VposGet readcur
#define Vprintc printc
#define Vprints prints
#define VputSpa spc
#define Vrestore srestore
#define VsavScr scrntodisk
#define Vsave ssave
#define VsetLin setlines
static inline IntT Wactiv(WINDOW whandle) { return _TcxlRet(wactiv(whandle)); }
static inline IntT Wborder(int btype) { return _TcxlRet(wborder(btype)); }
static inline IntT Wbprintc(int bord, int offs, int attr, int ch) { return _TcxlRet(wbprintc(bord, offs, attr, ch)); }
static inline IntT WchgAtr(int battr, int wattr) { return _TcxlRet(wchgattr(battr, wattr)); }
#define WchkBox wchkbox
#define WchkCol wchkcol
#define WchkPos wchkcoord
#define WchkRow wchkrow
#define Wclear wclear
static inline IntT Wclose(void) { return _TcxlRet(wclose()); }
static inline IntT WcloseAll(void) { return _TcxlRet(wcloseall()); }
static inline IntT WclrEol(void) { return _TcxlRet(wclreol()); }
static inline IntT WclrEos(void) { return _TcxlRet(wclreos()); }
static inline IntT WclrWin(int attr) { return _TcxlRet(wcclear(attr)); }
#define Wcopy wcopy
static inline IntT Wdrag(int direction) { return _TcxlRet(wdrag(direction)); }
static inline IntT WdrwBox(int wsrow, int wscol, int werow, int wecol, int btype, int attr) { return _TcxlRet(wbox(wsrow, wscol, werow, wecol, btype, attr)); }
static inline IntT WdrwHor(int wsrow, int wscol, int count, int btype, int attr) { return _TcxlRet(whline(wsrow, wscol, count, btype, attr)); }
static inline IntT WdrwVer(int wsrow, int wscol, int count, int btype, int attr) { return _TcxlRet(wvline(wsrow, wscol, count, btype, attr)); }
static inline IntT Wdump(void) { return _TcxlRet(wdump()); }
static inline IntT WdupChr(int ch, int count) { return _TcxlRet(wdupc(ch, count)); }
static inline IntT WdupStr(char *str, int count) { return _TcxlRet(wdups(str, count)); }
#define Werrmsg werrmsg
static inline IntT WfilBox(int wsrow, int wscol, int werow, int wecol, int ch, int attr) { return _TcxlRet(wfill(wsrow, wscol, werow, wecol, ch, attr)); }
static inline IntT Wgotoxy(int wrow, int wcol) { return _TcxlRet(wgotoxy(wrow, wcol)); }
#define Whandle whandle
static inline IntT Whide(void) { return _TcxlRet(whide()); }
#define WinLoc wfindrec
#define Windowat windowat
#define Wisactiv wisactiv
static inline IntT WlinDel(int wrow, int direc) { return _TcxlRet(wdelline(wrow, direc)); }
static inline IntT WlinIns(int wrow, int direc) { return _TcxlRet(winsline(wrow, direc)); }
static inline IntT Wmessage(char *str, int border, int leftofs, int attr) { return _TcxlRet(wmessage(str, border, leftofs, attr)); }
static inline IntT Wmove(int nsrow, int nscol) { return _TcxlRet(wmove(nsrow, nscol)); }
#define Wopen wopen
static inline IntT Wperror(char *message) { return _TcxlRet(wperror(message)); }
static inline IntT Wpgotoxy(int wrow, int wcol) { return _TcxlRet(wpgotoxy(wrow, wcol)); }
static inline IntT WprtCen(int wrow, int attr, char *str) { return _TcxlRet(wcenters(wrow, attr, str)); }
static inline IntT WprtFmt(int wrow, int wcol, int attr, char *format, char *str) { return _TcxlRet(wprintsf(wrow, wcol, attr, format, str)); }
static inline IntT Wprtc(int wrow, int wcol, int attr, int ch) { return _TcxlRet(wprintc(wrow, wcol, attr, ch)); }
#define Wprtf wprintf
static inline IntT Wprts(int wrow, int wcol, int attr, char *str) { return _TcxlRet(wprints(wrow, wcol, attr, str)); }
static inline IntT Wputc(int ch) { return _TcxlRet(wputc(ch)); }
static inline IntT Wputns(char *str, int width) { return _TcxlRet(wputns(str, width)); }
static inline IntT Wputs(char *str) { return _TcxlRet(wputs(str)); }
static inline IntT Wputsw(char *str) { return _TcxlRet(wputsw(str)); }
#define Wrestore wrestore
#define Wsave wsave
static inline IntT WscrBox(int wsrow, int wscol, int werow, int wecol, int count, int direction) { return _TcxlRet(wscrollbox(wsrow, wscol, werow, wecol, count, direction)); }
static inline IntT WscrWin(int count, int direc) { return _TcxlRet(wscroll(count, direc)); }
#define WsetEsc wsetesc
#define WsetFil wfillch
#define WsetFun wsetfill
#define WsetTab wtabwidth
static inline IntT WsetTxt(int attr) { return _TcxlRet(wtextattr(attr)); }
static inline IntT Wshadoff(void) { return _TcxlRet(wshadoff()); }
static inline IntT Wshadow(int attr) { return _TcxlRet(wshadow(attr)); }
static inline IntT Wsize(int nerow, int necol) { return _TcxlRet(wsize(nerow, necol)); }
static inline IntT Wslide(int nsrow, int nscol) { return _TcxlRet(wslide(nsrow, nscol)); }
static inline IntT Wtitle(char *str, int tpos, int tattr) { return _TcxlRet(wtitle(str, tpos, tattr)); }
static inline IntT Wunhide(WINDOW whandle) { return _TcxlRet(wunhide(whandle)); }
static inline IntT Wunlink(WINDOW whandle) { return _TcxlRet(wunlink(whandle)); }
#define _Delay delay_
#define _StdOut _stdoutch
#define _Timer timer
static inline IntT HlpDef(char *file, unsigned key, int winattr, int textattr, int selattr, int barattr, void (*open)(void)) { return _TcxlRet(whelpdef(file, key, winattr, textattr, selattr, barattr, open)); }
#define WerrMsg werrmsg
#define Wputf   wprintf
#define wputf   wprintf
#define MSlimit(sr, sc, er, ec) (msvbounds((sr), (er)), mshbounds((sc), (ec)))
#define VidMode(m) mode(m)

/* TCXL-only functions. */
WndT TcxlPopUp_(int cf, int sr, int sc, int er, int ec, int bt, int ba, int wa);
#define WpopUp TcxlPopUp_
IntT Wcenter(IntT cf);
void Werror(void);
IntT WputCen(ChrP ps);
IntT Wputrj(ChrP ps);
IntT Wprtrj(IntT wr, IntT wc, IntT at, ChrP ps);
void Wprtns(IntT wr, IntT wc, IntT at, ChrP ps, IntT wd);
IntT Wprtsw(IntT wr, IntT wc, IntT at, ChrP ps);
IntT wprtf(IntT wr, IntT wc, IntT at, ChrP fs, ...) __attribute__((__format__(__printf__, 4, 5)));
void WdupChrA(IntT wr, IntT wc, IntT at, IntT ch, IntT nd);
void WdupStrA(IntT wr, IntT wc, IntT at, ChrP ps, IntT nd);
IntT VcurHid(IntT s);
void VcurSet(IntT s);
void TcxlInit(void);
void TcxlTerm(void);
IntT EntFld(IntT wr, IntT wc, ChrP ps, ChrP fs, IntT fm, TagT id, IntT md,
            IfcpCP vf, TagT ht);
FdfP FldLoc(TagT id);

#ifdef __cplusplus
}
#endif

#endif /* __TCXL_H */
