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

#ifndef AMINZB_DOWNLER_H
#define AMINZB_DOWNLER_H

#include "analyzer.h"	/* for #?_MAXLEN */

struct dlNode // each download manager (thread) has this list to work with
{
	struct dlNode * next;
	ULONG id; // that ID is the same as well on all ->next fields
	
	UBYTE poster[POSTER_MAXLEN]; // the person who uploaded the file(s)
	UBYTE subject[SUBJECT_MAXLEN]; // article's subject (filename)
	ULONG date; // post date
	ULONG size; // size in bytes of the file (whole segments)
	UWORD segnum; // number of segments (articles) for this file
	
	struct MinList segments; // article(s) (Message-ID) to download
	struct MinList groups; // groups where we can find that article
};

struct dlQueue // Queue managed by the main task
{
	struct dlQueue * next;
	
	struct dlNode * dl;
};

struct dlStatus // the way of reporting dls progress to the main task
{
	ULONG magic;
	#define dlStatusMagicID	0x0004f22A
	
	ULONG task;
	STRPTR file;
	ULONG size_current;
	ULONG size_total;
	UWORD segment_current;
	UWORD segments_total;
	STRPTR status;
	
	ULONG cps;
};

GLOBAL BOOL DownloadStart(struct dlNode *);
GLOBAL VOID FreeDlNode(struct dlNode *);

GLOBAL struct dlQueue * RecoverSavedDownloads( VOID );



#endif /* AMINZB_DOWNLER_H */
