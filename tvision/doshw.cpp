/*------------------------------------------------------------*/
/* filename -       doshw.cpp                                 */
/*                                                            */
/* function(s)                                                */
/*                  THardwareInfo for 16-bit real-mode DOS    */
/*                  (clang-dos)                               */
/*------------------------------------------------------------*/

// Copyright (c) 2026 clang-retro contributors
// SPDX-License-Identifier: AGPL-3.0-only

// The 32-bit (__FLAT__) THardwareInfo interface on the PC BIOS: the screen
// is the text mode of the video BIOS, written through a buffer in memory,
// the keyboard is read with INT 16h and the mouse with INT 33h. Ctrl+Break
// and Ctrl+C (INT 1Bh and INT 23h) set TSystemError::ctrlBreakHit, and
// critical errors (INT 24h) fail the DOS call instead of asking the user.

#define Uses_TKeys
#define Uses_TEvent
#define Uses_TScreen
#define Uses_THardwareInfo
#define Uses_TSystemError
#include <tvision/tv.h>

#if defined( __TV_CLANG_DOS__ )

#include <dos.h>
#include <i86.h>

ushort THardwareInfo::caretSize = 0;
Boolean THardwareInfo::caretVisible = False;
Boolean THardwareInfo::mouseVisible = False;
short THardwareInfo::mouseX = -1;
short THardwareInfo::mouseY = -1;

static uchar buttons;           // Mouse buttons, 0 with no mouse.
static Boolean mouseChecked;
static Boolean enhancedKeyboard;
static void (__interrupt __far *oldInt1B)();
static void (__interrupt __far *oldInt23)();
static void (__interrupt __far *oldInt24)();

static inline volatile uchar __far *biosByte( ushort offset ) noexcept
{
    return (volatile uchar __far *) MK_FP( 0x40, offset );
}

static inline volatile ushort __far *biosWord( ushort offset ) noexcept
{
    return (volatile ushort __far *) MK_FP( 0x40, offset );
}

static inline void videoInt( union REGS &r ) noexcept
{
    int86( 0x10, &r, &r );
}

static inline void mouseInt( union REGS &r ) noexcept
{
    int86( 0x33, &r, &r );
}

THardwareInfo::THardwareInfo() noexcept
{
    // Use INT 16h functions 10h and 11h when the BIOS reports an enhanced
    // (101-key) keyboard, for F11, F12 and the grey keys.
    enhancedKeyboard = Boolean( (*biosByte( 0x96 ) & 0x10) != 0 );
}

THardwareInfo::~THardwareInfo()
{
    setCtrlBrkHandler( False );
    setCritErrorHandler( False );
}

uint32_t THardwareInfo::getTickCount() noexcept
{
    // The BIOS timer count, 18.2 ticks a second.
    disable();
    uint32_t ticks = *(volatile uint32_t __far *) MK_FP( 0x40, 0x6C );
    enable();
    return ticks;
}

uint32_t THardwareInfo::getTickCountMs()
{
    return getTickCount() * 55;
}

// BIOS data.

uchar THardwareInfo::getShiftState() noexcept
{
    return *biosByte( 0x17 );
}

ushort THardwareInfo::getBiosEquipmentFlag() noexcept
{
    return *biosWord( 0x10 );
}

void THardwareInfo::setBiosEquipmentFlag( ushort flag ) noexcept
{
    *biosWord( 0x10 ) = flag;
}

uchar THardwareInfo::getBiosVideoInfo() noexcept
{
    return *biosByte( 0x87 );
}

void THardwareInfo::setBiosVideoInfo( uchar info ) noexcept
{
    *biosByte( 0x87 ) = info;
}

Boolean THardwareInfo::isEGAorVGA() noexcept
{
    union REGS r;
    r.h.ah = 0x12;
    r.h.bl = 0x10;
    videoInt( r );
    return Boolean( r.h.bl != 0x10 );
}

