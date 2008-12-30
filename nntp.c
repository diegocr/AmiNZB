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

/* :ts=5 */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>

#include "debug.h"
#include "muiface.h"
#include "nntp.h"

STATIC LONG nntp_auth( LONG sockfd, STRPTR auth_step, STRPTR auth_str, UBYTE *errstr, ULONG errstr_size, struct Library *SocketBase);

//------------------------------------------------------------------------------

LONG recv_line(LONG sockfd, UBYTE *buffer, ULONG buflen, struct Library *SocketBase )
{
	UBYTE ch;
	LONG maxlen = buflen, rrc;
	
	ENTER();
	*buffer = 0;
	
	do {
		if((rrc = recv( sockfd, &ch, 1,0)) < 1)
		{
			DBG_VALUE(rrc);
			break;
		}
		
		if(ch != '\r')
		{
			if(--maxlen < 1)
			{
				nfo("error: buffer overflow at recv_line()");
				break;
			}
			*buffer++ = ch;
		}
		if(ch == '\n')
		{
			*buffer = 0;
			RETURN(buflen - maxlen);
			return(buflen - maxlen);
		}
		
	} while(1);
	
	*buffer = 0;
	DBG("Incomplete buffer = \"%s\"\n", buffer );
	
	RETURN(-1);
	return(-1);
}

//------------------------------------------------------------------------------

LONG nntp_recv( LONG sockfd, UBYTE *buffer, ULONG buflen, struct Library *SocketBase )
{
	LONG rc = -1;
	
	ENTER();
	
	if(recv_line( sockfd, buffer, buflen, SocketBase ) != -1)
	{
		DBG_STRING(buffer);
		
		StrToLong( buffer, &rc );
	}
	
	RETURN(rc);
	return rc;
}

//------------------------------------------------------------------------------

LONG nntp_send( LONG sockfd, UBYTE *errstr, ULONG errstr_size, APTR SocketBase, const char *fmt, ...)
{
	UBYTE recv_string[1024];
	UBYTE send_string[1024];
	LONG send_string_length;
	LONG rc = -1;
	va_list args;
	
	ENTER();
	*errstr = 0;
	
	va_start( args, fmt );
	vsnprintf( send_string, sizeof(send_string)-1, fmt, args );
	va_end( args );
	
	DBG_STRING(send_string);
	send_string_length = strlen(send_string);
	
	if(send( sockfd, send_string, send_string_length, 0 ) == send_string_length )
	{
		if((rc = nntp_recv( sockfd, recv_string, sizeof(recv_string)-1, SocketBase))!=-1)
		{
			CopyMem( &recv_string[4], errstr, errstr_size);
		}
		else CopyMem( "recv error", errstr, errstr_size);
	}
	else CopyMem( "send error", errstr, errstr_size);
	
	RETURN(rc);
	return rc;
}
#define _nntp_send( fmt... )	\
	nntp_send( sockfd, errstr, errstr_size,(APTR) SocketBase, fmt )

//------------------------------------------------------------------------------

