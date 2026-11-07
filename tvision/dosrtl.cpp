/*------------------------------------------------------------*/
/* filename -       dosrtl.cpp                                */
/*                                                            */
/* function(s)                                                */
/*                  The Borland C++ RTL functions that Turbo  */
/*                  Vision uses and the 16-bit DOS C library  */
/*                  (picolibc, clang-dos) does not have.      */
/*------------------------------------------------------------*/

// Copyright (c) 2026 clang-retro contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include <tvision/tv.h>

#if defined( __TV_CLANG_DOS__ )

#include <dir.h>
#include <dos.h>
#include <i86.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

// Strings.

extern "C" int stricmp( const char *s1, const char *s2 ) noexcept
{
    for( ;; ++s1, ++s2 )
        {
        int c1 = toupper( (uchar) *s1 ), c2 = toupper( (uchar) *s2 );
        if( c1 != c2 || c1 == 0 )
            return c1 - c2;
        }
}

extern "C" int strnicmp( const char *s1, const char *s2, size_t maxlen ) noexcept
{
    for( ; maxlen != 0; --maxlen, ++s1, ++s2 )
        {
        int c1 = toupper( (uchar) *s1 ), c2 = toupper( (uchar) *s2 );
        if( c1 != c2 || c1 == 0 )
            return c1 - c2;
        }
    return 0;
}

extern "C" int memicmp( const void *s1, const void *s2, size_t n ) noexcept
{
    const uchar *p1 = (const uchar *) s1, *p2 = (const uchar *) s2;
    for( ; n != 0; --n, ++p1, ++p2 )
        {
        int c1 = toupper( *p1 ), c2 = toupper( *p2 );
        if( c1 != c2 )
            return c1 - c2;
        }
    return 0;
}

extern "C" char *ultoa( unsigned long value, char *buffer, int radix ) noexcept
{
    char tmp[33];
    char *p = tmp;
    if( radix < 2 || radix > 36 )
        radix = 10;
    do  {
        unsigned digit = unsigned( value % radix );
        *p++ = char( digit < 10 ? '0' + digit : 'a' + digit - 10 );
        value /= radix;
        } while( value != 0 );
    char *d = buffer;
    while( p != tmp )
        *d++ = *--p;
    *d = '\0';
    return buffer;
}

extern "C" char *ltoa( long value, char *buffer, int radix ) noexcept
{
    if( radix == 10 && value < 0 )
        {
        buffer[0] = '-';
        ultoa( 0UL - (unsigned long) value, buffer + 1, 10 );
        return buffer;
        }
    return ultoa( (unsigned long) value, buffer, radix );
}

// Drives and directories.

int getdisk() noexcept
{
    union REGS r;
    r.h.ah = 0x19;
    intdos( &r, &r );
    return r.h.al;
}

int setdisk( int drive ) noexcept
{
    // Returns the number of drives, as Borland's.
    union REGS r;
    r.h.ah = 0x0E;
    r.h.dl = uchar( drive );
    intdos( &r, &r );
    return r.h.al;
}

int getcurdir( int drive, char *direc ) noexcept
{
    // The directory, without the drive and the first backslash. Drive 0 is
    // the current one, 1 is A and so on.
    char buf[64];
    union REGS r;
    struct SREGS s;
    segread( &s );
    r.h.ah = 0x47;
    r.h.dl = uchar( drive );
    s.ds = FP_SEG( (char __far *) buf );
    r.x.si = FP_OFF( (char __far *) buf );
    intdosx( &r, &r, &s );
    if( r.x.cflag )
        return -1;
    strnzcpy( direc, buf, MAXDIR );
    return 0;
}

// Finding files: INT 21h functions 4Eh and 4Fh, with the find_t (or ffblk)
// as the DTA.

static void __far *getDTA() noexcept
{
    union REGS r;
    struct SREGS s;
    segread( &s );
    r.h.ah = 0x2F;
    intdosx( &r, &r, &s );
    return MK_FP( s.es, r.x.bx );
}

static void putDTA( void __far *dta ) noexcept
{
    union REGS r;
    struct SREGS s;
    segread( &s );
    r.h.ah = 0x1A;
    s.ds = FP_SEG( dta );
    r.x.dx = FP_OFF( dta );
    intdosx( &r, &r, &s );
}

unsigned _dos_findfirst( const char *path, unsigned attrib,
                         struct find_t *finfo ) noexcept
{
    void __far *old = getDTA();
    putDTA( (void __far *) finfo );
    union REGS r;
    struct SREGS s;
    segread( &s );
    r.h.ah = 0x4E;
    r.x.cx = attrib;
    s.ds = FP_SEG( (const char __far *) path );
    r.x.dx = FP_OFF( (const char __far *) path );
    intdosx( &r, &r, &s );
    putDTA( old );
    return r.x.cflag ? r.x.ax : 0;
}

