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
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/alib.h>
#include <proto/utility.h>

#include <proto/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "muiface.h"
#include "thread.h"
#include "debug.h"
#include "util.h"
#include "nntp.h"
#include "wndp.h"

//------------------------------------------------------------------------------

VOID ObtainServerList_Stub( VOID )
{
	struct Library * SocketBase;
	APTR ptask;
	
	ENTER();
	
	ptask = ProgressWindow(" Receiving list of available groups... ");
	
	while(!(SocketBase = OpenLibrary("bsdsocket.library", 0)))
	{
		Delay( TICKS_PER_SECOND );
		if(SetSignal(0L,SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
			break;
	}
	
	if( SocketBase != NULL )
	{
		STRPTR nntp_host, nntp_user, nntp_pass;
		UWORD  nntp_port;
		UBYTE errstr[512];
		LONG sockfd;
		
		*errstr = '\0';
		SettingsGetNntpAuthInfo( &nntp_host, &nntp_port, &nntp_user, &nntp_pass );
		
		if((sockfd = nntp_connect(nntp_host,nntp_port,nntp_user,nntp_pass,errstr,sizeof(errstr)-1,SocketBase)) != -1)
		{
			BPTR fd;
			UBYTE file[100];
			
			snprintf( file, sizeof(file)-1, "RAM:%s", nntp_host );
			
			if(!(fd = Open( file, MODE_NEWFILE)))
			{
				CopyMem( "Cannot write to RAM:", errstr, sizeof(errstr)-1);
			}
			else
			{
				APTR buffer;
				ULONG buffer_size = 262145;
				
				if((buffer = AllocMem( buffer_size, MEMF_FAST)))
				{
					fd_set rdfs;
					struct timeval timeout = {40,0};
					
					FD_ZERO(&rdfs);
					FD_SET(sockfd, &rdfs);
					
					if(215==nntp_send(sockfd,errstr,sizeof(errstr)-1,SocketBase,"LIST\r\n"))
					{
						LONG r;
						*errstr = '\0';
						
						while(WaitSelect( sockfd+1, &rdfs, NULL, NULL, &timeout, NULL)>0)
						{
							r = recv( sockfd, buffer, buffer_size-1, 0);
							if( r < 1 ) break;
							
							Write( fd, buffer, r );
						}
					}
					
					FreeMem( buffer, buffer_size);
				}
				
				Close( fd );
				
				if(*errstr)
					DeleteFile( file );
				else
					request("groups list saved to \"%s\" succesfully", file );
			}
			
			nntp_close( sockfd, SocketBase );
		}
		
		CloseLibrary( SocketBase );
		
		if( *errstr )
			__request( errstr );
	}
	
	if( ptask != NULL ) // This is A MUST!
		Signal( ptask, SIGBREAKF_CTRL_C);
	LEAVE();
}

//------------------------------------------------------------------------------

BOOL ObtainServerList( VOID )
{
	APTR thr;
	BOOL rc = TRUE;
	
	ENTER();
	
	thr = QuickThread((APTR) ObtainServerList_Stub, NULL );
	
	if(thr == NULL)
	{
		__request("Cannot launch SubTask!");
		rc = FALSE;
	}
	
	RETURN(rc);
	return(rc);
}


