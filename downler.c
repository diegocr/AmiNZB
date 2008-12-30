/* ***** BEGIN LICENSE BLOCK *****
 * Version: MIT/X11 License
 * 
 * Copyright (c) 2008 Diego Casorran
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
#include <proto/utility.h>
#include <proto/alib.h>
#include "ipc.h"
#include "debug.h"
#include "downler.h"
#include "muiface.h"
#include "util.h"
#include "nntp.h"
#include "thread.h"
#include "decoders.h"
#include <stdio.h> /* snprintf */
#include <string.h> /* strlen */
#include <proto/socket.h> /* Just WaitSelect() */

#define ANZB_ID	MAKE_ID('A','N','Z','B')
#define LIST_ID	MAKE_ID('L','I','S','T')
#define NEXT_ID	MAKE_ID('N','E','X','T')
#define FEOF_ID	MAKE_ID( 0 ,'E','O','F')

//------------------------------------------------------------------------------

VOID FreeDlNode( struct dlNode * node )
{
	struct dlNode * c, * n;
	
	for( c = node ; c ; c = n )
	{
		n = c->next;
		
		FreeNodeListItem( &c->segments );
		FreeNodeListItem( &c->groups );
		
		FreeVec( c );
	}
}

//------------------------------------------------------------------------------

STATIC STRPTR dlFileTag( UBYTE *buf, ULONG buf_size )
{
	snprintf( buf, buf_size, "%s-dl", ProgramName());
	
	return buf;
}

STATIC STRPTR dlFilename( struct dlNode * dl, UBYTE *buf, ULONG buf_size )
{
	BOOL error = TRUE;
	
	ENTER();
	
	*buf = 0;
	if(AddPart( buf, IncomingFolder(), buf_size)!=FALSE)
	{
		UBYTE tag[32], daz[32];
		
		snprintf(daz,sizeof(daz)-1, "%s.%lx", dlFileTag(tag,sizeof(tag)-1), dl->id);
		DBG_STRING(daz);
		
		if(AddPart( buf, daz, buf_size )!=FALSE)
		{
			error = FALSE;
		}
	}
	DBG_STRING(buf);
	
	if( error )
		*buf = 0;
	
	LEAVE();
	
	return buf;
}

STATIC STRPTR sFileTemp(STRPTR MessageID, UBYTE *buf, ULONG buflen )
{
	BOOL error = TRUE;
	
	ENTER();
	
	*buf = 0;
	if(AddPart( buf, IncomingFolder(), buflen )!=FALSE)
	{
		UBYTE tmp[12];
		
		snprintf( tmp, sizeof(tmp)-1, "%lu", crc32(MessageID,strlen(MessageID)));
		DBG_STRING(tmp);
		
		if(AddPart( buf, tmp, buflen)!=FALSE)
		{
			error = FALSE;
		}
	}
	DBG_STRING(buf);
	
	if( error )
		*buf = 0;
	
	LEAVE();
	
	return buf;
}

//------------------------------------------------------------------------------