// The height of a character cell in scan lines, which the cursor shape is
// measured in.
static uchar charHeight() noexcept
{
    union REGS r;
    r.h.ah = 0x12;
    r.h.bl = 0x10;
    int86( 0x10, &r, &r );
    if( r.h.bl != 0x10 )                        // EGA or VGA
        {
        r.x.ax = 0x1130;
        r.h.bh = 0;
        int86( 0x10, &r, &r );
        if( r.h.cl != 0 )
            return r.h.cl;
        }
    r.h.ah = 0x0F;
    int86( 0x10, &r, &r );
    return r.h.al == 7 ? 14 : 8;                // MDA or CGA
}

// Caret functions.

void THardwareInfo::setCaretSize( ushort size ) noexcept
{
    union REGS r;
    caretSize = size;
    caretVisible = Boolean( size != 0 );
    if( size == 0 )
        r.x.cx = 0x2000;
    else
        {
        uchar height = charHeight();
        if( size > 100 )
            size = 100;
        uchar lines = uchar( (size * height + 50) / 100 );
        if( lines == 0 )
            lines = 1;
        r.h.ch = uchar( height - lines );
        r.h.cl = uchar( height - 1 );
        }
    r.h.ah = 1;
    videoInt( r );
}

ushort THardwareInfo::getCaretSize() noexcept
{
    union REGS r;
    r.h.ah = 3;
    r.h.bh = 0;
    videoInt( r );
    if( (r.h.ch & 0x20) != 0 || r.h.ch > r.h.cl )
        return 0;
    uchar height = charHeight();
    ushort size = ushort( (r.h.cl - r.h.ch + 1) * 100 / height );
    if( size == 0 )
        size = 1;
    if( size > 100 )
        size = 100;
    return size;
}

void THardwareInfo::setCaretPosition( ushort x, ushort y ) noexcept
{
    union REGS r;
    r.h.ah = 2;
    r.h.bh = 0;
    r.h.dh = uchar( y );
    r.h.dl = uchar( x );
    videoInt( r );
}

BOOL THardwareInfo::isCaretVisible() noexcept
{
    return caretVisible;
}

// Screen functions.

ushort THardwareInfo::getScreenRows() noexcept
{
    union REGS r;
    r.x.ax = 0x1130;
    r.h.bh = 0;
    r.h.dl = 0;
    videoInt( r );
    if( r.h.dl == 0 )           // Not an EGA or VGA: 25 rows.
        r.h.dl = 24;
    return r.h.dl + 1;
}

ushort THardwareInfo::getScreenCols() noexcept
{
    union REGS r;
    r.h.ah = 0x0F;
    videoInt( r );
    return r.h.ah;
}

ushort THardwareInfo::getScreenMode() noexcept
{
    union REGS r;
    r.h.ah = 0x0F;
    videoInt( r );
    ushort mode = r.h.al & 0x7F;
    if( getScreenRows() > 25 )
        mode |= TDisplay::smFont8x8;
    return mode;
}

void THardwareInfo::setScreenMode( ushort mode ) noexcept
{
    union REGS r;

    // Select the colour or monochrome adapter in the equipment word.
    ushort eflag = getBiosEquipmentFlag() & 0xFFCF;
    eflag |= ((mode & 0xFF) == TDisplay::smMono) ? 0x30 : 0x20;
    setBiosEquipmentFlag( eflag );
    setBiosVideoInfo( getBiosVideoInfo() & 0xFE );

    r.h.ah = 0;
    r.h.al = uchar( mode );
    videoInt( r );

    if( (mode & TDisplay::smFont8x8) != 0 )
        {
        r.x.ax = 0x1112;        // 8x8 font: 43 lines on EGA, 50 on VGA
        r.h.bl = 0;
        videoInt( r );

        if( getScreenRows() > 25 )
            {
            setBiosVideoInfo( getBiosVideoInfo() | 1 );

            r.h.ah = 1;
            r.x.cx = 0x0607;
            videoInt( r );

            r.h.ah = 0x12;      // Alternate print screen routine.
            r.h.bl = 0x20;
            videoInt( r );
            }
        }
}

