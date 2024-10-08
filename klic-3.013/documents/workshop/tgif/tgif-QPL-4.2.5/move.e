/*
 * Author:      William Chia-Wei Cheng (bill.cheng@acm.org)
 *
 * Copyright (C) 2001-2009, William Chia-Wei Cheng.
 *
 * This file may be distributed under the terms of the Q Public License
 * as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * THIS FILE IS PROVIDED AS IS WITH NO WARRANTY OF ANY KIND, INCLUDING
 * THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * @(#)$Header: /mm2/home/cvs/bc-src/tgif/move.e,v 1.7 2011/05/16 16:21:58 william Exp $
 */

#ifndef _MOVE_E_
#define _MOVE_E_

extern int	oneMotionTimeout;
extern int	minMoveInterval;

#ifdef _INCLUDE_FROM_MOVE_C_
#undef extern
#define extern
#endif /*_INCLUDE_FROM_MOVE_C_*/

extern void	MoveObj ARGS_DECL((struct ObjRec *, int Dx, int Dy));
extern void	MoveAllSelObjects ARGS_DECL((int Dx, int Dy));
extern int	EndPtInSelected ARGS_DECL((int XOff, int YOff));
extern void	MoveAllSel ARGS_DECL((int Dx, int Dy));
extern void	MoveAnObj ARGS_DECL((struct ObjRec *ObjPtr,
		                     struct ObjRec *TopOwner, int Dx, int Dy));
extern void	MoveAnAttr ARGS_DECL((struct AttrRec *AttrPtr,
		                      struct ObjRec *AttrOwnerObj,
		                      int Dx, int Dy));
extern void	MoveSel ARGS_DECL((int Dx, int Dy, struct ObjRec *,
		                   XButtonEvent *));
extern void	FinishMoveVertexForStretchStructSpline ARGS_DECL((
				struct VSelRec *, int AbsDx, int AbsDy,
				StretchStructuredSplineInfo*));
extern void	MoveAllSelVs ARGS_DECL((int Dx, int Dy));
extern void	MoveSelVs ARGS_DECL((int OrigX, int OrigY));

#ifdef _INCLUDE_FROM_MOVE_C_
#undef extern
#ifndef _NO_RECURSIVE_EXTERN
#define extern extern
#endif /* ~_NO_RECURSIVE_EXTERN */
#endif /*_INCLUDE_FROM_MOVE_C_*/

#endif /*_MOVE_E_*/
