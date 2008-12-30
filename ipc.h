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


#ifndef __IPC_H
#define __IPC_H

typedef enum
{
	IPCA_NOACTION,
	IPCA_INFOMSG,
	IPCA_NZBFILE,
	IPCA_DLSTATUS
} IPCACT_T;

typedef enum { IPCR_OK, IPCR_FAIL, IPCR_ABORTED } IPCRES_T;

struct IPCMsg
{
	struct Message  ipc_msg;
	unsigned long   ipc_ID;
	IPCACT_T        ipc_action;
	APTR            ipc_data;
	union
	{
		LONG            ipc_res_result;
		struct MsgPort *ipc_res_port;
	} ipc_res;
};

#define ipc_result	ipc_res.ipc_res_result
#define ipc_port	ipc_res.ipc_res_port
#define IPC_MAGIC	0x9ffff444


GLOBAL BOOL IPC_PutMsg( struct MsgPort *destino, IPCACT_T action, APTR udata );
GLOBAL VOID IPC_Dispatch( struct MsgPort * mPort );

#endif /* __IPC_H */
