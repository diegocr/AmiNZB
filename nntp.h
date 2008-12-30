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

#ifndef AMINZB_NNTP_H
#define AMINZB_NNTP_H

GLOBAL LONG recv_line(LONG sockfd, UBYTE *buffer, ULONG buflen, struct Library *SocketBase );

GLOBAL LONG nntp_recv( LONG sockfd, UBYTE *buffer, ULONG buflen, struct Library *SocketBase );
GLOBAL LONG nntp_send( LONG sockfd, UBYTE *errstr, ULONG errstr_size, APTR SocketBase, const char *fmt, ...);
GLOBAL LONG nntp_connect(STRPTR nntp_host,UWORD nntp_port, STRPTR nntp_user,STRPTR nntp_pass, UBYTE *errstr, ULONG errstr_size, struct Library *SocketBase);
GLOBAL VOID nntp_close( LONG sockfd, struct Library *SocketBase);
GLOBAL LONG nntp_auth_user( LONG sockfd, STRPTR auth_user, UBYTE *errstr, ULONG errstr_size, struct Library *SocketBase);
GLOBAL LONG nntp_auth_pass( LONG sockfd, STRPTR auth_pass, UBYTE *errstr, ULONG errstr_size, struct Library *SocketBase);
GLOBAL LONG nntp_group( LONG sockfd, STRPTR group, UBYTE *errstr, ULONG errstr_size, struct Library *SocketBase);
GLOBAL LONG nntp_article( LONG sockfd, STRPTR article, UBYTE *errstr, ULONG errstr_size, struct Library *SocketBase);


#endif /* AMINZB_NNTP_H */