unsigned _dos_findnext( struct find_t *finfo ) noexcept
{
    void __far *old = getDTA();
    putDTA( (void __far *) finfo );
    union REGS r;
    r.h.ah = 0x4F;
    intdos( &r, &r );
    putDTA( old );
    return r.x.cflag ? r.x.ax : 0;
}

int findfirst( const char *path, struct ffblk *ffblk, int attrib ) noexcept
{
    return _dos_findfirst( path, attrib, (struct find_t *) ffblk ) == 0 ? 0 : -1;
}

int findnext( struct ffblk *ffblk ) noexcept
{
    return _dos_findnext( (struct find_t *) ffblk ) == 0 ? 0 : -1;
}

// Path names.

static size_t movestr( char *dest, const char *src, size_t size ) noexcept
{
    size_t n = strlen( src );
    if( size == 0 )
        return 0;
    if( n > size - 1 )
        n = size - 1;
    memmove( dest, src, n );
    dest[n] = '\0';
    return n;
}

void fnmerge( char *pathP, const char *driveP, const char *dirP,
              const char *nameP, const char *extP ) noexcept
{
    // The parts may be in pathP itself, further on (fexpand does that), so
    // they are moved in order and pathP is not cleared first.
    size_t n = 0;
    if( driveP && *driveP )
        {
        n += movestr( &pathP[n], driveP, MAXPATH - n );
        if( n && pathP[n-1] != ':' )
            n += movestr( &pathP[n], ":", MAXPATH - n );
        }
    if( dirP && *dirP )
        {
        n += movestr( &pathP[n], dirP, MAXPATH - n );
        if( n && pathP[n-1] != '\\' && pathP[n-1] != '/' )
            n += movestr( &pathP[n], "\\", MAXPATH - n );
        }
    if( nameP && *nameP )
        n += movestr( &pathP[n], nameP, MAXPATH - n );
    if( extP && *extP )
        {
        if( *extP != '.' )
            n += movestr( &pathP[n], ".", MAXPATH - n );
        n += movestr( &pathP[n], extP, MAXPATH - n );
        }
    pathP[n] = '\0';
}


static Boolean copyComponent( char *dst, const char *start, const char *end,
                              size_t max ) noexcept
{
    if( end == start )
        return False;
    if( dst )
        {
        size_t n = size_t( end - start );
        if( n > max - 1 )
            n = max - 1;
        memcpy( dst, start, n );
        dst[n] = '\0';
        }
    return True;
}

int fnsplit( const char *pathP, char *driveP, char *dirP, char *nameP,
             char *extP ) noexcept
{
    int flags = 0;
    if( driveP )
        *driveP = '\0';
    if( dirP )
        *dirP = '\0';
    if( nameP )
        *nameP = '\0';
    if( extP )
        *extP = '\0';
    if( !pathP || !*pathP )
        return 0;

    size_t len = strlen( pathP );
    const char *pathEnd = pathP + len;
    const char *colonP = 0, *slashP = 0, *lastDotP = 0, *firstDotP = 0;
    for( size_t i = len; i-- > 0; )
        {
        char c = pathP[i];
        if( (c == '?' || c == '*') && !slashP )
            flags |= WILDCARDS;
        else if( c == '.' && !slashP )
            {
            if( !lastDotP )
                lastDotP = pathP + i;
            firstDotP = pathP + i;
            }
        else if( (c == '\\' || c == '/') && !slashP )
            slashP = pathP + i;
        else if( c == ':' && i == 1 )
            {
            colonP = pathP + i;
            break;
            }
        }
    const char *driveEnd = colonP ? colonP + 1 : pathP;
    const char *dirEnd = slashP ? slashP + 1 : driveEnd;
    const char *nameEnd = lastDotP ? lastDotP : pathEnd;
    // A path that ends with '.' or '..' has no file name.
    if( lastDotP == pathEnd - 1 && lastDotP - firstDotP < 2 && firstDotP == dirEnd )
        dirEnd = nameEnd = pathEnd;
    if( copyComponent( driveP, pathP, driveEnd, MAXDRIVE ) )
        flags |= DRIVE;
    if( copyComponent( dirP, driveEnd, dirEnd, MAXDIR ) )
        flags |= DIRECTORY;
    if( copyComponent( nameP, dirEnd, nameEnd, MAXFILE ) )
        flags |= FILENAME;
    if( copyComponent( extP, nameEnd, pathEnd, MAXEXT ) )
        flags |= EXTENSION;
    return flags;
}

#endif // __TV_CLANG_DOS__