STATIC BOOL SaveDownload( struct dlNode * dl )
{
	BPTR fd;
	UBYTE fileptr[MAXFILELENGTH], *filename;
	BOOL rc = FALSE;
	
	ENTER();
	DBG_POINTER(dl);
	
	filename = dlFilename(dl,fileptr,sizeof(fileptr)-1);
	
	if((fd = Open( filename, MODE_NEWFILE)))
	{
		ULONG ul;
		UWORD uw;
		
		ul = ANZB_ID;
		if(Write( fd, &ul, sizeof(ULONG)) == sizeof(ULONG))
		{
			ul = dl->id;
			if(Write( fd, &ul, sizeof(ULONG)) == sizeof(ULONG))
			{
				struct dlNode * ptr;
				BOOL error = TRUE;
				
				for( ptr = dl ; ptr ; ptr = ptr->next )
				{
					struct NodeListItem * node;
					BOOL ml_Error = FALSE;
					
					error = TRUE; // assume a error will happend ;-)
					
					if(Write( fd, &ptr->date, sizeof(ULONG)) != sizeof(ULONG))
						break;
					
					if(Write( fd, &ptr->size, sizeof(ULONG)) != sizeof(ULONG))
						break;
					
					if(Write( fd, &ptr->segnum, sizeof(UWORD)) != sizeof(UWORD))
						break;
					
					uw = strlen(ptr->poster);
					if(Write( fd, &uw, sizeof(UWORD)) != sizeof(UWORD))
						break;
					
					if(Write( fd, ptr->poster, uw) != uw)
						break;
					
					uw = strlen(ptr->subject);
					if(Write( fd, &uw, sizeof(UWORD)) != sizeof(UWORD))
						break;
					
					if(Write( fd, ptr->subject, uw) != uw)
						break;
					
					ul = LIST_ID;
					if(Write( fd, &ul, sizeof(ULONG)) != sizeof(ULONG))
						break;
					
					ITERATE_LIST( &ptr->segments, struct NodeListItem *, node)
					{
						if(Write( fd, &node->mItemLength, sizeof(UWORD)) != sizeof(UWORD))
						{	ml_Error=TRUE;	break;}
						
						if(Write( fd, node->mItem, node->mItemLength) != node->mItemLength)
						{	ml_Error=TRUE;	break;}
					}
					
					if( ml_Error == TRUE ) break;
					
					ul = LIST_ID;
					if(Write( fd, &ul, sizeof(ULONG)) != sizeof(ULONG))
						break;
					
					ITERATE_LIST( &ptr->groups, struct NodeListItem *, node)
					{
						if(Write( fd, &node->mItemLength, sizeof(UWORD)) != sizeof(UWORD))
						{	ml_Error=TRUE;	break;}
						
						if(Write( fd, node->mItem, node->mItemLength) != node->mItemLength)
						{	ml_Error=TRUE;	break;}
					}
					
					if( ml_Error == TRUE ) break;
					
					ul = NEXT_ID;
					if(Write( fd, &ul, sizeof(ULONG)) != sizeof(ULONG))
						break;
					
					error = FALSE; // we have luck this time ;-)
				}
				
				if( error == FALSE )
				{
					ul = FEOF_ID;
					if(Write( fd, &ul, sizeof(ULONG)) != sizeof(ULONG))
						error = TRUE;
				}
				
				if( error == TRUE )
				{
					nfo_fmt("Error writing to file \033b%s", filename );
				}
				else rc = TRUE;
			}
			else nfo_fmt("Error writing to file \033b%s", filename );
		}
		else nfo_fmt("Error writing to file \033b%s", filename );
		
		Close(fd);
	}
	else nfo_fmt("Error storing download data into \033b%s", filename );
	
	if( rc == FALSE )
		DeleteFile(filename);
	else {
		nfo_fmt("New download status saved to %s", filename );
	}
	
	LEAVE();
	return rc;
}

//------------------------------------------------------------------------------

STATIC VOID FreeQueueVec( struct dlQueue * queue )
{
	struct dlQueue * ptr, * next;
	
	for( ptr = queue ; ptr ; ptr = next )
	{
		next = ptr->next;
		
		FreeDlNode( ptr->dl );
		FreeVec( ptr );
	}
}