void THardwareInfo::clearScreen( ushort w, ushort h ) noexcept
{
    union REGS r;
    r.x.ax = 0x0600;
    r.h.bh = 0x07;
    r.x.cx = 0;
    r.h.dl = uchar( w - 1 );
    r.h.dh = uchar( h - 1 );
    videoInt( r );
}

void THardwareInfo::flushScreen() noexcept
{
}

void THardwareInfo::screenWrite( ushort x, ushort y, TScreenCell *buf, DWORD len ) noexcept
{
    // Text page 0 of the colour or monochrome adapter.
    ushort cols = *biosWord( 0x4A );
    ushort seg = (*biosByte( 0x49 ) == 7) ? 0xB000 : 0xB800;
    volatile ushort __far *dst =
        (volatile ushort __far *) MK_FP( seg, (y * cols + x) * 2 );
    const ushort *src = (const ushort *) buf;

    // The mouse driver draws its text cursor in video memory, so hide it
    // while writing the line it is on.
    Boolean hide = Boolean( mouseVisible && y == ushort( mouseY ) );
    if( hide )
        cursorOff();
    for( DWORD i = 0; i < len; ++i )
        dst[i] = src[i];
    if( hide )
        cursorOn();
}

TScreenCell *THardwareInfo::allocateScreenBuffer() noexcept
{
    // Enough for any text mode TScreen can switch to without reallocating.
    ushort cols = getScreenCols(), rows = getScreenRows();
    if( cols < 80 )
        cols = 80;
    if( rows < 50 )
        rows = 50;
    TScreenCell *buffer = new TScreenCell[cols * rows];
    // Start from what is on the screen.
    ushort seg = (*biosByte( 0x49 ) == 7) ? 0xB000 : 0xB800;
    const volatile ushort __far *src = (const volatile ushort __far *) MK_FP( seg, 0 );
    ushort *dst = (ushort *) buffer;
    ushort n = getScreenCols() * getScreenRows();
    for( ushort i = 0; i < n; ++i )
        dst[i] = src[i];
    return buffer;
}

void THardwareInfo::freeScreenBuffer( TScreenCell *buffer ) noexcept
{
    delete[] buffer;
}

void THardwareInfo::setUpConsole() noexcept
{
}

void THardwareInfo::restoreConsole() noexcept
{
}

// Mouse functions.

DWORD THardwareInfo::getButtonCount() noexcept
{
    if( !mouseChecked )
        {
        mouseChecked = True;
        buttons = 0;
        if( getvect( 0x33 ) != 0 )
            {
            union REGS r;
            r.x.ax = 0;             // Reset the driver.
            mouseInt( r );
            if( r.x.ax != 0 )
                buttons = (r.x.bx == 0xFFFF) ? 2 : uchar( r.x.bx );
            if( buttons != 0 )
                {
                r.x.ax = 4;         // Move the pointer to the top left.
                r.x.cx = 0;
                r.x.dx = 0;
                mouseInt( r );
                }
            }
        mouseVisible = False;       // A reset hides the pointer.
        }
    return buttons;
}

void THardwareInfo::cursorOn() noexcept
{
    if( buttons != 0 && !mouseVisible )
        {
        union REGS r;
        r.x.ax = 1;
        mouseInt( r );
        mouseVisible = True;
        }
}

void THardwareInfo::cursorOff() noexcept
{
    if( buttons != 0 && mouseVisible )
        {
        union REGS r;
        r.x.ax = 2;
        mouseInt( r );
        mouseVisible = False;
        }
}

void THardwareInfo::setMouseRange( ushort rx, ushort ry ) noexcept
{
    if( buttons != 0 )
        {
        union REGS r;
        r.x.ax = 7;
        r.x.cx = 0;
        r.x.dx = rx << 3;
        mouseInt( r );
        r.x.ax = 8;
        r.x.cx = 0;
        r.x.dx = ry << 3;
        mouseInt( r );
        }
}

