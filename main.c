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


#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include "ipc.h"
#include "thread.h"
#include "debug.h"
#include "util.h"

GLOBAL BOOL muiface(STRPTR nzbFile);
GLOBAL STRPTR ProgramName(VOID);

int __nocommandline = 1;

int startup ( VOID )
{
	LONG arg[1] = {0};
	struct RDArgs *args;
	STATIC UBYTE nzbFile[512];
	short nzbFile_error = 0;
	int rc = RETURN_FAIL;
	
	ENTER();
	*nzbFile = 0;
	
	if((args = ReadArgs("NZBFILE", (long*)arg, NULL)) != NULL)
	{
		if(arg[0] && *((char *) arg[0] ))
		{
			BPTR fd;
			
			CopyMem((STRPTR)arg[0], nzbFile, sizeof(nzbFile)-1);
			
			if((fd = Open( nzbFile, MODE_OLDFILE)))
			{
				#define XML_TAG	"<?xml"
				UBYTE tag[sizeof(XML_TAG)+2];
				LONG r;
				
				r = Read( fd, tag, sizeof(XML_TAG));
				Close( fd );
				
				if(r != (LONG) sizeof(XML_TAG))
				{
					PrintFault(IoErr(),FilePart(nzbFile));
					nzbFile_error = 7;
				}
				else if(Strnicmp( tag, XML_TAG, sizeof(XML_TAG)-1))
				{
					Printf("%s: invalid or unrecognized file...\n", (LONG) FilePart(nzbFile));
					nzbFile_error = 6;
				}
				else
				{
					struct MsgPort * brother;
					
					Forbid();
					brother = (struct MsgPort *)FindPort(ProgramName());
					Permit();
					
					if( brother == NULL )
					{
						nzbFile_error = -1;
					}
					else
					{
						if(!IPC_PutMsg( brother, IPCA_NZBFILE, nzbFile ))
						{
							PutStr("error contacting brother task...\n");
							nzbFile_error = 5;
						}
						else
						{
							if(!Strnicmp( FilePart(nzbFile), "IBrowseTempFile", 15))
							{
								// IBrowse deletes the file just after 
								// exiting from the external viewer (this task),
								// hence I'll do a little delay to prevent
								// that deletion
								
								Delay( TICKS_PER_SECOND * 4 );
							}
							nzbFile_error = -2;
						}
					}
				}
			}
			else
			{
				PrintFault(IoErr(),FilePart(nzbFile));
				nzbFile_error = 4;
			}
		}
		
		FreeArgs( args );
	}
	
	if( nzbFile_error == -2 )
		return RETURN_OK;
	
	if( nzbFile_error > 0 )
		return RETURN_FAIL;
	
	if( nzbFile_error != -1 )
		*nzbFile = 0;
	
	rc = (muiface(*nzbFile ? nzbFile : NULL));
	LEAVE();
	
	return rc;
}

int main( /*int argv, char ** argc*/ )
{
	ENTER();
	
	if(aThreadInit())
	{
		startup();
		
		aThreadExit();
	}
	
	LEAVE();
}
