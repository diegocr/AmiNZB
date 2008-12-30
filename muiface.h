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


#define SAVEFOLDERLEN	1024
#define MAXFILELENGTH	((SAVEFOLDERLEN)+30/*FFS*/)

GLOBAL Object * MainApp;

#define PushMethod( obj, numargs, args... )	\
	DoMethod( MainApp, MUIM_Application_PushMethod, (obj),(numargs), args )

#define PushSet( obj, numargs, args... ) \
	PushMethod((obj), ((numargs)+1), MUIM_Set, args )


#if defined(__GNUC__) || ((__STDC__ == 1L) && (__STDC_VERSION__ >= 199901L))
  // please note that we do not evaluate the return value of GetAttr()
  // as some attributes (e.g. MUIA_Selected) always return FALSE, even
  // when they are supported by the object. But setting b=0 right before
  // the GetAttr() should catch the case when attr doesn't exist at all
# define xget(OBJ, ATTR) ({ULONG b=0; GetAttr(ATTR, OBJ, &b); b;})
#endif

#ifndef MAKE_ID
# define MAKE_ID(a,b,c,d)	\
	((ULONG) (a)<<24 | (ULONG) (b)<<16 | (ULONG) (c)<<8 | (ULONG) (d))
#endif

GLOBAL STRPTR ProgramName( VOID );
GLOBAL STRPTR ProgramVersionTag( VOID );
GLOBAL STRPTR ProgramDescription( VOID );
GLOBAL STRPTR ProgramCopyright( VOID );

GLOBAL VOID MakeColsSortable( Object *list );

GLOBAL VOID iListInsert(STRPTR msg);
GLOBAL BOOL iListAdd(STRPTR msg);
GLOBAL VOID dlStatusReport( APTR data );

#define nfo iListAdd
#define nfo_fmt(args...)				\
({							\
	STATIC UBYTE message[2048];			\
	snprintf( message, sizeof(message)-1, args );	\
	nfo( message ); message;			\
})


GLOBAL VOID SettingsGetNntpAuthInfo( STRPTR *nntp_host, UWORD *nntp_port, STRPTR *nntp_user, STRPTR *nntp_pass );
GLOBAL VOID SettingsSetNntpAuthInfo( STRPTR nntp_host, UWORD nntp_port, STRPTR nntp_user, STRPTR nntp_pass );
GLOBAL STRPTR IncomingFolder( VOID );
GLOBAL STRPTR CompletedFolder( VOID );


GLOBAL void __request(const char *msg);