STATIC struct dlQueue *QueueFile( STRPTR filename, struct dlQueue * queue )
{
	struct dlQueue * new;
	BOOL fatal = TRUE;
	
	ENTER();
	DBG_STRING(filename);
	DBG_POINTER(queue);
	
	if((new = AllocVec(sizeof(struct dlQueue), MEMF_PUBLIC|MEMF_CLEAR)))
	{
		STRPTR file;
		LONG size = 0;
		
		if((file = (STRPTR) FileToMem( filename, &size )))
		{
			STRPTR ptr=file;
			LONG error = 0;
			ULONG ul, id = 0;
			UWORD uw;
			
			#define GetV( dst, SiZe ) \
			({	if( size >= (LONG)sizeof(SiZe)) { \
					dst = *((SiZe *) ptr ); ptr += sizeof(SiZe); size -= sizeof(SiZe); \
					if( size < 1 ) error = ERROR_SEEK_ERROR; \
				} else dst = 0; dst; \
			})
			#define GetX( dst, SiZe ) \
			({	if( size >= (LONG)SiZe ) { \
					CopyMem( ptr, dst, SiZe ); ptr += SiZe; size -= SiZe; \
					if( size < 1 ) error = ERROR_SEEK_ERROR; \
				} dst; \
			})
			
			if( size > (LONG)sizeof(ULONG))
				GetV( id, ULONG);
			
			if(id == ANZB_ID)
			{
				struct dlNode * dl = NULL;
				
				GetV( id, ULONG);
				
				while(!error && size >= (LONG)sizeof(ULONG))
				{
					if(*((ULONG *) ptr ) == FEOF_ID)
						break;
					
					error = ERROR_SEEK_ERROR;
					
					if((dl = AllocVec(sizeof(struct dlNode), MEMF_PUBLIC|MEMF_CLEAR)))
					{
						STRPTR item;
						UWORD itemLen;
						
						dl->id = id;
						NewList((struct List *) &dl->segments );
						NewList((struct List *) &dl->groups );
						
						GetV( dl->date   , ULONG ); DBG_VALUE(dl->date);
						GetV( dl->size   , ULONG ); DBG_VALUE(dl->size);
						GetV( dl->segnum , UWORD ); DBG_VALUE(dl->segnum);
						
						GetV( uw, UWORD );
						GetX( dl->poster, uw );	DBG_STRING(dl->poster);
						GetV( uw, UWORD );
						GetX( dl->subject, uw ); DBG_STRING(dl->subject);
						
						GetV( ul, ULONG);
						if(ul != LIST_ID)
							break;
						
						DBG(" ---- Segments:\n");
						
						do {
							GetV( itemLen, UWORD );
							if( size < 1 || !itemLen )
								break;
							
							if(!(item = AllocMem( itemLen+1, MEMF_ANY)))
							{
								error = ERROR_NO_FREE_STORE;
								break;
							}
							
							GetX( item, itemLen);
							item[itemLen] = '\0';
							
							DBG_STRING(item);
							
							uw = AddNodeListItem(&dl->segments, item);
							
							FreeMem( item, (ULONG) itemLen+1 );
							
							if(uw == FALSE)
							{
								error = ERROR_NO_FREE_STORE;
								break;
							}
							
						} while(size > (LONG)sizeof(ULONG) && *((ULONG *) ptr ) != LIST_ID);
						
						if( error == ERROR_NO_FREE_STORE )
							break;
						
						GetV( ul, ULONG);
						if(ul != LIST_ID || size < 1)
							break;
						
						DBG(" ---- Groups:\n");
						
						do {
							GetV( itemLen, UWORD );
							if( size < 1 || !itemLen )
								break;
							
							if(!(item = AllocMem( itemLen+1, MEMF_ANY)))
							{
								error = ERROR_NO_FREE_STORE;
								break;
							}
							
							GetX( item, itemLen);
							item[itemLen] = '\0';
							
							DBG_STRING(item);
							
							uw = AddNodeListItem(&dl->groups, item);
							
							FreeMem( item, (ULONG) itemLen+1 );
							
							if(uw == FALSE)
							{
								error = ERROR_NO_FREE_STORE;
								break;
							}
							
						} while(size > (LONG)sizeof(ULONG) && *((ULONG *) ptr ) != NEXT_ID);
						
						if( error == ERROR_NO_FREE_STORE )
							break;
						
						GetV( ul, ULONG);
						if(ul != NEXT_ID || size < 1)
							break;
						
						DBG_POINTER(dl);
						
						dl->next = new->dl;
						new->dl = dl;
						dl = NULL;
						error = FALSE;
					}
					else error = IoErr();
				}
				
				if( error )
					FreeDlNode( dl );
			}
			else error = ERROR_OBJECT_WRONG_TYPE;
			
			FreeVec( file );
			
			if(error) SetIoErr( error );
			else fatal = FALSE;
		}
	}
	
	if( fatal == TRUE )
	{
		STRPTR ies = ioerrstr(NULL);
		
		request("Failed loading %s, ioerr: %s", filename, ies);
		nfo_fmt("Failed loading %s, ioerr: %s", filename, ies);
		
		if( new )
		{
			new->next = queue;
			queue = new;
		}
		
		FreeQueueVec( queue );
		
		queue = NULL;
	}
	else
	{
		new->next = queue;
		queue = new;
	}
	
	DBG_POINTER(queue);
	LEAVE();
	return queue;
}

struct dlQueue * RecoverSavedDownloads( VOID )
{
	char __fib[sizeof(struct FileInfoBlock) + 3];
	struct FileInfoBlock *fib = 
		(struct FileInfoBlock *)(((long)__fib + 3L) & ~3L);
	
	BPTR lock;
	UBYTE tag_buf[32],*tag = dlFileTag(tag_buf,sizeof(tag_buf)-1);
	LONG howmany = 0, tlen = strlen(tag);
	struct dlQueue * queue = NULL;
	
	ENTER();
	
