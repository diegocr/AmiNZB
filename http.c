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
#include "http.h"
#include "nntp.h"
#include "wndp.h"

//------------------------------------------------------------------------------

STATIC LONG http_recv(LONG sockfd, APTR buf, ULONG buf_len, struct Library * SocketBase)
{
	fd_set rdfs;
	LONG result = 0;
	
	struct timeval timeout = { 40,0 };
	
	FD_ZERO(&rdfs);
	FD_SET(sockfd, &rdfs);
	
	if(WaitSelect( sockfd+1, &rdfs, NULL, NULL, &timeout, NULL))
		result = recv( sockfd, buf, buf_len, 0);
	
	return( result );
}

//------------------------------------------------------------------------------

STATIC STRPTR GetURL_Real(STRPTR url, struct Library *SocketBase)
{
	UBYTE *rc = NULL, _host[300], _path[4096]; // the thread HAS enough stack!
	STRPTR host=_host, path=_path;
	long maxlen, port = 80, sockfd = -1;
	struct hostent *hostaddr;
	
	ENTER();
	DBG_STRING(url);
	
	if(!Strnicmp( url, "http://", 7))
		url += 7;
	
	maxlen = sizeof(_host)-1;
	do {
		*host++ = *url++;
		
	} while( *url && *url != '/' && *url != ':' && --maxlen > 0);
	
	// check for overflow
	if(maxlen <= 0) {
		SetIoErr( ERROR_BUFFER_OVERFLOW );
		goto done;
	}
	
	// was the server port supplied on the url?
	if(*url == ':')
	{
		long chs;
		
		// assuming AmigaOS > 3.0 !!
		chs = StrToLong( ++url, &port );
		
		// if a invalid port was found, fall back to standard number
		if(!(port > 0 && port < 65536))
			port = 80;
		
		url += chs;
	}
	
	maxlen = sizeof(_path)-1;
	if(!*url)
		*path++ = '/';
	else do {
		*path++ = *url++;
		
	} while( *url && --maxlen > 0);
	
	// check for overflow
	if(maxlen <= 0) {
		SetIoErr( ERROR_BUFFER_OVERFLOW );
		goto done;
	}
	
	// null terminate hostname and server path
	*path = 0;
	*host = 0;
	
	host = _host;
	path = _path;
	if((hostaddr = gethostbyname( host )))
	{
		if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) != -1)
		{
			struct sockaddr_in saddr;
			int i;
			
			bzero( &saddr, sizeof(struct sockaddr_in));
			
			saddr.sin_len		= sizeof(struct sockaddr_in);
			saddr.sin_family	= AF_INET;
			saddr.sin_port		= htons( port );
			
			for( i = 0 ; hostaddr->h_addr_list[i] ; i++ )
			{
				CopyMem( hostaddr->h_addr_list[i], (APTR)&saddr.sin_addr, hostaddr->h_length);
				
				if(!connect( sockfd, (struct sockaddr *)&saddr, sizeof(saddr)))
					break;
			}
			
			if( hostaddr->h_addr_list[i] != NULL ) // connected?
			{
				#define RC_MEMSIZE	1*1024*1024
				
				if((rc = AllocVec( RC_MEMSIZE, MEMF_PUBLIC))) // should be more than enough for a web page...
				{
					LONG len;
					BOOL all_ok = FALSE;
					
					snprintf( rc, RC_MEMSIZE-1, 
						"GET %s HTTP/1.0\r\n"
						"User-Agent: Mozilla/5.0 (compatible; %s %s)\r\n"
						"Host: %s\r\n"
						"\r\n", path, ProgramVersionTag()+6, ProgramCopyright(), host );
					
					len = strlen(rc);
					
					if(send( sockfd, rc, len, 0) == len )
					{
						LONG pos = 0, max = RC_MEMSIZE-1, code = 404;
						
						while((len = http_recv( sockfd, &rc[pos], max, SocketBase))>0)
						{
							max -= len;
							pos += len;
						}
						
						*((char *)&rc[pos]) = '\0';
						
						if(pos > 9) StrToLong( &rc[9], &code);
						
						DBG_VALUE(pos);
						DBG_VALUE(code);
						
						switch( code )
						{
							case 200:
								DBG("Everything ok, got a 200 page\n");
								all_ok = TRUE;
								break;
							
							case 301:
							case 302:
							{
								if((pos = FindPosNoCase( rc, "location: ")))
								{
									UBYTE new_url[2048];
									unsigned int ep = FindPos( &rc[pos], "\r\n");
									
									DBG_ASSERT(ep < sizeof(new_url)-1);
									
									if(ep < sizeof(new_url)-1)
									{
										CopyMem( &rc[pos], new_url, ep-2);
										new_url[ep-2] = '\0';
										
										FreeVec( rc );
										rc = GetURL_Real( new_url, SocketBase );
										
										if( rc != NULL )
											all_ok = TRUE;
									}
								}
								
							}	break;
							
							default:
								break;
						}
					}
					
					if( all_ok != TRUE )
					{
						if( rc != NULL )
						{
							FreeVec( rc );
							rc = NULL;
						}
					}
				}
			}
			
			shutdown( sockfd, 2 );
			CloseSocket( sockfd );
		}
	}
	
