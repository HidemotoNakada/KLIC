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
 * @(#)$Header: /mm2/home/cvs/bc-src/tgif/shape.e,v 1.6 2011/05/16 16:21:59 william Exp $
 */

#ifndef _SHAPE_E_
#define _SHAPE_E_

extern int	numShapes;
extern int	curShape;
extern int	shapeShadowInResource;
extern int	shapeShadowDx;
extern int	shapeShadowDy;

#ifdef _INCLUDE_FROM_SHAPE_C_
#undef extern
#define extern
#endif /*_INCLUDE_FROM_SHAPE_C_*/

extern void	CleanUpShape ARGS_DECL((void));
extern void	InitShape ARGS_DECL((void));
extern void	ShapeSubMenu ARGS_DECL((int Index));
extern void	RefreshShapeMenu ARGS_DECL((TgMenu*));
extern TgMenu	*CreateShapeMenu ARGS_DECL((TgMenu*, int X, int Y,
                                            TgMenuInfo*,
                                            int status_str_xlated));
extern int	ShapeMenu ARGS_DECL((int X, int Y, int TrackMenubar));
extern void	SetShapeShadow ARGS_DECL((void));

#ifdef _INCLUDE_FROM_SHAPE_C_
#undef extern
#ifndef _NO_RECURSIVE_EXTERN
#define extern extern
#endif /* ~_NO_RECURSIVE_EXTERN */
#endif /*_INCLUDE_FROM_SHAPE_C_*/

#endif /*_SHAPE_E_*/