	if((lock = Lock( IncomingFolder(), SHARED_LOCK)))
	{
		if(Examine(lock,fib)!=DOSFALSE)
		{
			while(ExNext(lock,fib)!=DOSFALSE)
			{
				if(*fib->fib_FileName == *tag) // this is more faster than calling directly Stricmp ???
				{
					if(!Strnicmp( fib->fib_FileName, tag, tlen ))
					{
						STATIC UBYTE file[64];
						
						++howmany;
						
						*file = 0;
						AddPart( file, IncomingFolder(), sizeof(file)-1);
						AddPart( file, fib->fib_FileName, sizeof(file)-1);
						
						if(!(queue = QueueFile( file, queue )))
						{
							howmany = -1;
							break;
						}
					}
				}
			}
		}
		else request("Cannot %s %s!", "examine", IncomingFolder());
		
		UnLock( lock );
	}
	else request("Cannot %s %s!", "lock", IncomingFolder());
	
	switch( howmany )
	{
		case -1:
			nfo("Fatal error recovering previous downloads");
			break;
		
		case 0:
			break;
		
		default:
			nfo_fmt("Recovered %ld downloads from a previous session", howmany);
			break;
	}
	
	LEAVE();
	return queue;
}

//------------------------------------------------------------------------------

#define Status( string )		\
	status.status = string;	\
	dlStatusReport((APTR) &status )

