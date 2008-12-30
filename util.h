/* ***** BEGIN LICENSE BLOCK *****
 * Version: MIT/X11 License
 * 
 * Copyright (c) 2007 Diego Casorran
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * 
 * Contributor(s):
 *   Diego Casorran <dcasorran@gmail.com> (Original Author)
 * 
 * ***** END LICENSE BLOCK ***** */


#ifndef UTIL_H
#define UTIL_H

GLOBAL struct SignalSemaphore gSemaphore;

GLOBAL void __request(const char *msg); // <- it's at libnix

GLOBAL BOOL InitMemoryPool( VOID );
GLOBAL VOID ClearMemoryPool( VOID );
GLOBAL APTR Malloc( ULONG size );
GLOBAL VOID Free(APTR mem);

GLOBAL void request(const char *msg,...);
GLOBAL char *ioerrstr( char *prompt );

GLOBAL VOID OutOfMemory(STRPTR where);

GLOBAL VOID TimeToDS(ULONG time, struct DateStamp * ds);
GLOBAL ULONG DsToTime(struct DateStamp * ds);
GLOBAL ULONG Time(ULONG *t);

GLOBAL APTR FileToMem( STRPTR FileName, ULONG * SizePtr );
GLOBAL ULONG FileSize( STRPTR filename );

GLOBAL ULONG CRC32( APTR buffer, ULONG buffer_length, ULONG crc );
#define crc32( buf, len ) CRC32( buf, len, -1L)

GLOBAL unsigned int FindPos( unsigned char * pajar, unsigned char * aguja );
GLOBAL unsigned int FindPosNoCase( unsigned char * pajar, unsigned char * aguja );
GLOBAL STRPTR Strip( STRPTR string, UBYTE ch, UBYTE ch2 );
GLOBAL STRPTR Trim( STRPTR string );
GLOBAL ULONG hstoul( STRPTR hex_string, int * len );


#endif /* UTIL_H */