done:	
	RETURN(rc);
	return(rc);
}

//------------------------------------------------------------------------------

STATIC VOID GetURL_SubTask(APTR stub)
{
	APTR ptask;
	struct Library * SocketBase;
	GetURL_StubCB stub_cb;
	
	ENTER();
	DBG_POINTER(stub);
	
	stub_cb = stub;
	
	ptask = ProgressWindow(" Fetching HTTP Data... ");
	
	while(!(SocketBase = OpenLibrary("bsdsocket.library", 0)))
	{
		Delay( TICKS_PER_SECOND );
		if(SetSignal(0L,SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
			break;
	}
	
	if( SocketBase != NULL )
	{
		GetURL_StubCB_Data gsd;
		
		if((gsd = Malloc(sizeof(*gsd))) == NULL)
		{
			OutOfMemory("http stub");
		}
		else
		{
			STRPTR url, data;
			
			gsd->socketbase = SocketBase;
			
			while(stub_cb( &url, NULL, &gsd))
			{
				if(SetSignal(0L,SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
					break;
				
				data = GetURL_Real( url, SocketBase );
				
				if(!stub_cb( NULL, data, &gsd))
					break;
			}
			
			Free(gsd);
		}
		
		CloseLibrary( SocketBase );
	}
	
	if( ptask != NULL ) // This is A MUST!
		Signal( ptask, SIGBREAKF_CTRL_C);
	LEAVE();
}

//------------------------------------------------------------------------------

BOOL GetURL(APTR stub_cb)
{
	APTR thr;
	BOOL rc = TRUE;
	
	ENTER();
	DBG_POINTER(stub_cb);
	
	thr = QuickThread((APTR) GetURL_SubTask,(APTR) stub_cb );
	
	if(thr == NULL)
	{
		__request("Cannot launch GetURL() SubTask!");
		rc = FALSE;
	}
	
	RETURN(rc);
	return(rc);
}

//------------------------------------------------------------------------------

#define FSURL	"http://www.newsservers.net/FreeNewsServers.php?offset=%d"

STATIC BOOL ObtainFreeServer_Stub( STRPTR *url, STRPTR data, GetURL_StubCB_Data *gsd )
{
	BOOL rc = TRUE,nomorework = FALSE;
	
	ENTER();
	DBG_POINTER(url);
	DBG_POINTER(data);
	
	if(!(gsd && *gsd) || (SetSignal(0L,SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C))
	{
		nomorework = TRUE;
	}
	else if( url != NULL ) // requesting a [new] url
	{
		if((*gsd)->url_num > 30 ) // defined on the FSURL page
		{
			nomorework = TRUE;
			
			request("Couldn't found an alive server!");
		}
		else
		{
			snprintf((*gsd)->url, sizeof((*gsd)->url)-1, FSURL,(*gsd)->url_num );
			(*gsd)->url_num += 10; // to the next url (if needed)
			*url = (*gsd)->url;
		}
	}
	else if( data != NULL ) // requesting to process received data
	{
		int pos;
		STRPTR ptr=data, host=NULL, user=NULL, pass=NULL;
		BOOL syntax_error = TRUE, nomem = FALSE, gotAserver = FALSE;
		
		// loop while found ie "<a href='news://dnews.globalnews.it'>"
		while((pos = FindPos( ptr, "news://")))
		{
			UBYTE errstr[100];
			LONG sockfd;
			
			ptr += pos;
			
			if(!(pos = FindPos( ptr, "'")))
				break;
			
			if(syntax_error) syntax_error = FALSE;
			
			if((host = AllocVec( pos, MEMF_PUBLIC)))
			{
				CopyMem( ptr, host, --pos );
				host[pos] = '\0';
				
				ptr += pos;
				DBG_STRING(host);
				
				// look for username "<strong>Username:</strong> None &nbsp;"
				if((pos = FindPos( ptr, "<strong>Username:</strong> ")))
				{
					ptr += pos;
					
					if((pos = FindPos( ptr, " ")))
					{
						if((user = AllocVec( pos, MEMF_PUBLIC)))
						{
							BOOL auth_needed;
							
							CopyMem( ptr, user, --pos );
							user[pos] = '\0';
							
							ptr += pos;
							DBG_STRING(user);
							
							auth_needed = (Stricmp( user, "None") != 0);
							
							if( auth_needed )
							{
								// look for password "<strong>Password:</strong> None</td>"
								if((pos = FindPos( ptr, "<strong>Password:</strong> ")))
								{
									ptr += pos;
									
									if((pos = FindPos( ptr, "<")))
									{
										if((pass = AllocVec( pos, MEMF_PUBLIC)))
										{
											CopyMem( ptr, pass, --pos );
											pass[pos] = '\0';
											
											ptr += pos;
											DBG_STRING(pass);
										}
										else nomem = TRUE;
									}
									else syntax_error = TRUE;
								}
								else syntax_error = TRUE;
							}
							else
							{
								FreeVec( user );
								user = NULL;
							}
						}
						else nomem = TRUE;
					}
					else syntax_error = TRUE;
				}
				else syntax_error = TRUE;
			}
			else nomem = TRUE;
			
			if( syntax_error || nomem ) break;
			
			// try the server...
			sockfd = nntp_connect( host, 119, user, pass, errstr, sizeof(errstr)-1, (*gsd)->socketbase);
			
			if( sockfd != -1 ) // connection succeed!
			{
				STRPTR u=user,p=pass;
				
				DBG("Connection to news://%s succeed!\n", host);
				
				nntp_close( sockfd, (*gsd)->socketbase);
				
				gotAserver = TRUE;
				
				if(!u) u = "";
				if(!p) p = "";
				
				SettingsSetNntpAuthInfo(host,119,u,p);
				Delay(12); // let the MUI set the vars...
				break;
			}
			else
			{
				DBG("connection to %s: %s\n", host, errstr );
			}
			
			if(SetSignal(0L,SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
				break;
		}
		
		if( syntax_error || nomem || gotAserver )
		{
			if( gotAserver == FALSE )
			{
				if( syntax_error )
					request("newsservers.net's syntax has changed!");
				else
					OutOfMemory("http stub");
			}
			
			if( host ) FreeVec( host );
			if( user ) FreeVec( user );
			if( pass ) FreeVec( pass );
			
			nomorework = TRUE;
		}
	}
	else
	{
		DBG("Failed fetching last url\n");
	}
	
	if((rc = !nomorework)==FALSE)
	{
		if( data ) FreeVec( data );
		
		PushSet( MainApp, 2, MUIA_Application_Sleep, FALSE );
		DisplayBeep(NULL);
	}
	
	RETURN(rc);
	return(rc);
}

//------------------------------------------------------------------------------

BOOL ObtainFreeServer( VOID )
{
	BOOL rc;
	
	ENTER();
	
	if((rc = GetURL((APTR) ObtainFreeServer_Stub ))==TRUE)
	{
		set( MainApp, MUIA_Application_Sleep, TRUE );
	}
	
	RETURN(rc);
	return(rc);
}

//------------------------------------------------------------------------------


//------------------------------------------------------------------------------