LONG nntp_connect(STRPTR nntp_host,UWORD nntp_port,
	STRPTR nntp_user,STRPTR nntp_pass, UBYTE *errstr, ULONG errstr_size, struct Library *SocketBase)
{
	LONG sockfd = -1;
	struct hostent *hostaddr;
	
	ENTER();
	DBG("Connecting to news://%s:%ld (%s)...\n", nntp_host, nntp_port, nntp_user);
	
	if((hostaddr = gethostbyname( nntp_host )))
	{
		if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) != -1)
		{
			BOOL everything_ok = FALSE;
			struct sockaddr_in saddr;
			int i;
			
			bzero( &saddr, sizeof(struct sockaddr_in));
			
			saddr.sin_len		= sizeof(struct sockaddr_in);
			saddr.sin_family	= AF_INET;
			saddr.sin_port		= htons( nntp_port );
			
			for( i = 0 ; hostaddr->h_addr_list[i] ; i++ )
			{
				CopyMem( hostaddr->h_addr_list[i], (APTR)&saddr.sin_addr, hostaddr->h_length);
				
				if(!connect( sockfd, (struct sockaddr *)&saddr, sizeof(saddr)))
					break;
			}
			
			if( hostaddr->h_addr_list[i] != NULL ) // connected?
			{
				LONG v;
				UBYTE buf[1024];
				
				if((v=nntp_recv( sockfd, buf, sizeof(buf)-1,SocketBase))!=-1)
				{
					if( v == 200 || v == 201 )
					{
						// check if authentication is required
						if((v=nntp_group(sockfd,"alt",errstr,errstr_size,SocketBase))!=-1)
						{
							BOOL auth_required = (v == 480);
							
							DBG("Server requires authentication?: %s\n", auth_required ? "YES":"NO");
							
							if( auth_required )
							{
								if(nntp_user && *nntp_user)
								{
									if((v = nntp_auth_user(sockfd,nntp_user,errstr,errstr_size,SocketBase))!=-1)
									{
										if( v == 381 ) // pass required?
										{
											if((281 == nntp_auth_pass(sockfd,nntp_pass,errstr,errstr_size,SocketBase)))
											{
												everything_ok = TRUE;
											}
										}
										else if( v == 281 )
										{
											everything_ok = TRUE;
										}
									}
								}
							}
							else everything_ok = TRUE;
						}
					}
					else CopyMem( buf, errstr, errstr_size);
				}
				else CopyMem( "recv error", errstr, errstr_size);
			}
			else CopyMem( "cannot connect to host", errstr, errstr_size);
			
			if( everything_ok == FALSE )
			{
				CloseSocket( sockfd );
				sockfd = -1;
			}
		}
		else CopyMem( "cannot create socket", errstr, errstr_size);
	}
	else CopyMem( "cannot resolve hostname", errstr, errstr_size);
	//else CopyMem( "", errstr, errstr_size);
	
	RETURN(sockfd);
	return sockfd;
}

//------------------------------------------------------------------------------

VOID nntp_close( LONG sockfd, struct Library *SocketBase)
{
	send( sockfd, "QUIT\r\n", 6,0);
	shutdown( sockfd, 2 );
	CloseSocket( sockfd );
}

//------------------------------------------------------------------------------

LONG nntp_auth_user( LONG sockfd, STRPTR auth_user, UBYTE *errstr, ULONG errstr_size, struct Library *SocketBase)
{
	return nntp_auth( sockfd, "USER", auth_user, errstr, errstr_size, SocketBase );
}

//------------------------------------------------------------------------------

LONG nntp_auth_pass( LONG sockfd, STRPTR auth_pass, UBYTE *errstr, ULONG errstr_size, struct Library *SocketBase)
{
	return nntp_auth( sockfd, "PASS", auth_pass, errstr, errstr_size, SocketBase );
}

//------------------------------------------------------------------------------

STATIC LONG nntp_auth( LONG sockfd, STRPTR auth_step, STRPTR auth_str, UBYTE *errstr, ULONG errstr_size, struct Library *SocketBase)
{
	LONG rc;
	
	ENTER();
	
	rc = _nntp_send("AUTHINFO %s %s\r\n", auth_step, auth_str);
	
	RETURN(rc);
	return rc;
}

//------------------------------------------------------------------------------

LONG nntp_group( LONG sockfd, STRPTR group, UBYTE *errstr, ULONG errstr_size, struct Library *SocketBase)
{
	LONG rc;
	
	ENTER();
	
	rc = _nntp_send("GROUP %s\r\n", group);
	
	RETURN(rc);
	return rc;
}

//------------------------------------------------------------------------------

LONG nntp_article( LONG sockfd, STRPTR article, UBYTE *errstr, ULONG errstr_size, struct Library *SocketBase)
{
	LONG rc;
	
	ENTER();
	
	rc = _nntp_send("ARTICLE %s%s%s\r\n", 
		*article != '<' ? "<":"", article, *article != '<' ? ">":"");
	
	RETURN(rc);
	return rc;
}

//------------------------------------------------------------------------------



//------------------------------------------------------------------------------

