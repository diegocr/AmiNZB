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
#include "ipc.h"
#include "debug.h"
#include "downler.h"

GLOBAL void __request(const char *msg);
GLOBAL VOID iListInsert(STRPTR msg);
GLOBAL VOID dlStatusReportHandler( struct dlStatus * st );
//------------------------------------------------------------------------------

BOOL IPC_PutMsg( struct MsgPort *destino, IPCACT_T action, APTR udata )
{
	struct MsgPort *replyport = NULL;
	BOOL error = TRUE;
	
	ENTER();
	DBG_POINTER(destino);
	
	if(destino && (replyport = CreateMsgPort()))
	{
		struct IPCMsg ipcmsg;
		APTR xMsg;
		
		ipcmsg.ipc_msg.mn_ReplyPort	= replyport;
		ipcmsg.ipc_msg.mn_Length	= sizeof(struct IPCMsg);
		ipcmsg.ipc_ID			= IPC_MAGIC;
		ipcmsg.ipc_action		= action;
		ipcmsg.ipc_result		= IPCR_ABORTED;
		ipcmsg.ipc_data			= udata;
		
		DBG("Sending action '%ld' from %lx to %lx\n", action, replyport, destino);
		
		Forbid();
		PutMsg( destino, &ipcmsg.ipc_msg);
		WaitPort(replyport);
		while((xMsg = GetMsg( replyport )))
		{
			DBG("Got reply...\n");
			
			switch(((struct IPCMsg *)xMsg)->ipc_result)
			{ // TODO
				case IPCR_ABORTED:
					DBG("IPCR_ABORTED\n");
					break;
				
				case IPCR_FAIL:
					DBG("IPCR_FAIL\n");
					break;
				
				case IPCR_OK:
					DBG("IPCR_OK\n");
					break;
				default:
					break;
			}
		}
		Permit();
		
		DeleteMsgPort(replyport);
		
		error = FALSE;
	}
	
	LEAVE();
	
	return !error;
}

//------------------------------------------------------------------------------

VOID IPC_Dispatch( struct MsgPort * mPort )
{
	struct IPCMsg * ipcmsg;
	
	ENTER();
	DBG_POINTER(mPort);
	
	// get the next waiting message
	while((ipcmsg = (struct IPCMsg *)GetMsg( mPort )))
	{
		DBG_VALUE(ipcmsg->ipc_ID);
		
		if(ipcmsg->ipc_ID == IPC_MAGIC)
		{
			switch( ipcmsg->ipc_action )
			{
				case IPCA_INFOMSG:
					iListInsert((STRPTR) ipcmsg->ipc_data );
					ipcmsg->ipc_result = IPCR_OK;
					break;
				
				case IPCA_NZBFILE:
				{
					STRPTR nzbfile = (STRPTR) ipcmsg->ipc_data;
					DBG_STRING(nzbfile);
					
					if(Analyzer( nzbfile ))
						ipcmsg->ipc_result = IPCR_OK;
					else
						ipcmsg->ipc_result = IPCR_FAIL;
				}	break;
				
				case IPCA_DLSTATUS:
					dlStatusReportHandler((struct dlStatus *)ipcmsg->ipc_data);
					ipcmsg->ipc_result = IPCR_OK;
					break;
				
				default:
					break;
			}
		}
		
		ReplyMsg((APTR) ipcmsg );
	}
	
	LEAVE();
}