// Event functions.

static ushort controlKeyState() noexcept
{
    uchar shift = *biosByte( 0x17 );        // Shift flags
    uchar shift2 = *biosByte( 0x18 );       // Left Ctrl and left Alt
    uchar kbd = *biosByte( 0x96 );          // Right Ctrl and right Alt
    ushort state = 0;

    if( shift & 0x03 )
        state |= kbShift;
    if( shift & 0x04 )
        {
        if( kbd & 0x10 )                    // 101-key keyboard
            {
            if( shift2 & 0x01 )
                state |= kbLeftCtrl;
            if( kbd & 0x04 )
                state |= kbRightCtrl;
            if( !(state & kbCtrlShift) )
                state |= kbLeftCtrl;
            }
        else
            state |= kbLeftCtrl;
        }
    if( shift & 0x08 )
        {
        if( kbd & 0x10 )
            {
            if( shift2 & 0x02 )
                state |= kbLeftAlt;
            if( kbd & 0x08 )
                state |= kbRightAlt;
            if( !(state & kbAltShift) )
                state |= kbLeftAlt;
            }
        else
            state |= kbLeftAlt;
        }
    if( shift & 0x10 )
        state |= kbScrollState;
    if( shift & 0x20 )
        state |= kbNumState;
    if( shift & 0x40 )
        state |= kbCapsState;
    if( shift & 0x80 )
        state |= kbInsState;
    return state;
}

BOOL THardwareInfo::getMouseEvent( MouseEventType& event ) noexcept
{
    if( buttons == 0 )
        return False;

    // The state is polled, as the mouse driver has no queue. A click that
    // begins and ends between two polls is not in the state, so the press
    // counts (function 5) are read too: such a click is reported as the
    // button down at the press position, then (on the next poll) up.
    static uchar missed[2];         // Presses not seen, left and right.
    static short missedX[2], missedY[2];
    static Boolean reportedMissed;

    union REGS r;
    r.x.ax = 3;
    mouseInt( r );
    uchar state = r.h.bl;
    signed char wheel = (signed char) r.h.bh;   // CuteMouse
    short x = r.x.cx >> 3, y = r.x.dx >> 3;

    for( int b = 0; b < 2; ++b )
        {
        r.x.ax = 5;
        r.x.bx = b;
        mouseInt( r );
        // A press still held is in the state; count only the others.
        ushort presses = r.x.bx;
        if( presses && (state & (1 << b)) )
            --presses;
        if( presses )
            {
            missed[b] = uchar( presses > 2 ? 2 : presses );
            missedX[b] = r.x.cx >> 3;
            missedY[b] = r.x.dx >> 3;
            }
        }

    event.buttons = state;
    event.where.x = x;
    event.where.y = y;
    if( reportedMissed )
        reportedMissed = False;     // This poll reports the release.
    else if( state == 0 )
        for( int b = 0; b < 2; ++b )
            if( missed[b] )
                {
                --missed[b];
                event.buttons = uchar( 1 << b );
                event.where.x = missedX[b];
                event.where.y = missedY[b];
                reportedMissed = True;
                break;
                }

    event.wheel = wheel == 0 ? 0 : wheel > 0 ? mwDown : mwUp;
    event.eventFlags = 0;
    event.controlKeyState = controlKeyState();
    mouseX = x;
    mouseY = y;
    // Report the state each time, as the 16-bit Turbo Vision does: TEventQueue
    // works out moves, clicks and auto-repeat from it.
    return True;

}

