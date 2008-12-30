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


#include <exec/types.h>
#include <libraries/mui.h>
#include "logo.h"

GLOBAL Object *MUI_NewObject( STRPTR, Tag, ...);

Object * AmiNZB_Logo ( VOID )
{
	Object * obj;
	
	obj = BodychunkObject,
		MUIA_Group_Spacing         , 0,
		MUIA_FixWidth              , AMINZB_WIDTH ,
		MUIA_FixHeight             , AMINZB_HEIGHT,
		MUIA_Bitmap_Width          , AMINZB_WIDTH ,
		MUIA_Bitmap_Height         , AMINZB_HEIGHT,
		MUIA_Bodychunk_Depth       , AMINZB_DEPTH ,
		MUIA_Bodychunk_Body        , (UBYTE *) AmiNZB_body,
		MUIA_Bodychunk_Compression , AMINZB_COMPRESSION,
		MUIA_Bodychunk_Masking     , AMINZB_MASKING,
		MUIA_Bitmap_SourceColors   , (ULONG *) AmiNZB_colors,
		MUIA_Bitmap_Transparent    , 0,
		MUIA_Bitmap_Precision      , PRECISION_EXACT,
	End;
	
	return obj;
}