VOID DownloadStart_SubTask( struct dlNode * dl )
{
	struct dlStatus status;
	struct Library * SocketBase;
	struct Task * task = FindTask(NULL);
	
	ENTER();
	DBG_POINTER(dl);
	DBG("Starting NZB Download Manager, Task $%08lx\n", task );
	
	bzero( &status, sizeof(struct dlStatus));
	status.magic = dlStatusMagicID;
	status.task = (ULONG) task;
	status.segments_total = dl->segnum;
	status.size_total = dl->size;
	status.file = dl->subject;
	
	while(!(SocketBase = OpenLibrary("bsdsocket.library", 0)))
	{
		status.status = "Waiting for TCP/IP Stack to be running...";
		dlStatusReport((APTR) &status );
		Delay( TICKS_PER_SECOND );
		if(SetSignal(0L,SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
			break;
	}
	
	if( SocketBase != NULL )
	{
		UBYTE errstr[512];
		LONG sockfd;
		STRPTR nntp_host, nntp_user, nntp_pass;
		UWORD  nntp_port;
		
		Status("Stablishing connection...");
		SettingsGetNntpAuthInfo( &nntp_host, &nntp_port, &nntp_user, &nntp_pass );
		
		if((sockfd = nntp_connect(nntp_host,nntp_port,nntp_user,nntp_pass,errstr,sizeof(errstr)-1,SocketBase)) != -1)
		{
			struct dlNode * ptr;
			BOOL fatal = FALSE;
			fd_set rdfs;
			struct timeval timeout = {80,0};
			
			FD_ZERO(&rdfs);
			FD_SET(sockfd, &rdfs);
			
			for( ptr = dl ; ptr && !fatal ; ptr = ptr->next )
			{
				struct MinList decoded_files;
				struct NodeListItem * groups, *articles;
				UBYTE sf[MAXFILELENGTH], *savefile, *article;
				LONG v;
				
				SaveDownload( ptr );
				NewList((struct List *)&decoded_files);
				
				status.segments_total   = ptr->segnum;
				status.size_total       = ptr->size;
				status.file             = ptr->subject;
				status.size_current     = 0;
				status.segment_current  = 0;
				
				DBG("Downloading file \"%s\"\n", ptr->subject );
				
				ITERATE_LIST(&ptr->segments,struct NodeListItem *,articles)
				{
					BPTR lock;
					
					status.segment_current++;
					
					article = articles->mItem;
					savefile = sFileTemp( article, sf, sizeof(sf)-1);
					
					if((lock = Lock( savefile, SHARED_LOCK)))
					{
						DBG("Segment \"%s\" already exists on disk, skipping...", article);
						UnLock(lock);
					}
					else if((lock = Open( savefile, MODE_NEWFILE)))
					{
						BOOL downloaded = FALSE, next_group = FALSE;
						
						Status(nfo_fmt("Fetching segment \"%s\"...", article ));
						
						ITERATE_LIST(&ptr->groups, struct NodeListItem *, groups)
						{
							BOOL got_headers = FALSE;
							LONG tnow, told=0, lt=0, lnlen=0,lnlenleft=0;
							
							next_group = FALSE;
							
							DBG("Selecting group \"%s\"\n", groups->mItem);
							
							v=nntp_group(sockfd,groups->mItem,errstr,sizeof(errstr)-1,SocketBase);
							
							if( v == -1 )
							{
								Status(nfo_fmt("Error selecting %s \"%s\": %s (download aborted)", "group", groups->mItem, errstr ));
								fatal = TRUE;
								break;
							}
							
							if( v == 411 )
							{
								Status(nfo_fmt("group \"%s\" does not exists, server returned \"%s\"", groups->mItem, errstr ));
								next_group = TRUE;
								continue;
							}
							
							if( v != 211 ) // it MUST be 211...
							{
								Status(nfo_fmt("ERROR selecting %s \"%s\", server answered %ld where %d was expected (%s), download aborted...", "group", groups->mItem, v, 211, errstr ));
								fatal = TRUE;
								break;
							}
							
							DBG("Requesting article \"%s\"\n", article );
							
							v=nntp_article(sockfd,article,errstr,sizeof(errstr)-1,SocketBase);
							
							if( v == -1 )
							{
								Status(nfo_fmt("Error selecting %s \"%s\": %s (download aborted)", "segment", article, errstr ));
								fatal = TRUE;
								break;
							}
							
							if( v == 430 )
							{
								Status(nfo_fmt("segment \"%s\" does not exists into group \"%s\", trying another group...", article, groups->mItem ));
								next_group = TRUE;
								continue;
							}
							
							if( v != 220 ) // it MUST be 220...
							{
								Status(nfo_fmt("ERROR selecting %s \"%s\", server answered %ld where %d was expected (%s), download aborted...", "segment", articles->mItem, v, 220, errstr ));
								fatal = TRUE;
								break;
							}
							
							// start fetching the article
							
							DBG("Starting download...\n");
							//Status("Downloaidng...");
							
							while(!fatal)
							{
								UBYTE line[16384], *line_ptr, *ptr;
								LONG rs;
								
								if(SetSignal(0L,SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
								{
									nfo_fmt("Download Manager $%08lx HAVE BEEN ABORTED", (LONG)task );
									fatal = TRUE;
									break;
								}
								
								if(lt==0) {
									Status("Downloaidng (handshaking)...");
								}
								if(!WaitSelect( sockfd+1, &rdfs, NULL, NULL, &timeout, NULL))
								{
									Delay(20); // just to test..
									continue;
								}
								
								line_ptr = &line[lnlen];
								lnlenleft = sizeof(line) - lnlen - 1;
								
								if( got_headers )
									rs = recv( sockfd, line_ptr, lnlenleft, 0 );
								else
									rs = recv_line(sockfd,line,sizeof(line)-1,SocketBase);
								if( rs < 1 )
								{
									DBG(" ******* recv_line() FAILED, rc = %ld\n", rs);
									fatal = TRUE;
									break;
								}
								
								status.size_current += rs;
								
								if((Time(&tnow)-told) > 1) // update each second
								{
									if(lt!=0)
									{
										DOUBLE elapsed = (DOUBLE)(tnow-told);
										ULONG transfered = status.size_current-lt;
										status.cps = transfered / elapsed;
									}
									lt=status.size_current;
									
									Status("Downloaidng...");
									told = tnow;
								}
								
								// skip headers
								if( got_headers == FALSE )
								{
									if(line[0] == '\n' && !line[1])
									{
										got_headers = TRUE;
									}
									continue;
								}
								
								lnlen=0;
								line_ptr=line;
								while(line_ptr)
								{
									if((ptr = strchr( line_ptr, '\n')))
										*ptr++ = '\0';
									else {
										lnlen = strlen(line_ptr);
										memmove( line, line_ptr, lnlen+1);
										break;
									}
									
									if(line_ptr[0] == '.' && !line_ptr[1])
									{
										DBG("download of segment complete\n");
										downloaded = TRUE;
										break;
									}
									
									if((FPuts(lock,line_ptr) == -1) || (FPutC(lock,'\n') < 1))
									{
										Status(nfo_fmt("error writing to file \"%s\"!", savefile));
										fatal = TRUE;
										break;
									}
									
									line_ptr = ptr;
								}
								
								if( fatal || downloaded ) break;
							}
							
							if( fatal || downloaded ) break;
						}
						
						if( next_group == TRUE )
						{
							Status(nfo_fmt("error fetching segment \"%s\" from \033bALL\033n the available groups, \033baborting download \"%s\"...", article, ptr->subject));
							fatal = TRUE; // <- failed getting a segment, hence there is no sense on continuing
						}
						
						Close( lock );
						if( downloaded == FALSE )
						{
							if(DeleteFile(savefile)==FALSE)
							{
								Status(nfo_fmt("FATAL ERROR: cannot delete (temp) file \"%s\", DELETE IT YOURSELF", savefile));
								fatal = TRUE;
								break;
							}
						}
						else
						{
							UBYTE comm[80];
							
							CopyMem( ptr->subject, comm, sizeof(comm)-1);
							comm[79] = '\0';
							
							SetComment( savefile, comm );
							
							Status(nfo_fmt("download of segment \"%s\" for file \"%s\" COMPLETED.", article, ptr->subject));
						}
					}
					else
					{
						Status(nfo_fmt("FATAL ERROR: cannot write to \033b\"%s\"", savefile));
						fatal = TRUE;
						break;
					}
					
					if( fatal ) break;
				}
				
				DBG_VALUE(fatal);
				if( fatal == TRUE ) break;
				
				Status("All segments received, decoding starts NOW!...");
				
				ITERATE_LIST(&ptr->segments,struct NodeListItem *,articles)
				{
					DEC_T dect;
					LONG decoder_rc = ERROR_NOT_IMPLEMENTED;
					
					article = articles->mItem;
					savefile = sFileTemp( article, sf, sizeof(sf)-1);
					
					DBG_ASSERT(FileSize(savefile) != 0);
					
					switch((dect = GuessDecoderFromFile( savefile )))
					{
						case DEC_YENC:
							decoder_rc = yEnc_Decoder( savefile, CompletedFolder(), &decoded_files);
							break;
						
						case DEC_BASE64:
							break;
						
						case DEC_UU:
							break;
						
						default:
						case DEC_UNKNOWN:
							Status("File contains parts with unknown encoding!");
							fatal = TRUE;
							break;
					}
					
					if( fatal == TRUE ) break;
					
					if( decoder_rc != 0 ) // thats a value returned from IoErr()
					{
						UBYTE decoder_rc_string[100];
						
						Fault( decoder_rc, NULL, decoder_rc_string, sizeof(decoder_rc)-1);
						
						Status(nfo_fmt("ERROR %ld (%s) decoding segment \"%s\" for file \"%s\", \033bfurther download/process aborted", decoder_rc, decoder_rc_string, article, ptr->subject));
						fatal = TRUE;
					}
					
					if( fatal == TRUE ) break;
					
					Status(nfo_fmt("segment \"%s\" decoded correctly", article ));
				}
				
				if( fatal == TRUE ) break;
				
				// TODO: use 'decoded_files' to join/extract files IF needed (ie PAR2 decoder)
				FreeNodeListItem(&decoded_files);
				
				Status("ALL segments received AND decoded CORRECTLY!");
				
				ITERATE_LIST(&ptr->segments,struct NodeListItem *,articles)
				{
					article = articles->mItem;
					savefile = sFileTemp( article, sf, sizeof(sf)-1);
					
					DeleteFile( savefile );
				}
				
				nfo_fmt("\033b\033iDownload of file \"%s\" completed successfully!", ptr->subject );
			}
			
			if( ptr == NULL && fatal == FALSE )
			{
				UBYTE fileptr[MAXFILELENGTH], *file;
				
				file = dlFilename(dl,fileptr,sizeof(fileptr)-1);
				
				DeleteFile(file);
				
				DBG("dlNode succesfuly downloaded, deleteing cache file \"%s\"\n", file);
			}
			
			nntp_close( sockfd, SocketBase );
		}
		else
		{
			Status(errstr);
			
			SaveDownload( dl );
		}
		
		CloseLibrary( SocketBase );
	}
	else
	{
		SaveDownload( dl );
	}
	
	FreeDlNode( dl );
	
	DBG("NZB Download Manager $%08lx, Finished.\n", task );
	LEAVE();
}

//------------------------------------------------------------------------------

BOOL DownloadStart( struct dlNode * dl )
{
	APTR thr;
	BOOL rc = TRUE;
	
	ENTER();
	
	thr = QuickThread((APTR) DownloadStart_SubTask,(APTR) dl );
	
	if(thr == NULL)
	{
		SaveDownload( dl );
		
		__request("Cannot launch Downloader SubTask!\n\nYour Downloaded have been saved to disk and will be\nqueued to download on the next program run");
		rc = FALSE;
	}
	
	LEAVE();
	return rc;
}