BOOL THardwareInfo::getKeyEvent( TEvent& event ) noexcept
{
    union REGS r;

    r.h.ah = enhancedKeyboard ? 0x11 : 0x01;    // Is a key waiting?
    int86( 0x16, &r, &r );
    if( r.x.flags & INTR_ZF )
        return False;

    r.h.ah = enhancedKeyboard ? 0x10 : 0x00;    // Read it.
    int86( 0x16, &r, &r );

    ushort keyCode = r.x.ax;
    if( enhancedKeyboard )
        {
        // The grey keys of the 101-key keyboard come with E0h: in the
        // character byte for the cursor keys, and in the scan code byte
        // for the keypad's Enter and '/'.
        if( r.h.al == 0xE0 && r.h.ah != 0 )
            keyCode = r.h.ah << 8;
        else if( r.h.ah == 0xE0 )
            keyCode = (r.h.al == '/' ? 0x3500 : 0x1C00) | r.h.al;
        }

    // The keys the 16-bit Turbo Vision's keyboard interrupt handler
    // translated: Alt+Space, Ctrl+Ins, Shift+Ins, Ctrl+Del and Shift+Del,
    // which the BIOS reports as the plain keys (or, for Ctrl+Ins and
    // Ctrl+Del, as 9200h and 9300h).
    ushort state = controlKeyState();
    uchar scan = keyCode >> 8;
    if( scan == 0x39 && (state & kbAltShift) )
        keyCode = kbAltSpace;
    else if( (scan == 0x52 || scan == 0x92) && (state & kbCtrlShift) )
        keyCode = kbCtrlIns;
    else if( scan == 0x52 && (state & kbShift) )
        keyCode = kbShiftIns;
    else if( (scan == 0x53 || scan == 0x93) && (state & kbCtrlShift) )
        keyCode = kbCtrlDel;
    else if( scan == 0x53 && (state & kbShift) )
        keyCode = kbShiftDel;

    event.what = evKeyDown;
    event.keyDown.keyCode = keyCode;
    event.keyDown.controlKeyState = state;
    return True;

}

void THardwareInfo::waitForEvents( int timeoutMs ) noexcept
{
    if( timeoutMs == 0 )
        return;
    // Give the time to a multitasker (Windows, OS/2, DOSBox), or else wait
    // for the next interrupt: the keyboard, the mouse or the 18.2 Hz timer.
    union REGS r;
    r.x.ax = 0x1680;
    int86( 0x2F, &r, &r );
    if( r.h.al != 0 )
        __asm__ __volatile__( "sti\n\thlt" );
}

void THardwareInfo::interruptEventWait() noexcept
{
}

BOOL THardwareInfo::setClipboardText( TStringView ) noexcept
{
    return False;
}

BOOL THardwareInfo::requestClipboardText( void (&)( TStringView ) ) noexcept
{
    return False;
}

// System functions.

static void __interrupt __far ctrlBreakHandler()
{
    TSystemError::ctrlBreakHit = True;
}

static void __interrupt __far critErrorHandler( union INTPACK r )
{
    r.h.al = 3;                 // Fail the DOS call.
}

BOOL THardwareInfo::setCtrlBrkHandler( BOOL install ) noexcept
{
    if( install && oldInt1B == 0 )
        {
        oldInt1B = getvect( 0x1B );
        oldInt23 = getvect( 0x23 );
        setvect( 0x1B, ctrlBreakHandler );
        setvect( 0x23, ctrlBreakHandler );
        }
    else if( !install && oldInt1B != 0 )
        {
        setvect( 0x1B, oldInt1B );
        setvect( 0x23, oldInt23 );
        oldInt1B = oldInt23 = 0;
        }
    return TRUE;
}

BOOL THardwareInfo::setCritErrorHandler( BOOL install ) noexcept
{
    if( install && oldInt24 == 0 )
        {
        oldInt24 = getvect( 0x24 );
        // In C++ a handler that takes the registers needs a cast.
        setvect( 0x24, (void (__interrupt __far *)()) critErrorHandler );

        }
    else if( !install && oldInt24 != 0 )
        {
        setvect( 0x24, oldInt24 );
        oldInt24 = 0;
        }
    return TRUE;
}

#endif // __TV_CLANG_DOS__
