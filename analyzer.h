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

#ifndef AMINZB_ANALYZER_H
#define AMINZB_ANALYZER_H

#define POSTER_MAXLEN	512
#define SUBJECT_MAXLEN	1024

struct NodeListItem {
	
	struct MinNode node;
	
	STRPTR mItem;
	UWORD mItemLength;
};

GLOBAL BOOL Analyzer(STRPTR nzbFile);

GLOBAL BOOL AddNodeListItem( struct MinList * list, char * item );
GLOBAL VOID FreeNodeListItem( struct MinList * list );
GLOBAL BOOL DupNodeListItem( struct MinList * dst, struct MinList * src );


#if !defined(IsMinListEmpty)
# define IsMinListEmpty(x)     (((x)->mlh_TailPred) == (struct MinNode *)(x))
#endif

#define ITERATE_LIST(list, type, node)				\
	for (node = (type)((struct List *)(list))->lh_Head;	\
		((struct MinNode *)node)->mln_Succ;		\
			node = (type)((struct MinNode *)node)->mln_Succ)

#endif /* AMINZB_ANALYZER_H */
