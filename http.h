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

#ifndef AMINZB_HTTP_H
#define AMINZB_HTTP_H

typedef struct __GetURL_StubCB_Data
{
	UBYTE		url[4096];
	int		url_num;
	
	struct Library * socketbase;
	
} * GetURL_StubCB_Data;

/**
 * GetURL_StubCB Usage:
 *
 * when url != NULL we are requesting a [new] url, when *it is* NULL we want
 * to process the received 'data' from the previously set url
 * 
 * returns TRUE if new url should be fetched, FALSE on error/finished
 */
typedef BOOL (*GetURL_StubCB)(STRPTR *url, APTR data, GetURL_StubCB_Data *gsd);

GLOBAL BOOL GetURL(APTR stub_cb);
GLOBAL BOOL ObtainFreeServer( VOID );


#endif /* AMINZB_HTTP_H */
