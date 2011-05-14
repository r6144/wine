/*
 * DIB driver GDI objects.
 *
 * Copyright 2011 Huw Davies
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <assert.h>
#include <stdlib.h>

#include "gdi_private.h"
#include "dibdrv.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dib);

/*
 *
 * Decompose the 16 ROP2s into an expression of the form
 *
 * D = (D & A) ^ X
 *
 * Where A and X depend only on P (and so can be precomputed).
 *
 *                                       A    X
 *
 * R2_BLACK         0                    0    0
 * R2_NOTMERGEPEN   ~(D | P)            ~P   ~P
 * R2_MASKNOTPEN    ~P & D              ~P    0
 * R2_NOTCOPYPEN    ~P                   0   ~P
 * R2_MASKPENNOT    P & ~D               P    P
 * R2_NOT           ~D                   1    1
 * R2_XORPEN        P ^ D                1    P
 * R2_NOTMASKPEN    ~(P & D)             P    1
 * R2_MASKPEN       P & D                P    0
 * R2_NOTXORPEN     ~(P ^ D)             1   ~P
 * R2_NOP           D                    1    0
 * R2_MERGENOTPEN   ~P | D               P   ~P
 * R2_COPYPEN       P                    0    P
 * R2_MERGEPENNOT   P | ~D              ~P    1
 * R2_MERGEPEN      P | D               ~P    P
 * R2_WHITE         1                    0    1
 *
 */

/* A = (P & A1) | (~P & A2) */
#define ZERO {0, 0}
#define ONE {0xffffffff, 0xffffffff}
#define P {0xffffffff, 0}
#define NOT_P {0, 0xffffffff}

static const DWORD rop2_and_array[16][2] =
{
    ZERO, NOT_P, NOT_P, ZERO,
    P,    ONE,   ONE,   P,
    P,    ONE,   ONE,   P,
    ZERO, NOT_P, NOT_P, ZERO
};

/* X = (P & X1) | (~P & X2) */
static const DWORD rop2_xor_array[16][2] =
{
    ZERO, NOT_P, ZERO, NOT_P,
    P,    ONE,   P,    ONE,
    ZERO, NOT_P, ZERO, NOT_P,
    P,    ONE,   P,    ONE
};

#undef NOT_P
#undef P
#undef ONE
#undef ZERO

void calc_and_xor_masks(INT rop, DWORD color, DWORD *and, DWORD *xor)
{
    /* NB The ROP2 codes start at one and the arrays are zero-based */
    *and = (color & rop2_and_array[rop-1][0]) | ((~color) & rop2_and_array[rop-1][1]);
    *xor = (color & rop2_xor_array[rop-1][0]) | ((~color) & rop2_xor_array[rop-1][1]);
}

static inline void order_end_points(int *s, int *e)
{
    if(*s > *e)
    {
        int tmp;
        tmp = *s + 1;
        *s = *e + 1;
        *e = tmp;
    }
}

static inline BOOL pt_in_rect( const RECT *rect, const POINT *pt )
{
    return ((pt->x >= rect->left) && (pt->x < rect->right) &&
            (pt->y >= rect->top) && (pt->y < rect->bottom));
}

#define Y_INCREASING_MASK 0x0f
#define X_INCREASING_MASK 0xc3
#define X_MAJOR_MASK      0x99
#define POS_SLOPE_MASK    0x33

static inline BOOL is_xmajor(DWORD octant)
{
    return octant & X_MAJOR_MASK;
}

static inline BOOL is_pos_slope(DWORD octant)
{
    return octant & POS_SLOPE_MASK;
}

static inline BOOL is_x_increasing(DWORD octant)
{
    return octant & X_INCREASING_MASK;
}

static inline BOOL is_y_increasing(DWORD octant)
{
    return octant & Y_INCREASING_MASK;
}

/**********************************************************************
 *                  get_octant_number
 *
 * Return the octant number starting clockwise from the +ve x-axis.
 */
static inline int get_octant_number(int dx, int dy)
{
    if(dy > 0)
        if(dx > 0)
            return ( dx >  dy) ? 1 : 2;
        else
            return (-dx >  dy) ? 4 : 3;
    else
        if(dx < 0)
            return (-dx > -dy) ? 5 : 6;
        else
            return ( dx > -dy) ? 8 : 7;
}

static inline DWORD get_octant_mask(int dx, int dy)
{
    return 1 << (get_octant_number(dx, dy) - 1);
}

static void solid_pen_line_callback(dibdrv_physdev *pdev, INT x, INT y)
{
    RECT rect;

    rect.left   = x;
    rect.right  = x + 1;
    rect.top    = y;
    rect.bottom = y + 1;
    pdev->dib.funcs->solid_rects(&pdev->dib, 1, &rect, pdev->pen_and, pdev->pen_xor);
    return;
}

#define OUT_LEFT    1
#define OUT_RIGHT   2
#define OUT_TOP     4
#define OUT_BOTTOM  8

static inline DWORD calc_outcode(const POINT *pt, const RECT *clip)
{
    DWORD out = 0;
    if(pt->x < clip->left)         out |= OUT_LEFT;
    else if(pt->x >= clip->right)  out |= OUT_RIGHT;
    if(pt->y < clip->top)          out |= OUT_TOP;
    else if(pt->y >= clip->bottom) out |= OUT_BOTTOM;

    return out;
}

typedef struct
{
    unsigned int dx, dy;
    int bias;
    DWORD octant;
} bres_params;

/******************************************************************************
 *                clip_line
 *
 * Clips the start and end points to a rectangle.
 *
 * Note, this treats the end point like the start point.  If the
 * caller doesn't want it displayed, it should exclude it.  If the end
 * point is clipped out, then the likelihood is that the new end point
 * should be displayed.
 *
 * Returns 0 if totally excluded, 1 if partially clipped and 2 if unclipped.
 *
 * This derivation is based on the comments in X.org's xserver/mi/mizerclip.c,
 * however the Bresenham error term is defined differently so the equations
 * will also differ.
 *
 * For x major lines we have 2dy >= err + bias > 2dy - 2dx
 *                           0   >= err + bias - 2dy > -2dx
 *
 * Note dx, dy, m and n are all +ve.
 *
 * Moving the start pt from x1 to x1 + m, we need to figure out y1 + n.
 *                     err = 2dy - dx + 2mdy - 2ndx
 *                      0 >= 2dy - dx + 2mdy - 2ndx + bias - 2dy > -2dx
 *                      0 >= 2mdy - 2ndx + bias - dx > -2dx
 *                      which of course will give exactly one solution for n,
 *                      so looking at the >= inequality
 *                      n >= (2mdy + bias - dx) / 2dx
 *                      n = ceiling((2mdy + bias - dx) / 2dx)
 *                        = (2mdy + bias + dx - 1) / 2dx (assuming division truncation)
 *
 * Moving start pt from y1 to y1 + n we need to figure out x1 + m - there may be several
 * solutions we pick the one that minimizes m (ie that first unlipped pt). As above:
 *                     0 >= 2mdy - 2ndx + bias - dx > -2dx
 *                  2mdy > 2ndx - bias - dx
 *                     m > (2ndx - bias - dx) / 2dy
 *                     m = floor((2ndx - bias - dx) / 2dy) + 1
 *                     m = (2ndx - bias - dx) / 2dy + 1
 *
 * Moving end pt from x2 to x2 - m, we need to figure out y2 - n
 *                  err = 2dy - dx + 2(dx - m)dy - 2(dy - n)dx
 *                      = 2dy - dx - 2mdy + 2ndx
 *                   0 >= 2dy - dx - 2mdy + 2ndx + bias - 2dy > -2dx
 *                   0 >= 2ndx - 2mdy + bias - dx > -2dx
 *                   again exactly one solution.
 *                   2ndx <= 2mdy - bias + dx
 *                   n = floor((2mdy - bias + dx) / 2dx)
 *                     = (2mdy - bias + dx) / 2dx
 *
 * Moving end pt from y2 to y2 - n when need x2 - m this time maximizing x2 - m so
 * mininizing m to include all of the points at y = y2 - n.  As above:
 *                  0 >= 2ndx - 2mdy + bias - dx > -2dx
 *               2mdy >= 2ndx + bias - dx
 *                   m = ceiling((2ndx + bias - dx) / 2dy)
 *                     = (2ndx + bias - dx - 1) / 2dy + 1
 *
 * For y major lines, symmetry (dx <-> dy and swap the cases over) gives:
 *
 * Moving start point from y1 to y1 + n find x1 + m
 *                     m = (2ndx + bias + dy - 1) / 2dy
 *
 * Moving start point from x1 to x1 + m find y1 + n
 *                     n = (2mdy - bias - dy) / 2ndx + 1
 *
 * Moving end point from y2 to y2 - n find x1 - m
 *                     m = (2ndx - bias + dy) / 2dy
 *
 * Moving end point from x2 to x2 - m find y2 - n
 *                     n = (2mdy + bias - dy - 1) / 2dx + 1
 */
static int clip_line(const POINT *start, const POINT *end, const RECT *clip,
                     const bres_params *params, POINT *pt1, POINT *pt2)
{
    unsigned int n, m;
    BOOL clipped = FALSE;
    DWORD start_oc, end_oc;
    const int bias = params->bias;
    const unsigned int dx = params->dx;
    const unsigned int dy = params->dy;
    const unsigned int two_dx = params->dx * 2;
    const unsigned int two_dy = params->dy * 2;
    const BOOL xmajor = is_xmajor(params->octant);
    const BOOL neg_slope = !is_pos_slope(params->octant);

    *pt1 = *start;
    *pt2 = *end;

    start_oc = calc_outcode(start, clip);
    end_oc = calc_outcode(end, clip);

    while(1)
    {
        if(start_oc == 0 && end_oc == 0) return clipped ? 1 : 2; /* trivial accept */
        if(start_oc & end_oc)            return 0; /* trivial reject */

        clipped = TRUE;
        if(start_oc & OUT_LEFT)
        {
            m = clip->left - start->x;
            if(xmajor)
                n = (m * two_dy + bias + dx - 1) / two_dx;
            else
                n = (m * two_dy - bias - dy) / two_dx + 1;

            pt1->x = clip->left;
            if(neg_slope) n = -n;
            pt1->y = start->y + n;
            start_oc = calc_outcode(pt1, clip);
        }
        else if(start_oc & OUT_RIGHT)
        {
            m = start->x - clip->right + 1;
            if(xmajor)
                n = (m * two_dy + bias + dx - 1) / two_dx;
            else
                n = (m * two_dy - bias - dy) / two_dx + 1;

            pt1->x = clip->right - 1;
            if(neg_slope) n = -n;
            pt1->y = start->y - n;
            start_oc = calc_outcode(pt1, clip);
        }
        else if(start_oc & OUT_TOP)
        {
            n = clip->top - start->y;
            if(xmajor)
                m = (n * two_dx - bias - dx) / two_dy + 1;
            else
                m = (n * two_dx + bias + dy - 1) / two_dy;

            pt1->y = clip->top;
            if(neg_slope) m = -m;
            pt1->x = start->x + m;
            start_oc = calc_outcode(pt1, clip);
        }
        else if(start_oc & OUT_BOTTOM)
        {
            n = start->y - clip->bottom + 1;
            if(xmajor)
                m = (n * two_dx - bias - dx) / two_dy + 1;
            else
                m = (n * two_dx + bias + dy - 1) / two_dy;

            pt1->y = clip->bottom - 1;
            if(neg_slope) m = -m;
            pt1->x = start->x - m;
            start_oc = calc_outcode(pt1, clip);
        }
        else if(end_oc & OUT_LEFT)
        {
            m = clip->left - end->x;
            if(xmajor)
                n = (m * two_dy - bias + dx) / two_dx;
            else
                n = (m * two_dy + bias - dy - 1) / two_dx + 1;

            pt2->x = clip->left;
            if(neg_slope) n = -n;
            pt2->y = end->y + n;
            end_oc = calc_outcode(pt2, clip);
        }
        else if(end_oc & OUT_RIGHT)
        {
            m = end->x - clip->right + 1;
            if(xmajor)
                n = (m * two_dy - bias + dx) / two_dx;
            else
                n = (m * two_dy + bias - dy - 1) / two_dx + 1;

            pt2->x = clip->right - 1;
            if(neg_slope) n = -n;
            pt2->y = end->y - n;
            end_oc = calc_outcode(pt2, clip);
        }
        else if(end_oc & OUT_TOP)
        {
            n = clip->top - end->y;
            if(xmajor)
                m = (n * two_dx + bias - dx - 1) / two_dy + 1;
            else
                m = (n * two_dx - bias + dy) / two_dy;

            pt2->y = clip->top;
            if(neg_slope) m = -m;
            pt2->x = end->x + m;
            end_oc = calc_outcode(pt2, clip);
        }
        else if(end_oc & OUT_BOTTOM)
        {
            n = end->y - clip->bottom + 1;
            if(xmajor)
                m = (n * two_dx + bias - dx - 1) / two_dy + 1;
            else
                m = (n * two_dx - bias + dy) / two_dy;

            pt2->y = clip->bottom - 1;
            if(neg_slope) m = -m;
            pt2->x = end->x - m;
            end_oc = calc_outcode(pt2, clip);
        }
    }
}

static void bres_line_with_bias(INT x1, INT y1, INT x2, INT y2, const bres_params *params, INT err,
                                BOOL last_pt, void (* callback)(dibdrv_physdev*,INT,INT), dibdrv_physdev *pdev)
{
    const int xadd = is_x_increasing(params->octant) ? 1 : -1;
    const int yadd = is_y_increasing(params->octant) ? 1 : -1;
    INT erradd;

    if (is_xmajor(params->octant))  /* line is "more horizontal" */
    {
        erradd = 2*params->dy - 2*params->dx;
        while(x1 != x2)
        {
            callback(pdev, x1, y1);
            if (err + params->bias > 0)
            {
                y1 += yadd;
                err += erradd;
            }
            else err += 2*params->dy;
            x1 += xadd;
        }
        if(last_pt) callback(pdev, x1, y1);
    }
    else   /* line is "more vertical" */
    {
        erradd = 2*params->dx - 2*params->dy;
        while(y1 != y2)
        {
            callback(pdev, x1, y1);
            if (err + params->bias > 0)
            {
                x1 += xadd;
                err += erradd;
            }
            else err += 2*params->dx;
            y1 += yadd;
        }
        if(last_pt) callback(pdev, x1, y1);
    }
}

static BOOL solid_pen_line(dibdrv_physdev *pdev, POINT *start, POINT *end)
{
    const WINEREGION *clip = get_wine_region(pdev->clip);

    if(start->y == end->y)
    {
        RECT rect;
        int i;

        rect.left   = start->x;
        rect.top    = start->y;
        rect.right  = end->x;
        rect.bottom = end->y + 1;
        order_end_points(&rect.left, &rect.right);
        for(i = 0; i < clip->numRects; i++)
        {
            if(clip->rects[i].top >= rect.bottom) break;
            if(clip->rects[i].bottom <= rect.top) continue;
            /* Optimize the unclipped case */
            if(clip->rects[i].left <= rect.left && clip->rects[i].right >= rect.right)
            {
                pdev->dib.funcs->solid_rects(&pdev->dib, 1, &rect, pdev->pen_and, pdev->pen_xor);
                break;
            }
            if(clip->rects[i].right > rect.left && clip->rects[i].left < rect.right)
            {
                RECT tmp = rect;
                tmp.left = max(rect.left, clip->rects[i].left);
                tmp.right = min(rect.right, clip->rects[i].right);
                pdev->dib.funcs->solid_rects(&pdev->dib, 1, &tmp, pdev->pen_and, pdev->pen_xor);
            }
        }
    }
    else if(start->x == end->x)
    {
        RECT rect;
        int i;

        rect.left   = start->x;
        rect.top    = start->y;
        rect.right  = end->x + 1;
        rect.bottom = end->y;
        order_end_points(&rect.top, &rect.bottom);
        for(i = 0; i < clip->numRects; i++)
        {
            /* Optimize unclipped case */
            if(clip->rects[i].top <= rect.top && clip->rects[i].bottom >= rect.bottom &&
               clip->rects[i].left <= rect.left && clip->rects[i].right >= rect.right)
            {
                pdev->dib.funcs->solid_rects(&pdev->dib, 1, &rect, pdev->pen_and, pdev->pen_xor);
                break;
            }
            if(clip->rects[i].top >= rect.bottom) break;
            if(clip->rects[i].bottom <= rect.top) continue;
            if(clip->rects[i].right > rect.left && clip->rects[i].left < rect.right)
            {
                RECT tmp = rect;
                tmp.top = max(rect.top, clip->rects[i].top);
                tmp.bottom = min(rect.bottom, clip->rects[i].bottom);
                pdev->dib.funcs->solid_rects(&pdev->dib, 1, &tmp, pdev->pen_and, pdev->pen_xor);
            }
        }
    }
    else
    {
        bres_params params;
        INT dx = end->x - start->x;
        INT dy = end->y - start->y;
        INT i;

        params.dx = abs(dx);
        params.dy = abs(dy);
        params.octant = get_octant_mask(dx, dy);
        /* Octants 3, 5, 6 and 8 take a bias */
        params.bias = (params.octant & 0xb4) ? 1 : 0;

        for(i = 0; i < clip->numRects; i++)
        {
            POINT clipped_start, clipped_end;
            int clip_status;
            clip_status = clip_line(start, end, clip->rects + i, &params, &clipped_start, &clipped_end);

            if(clip_status)
            {
                int m = abs(clipped_start.x - start->x);
                int n = abs(clipped_start.y - start->y);
                int err;
                BOOL last_pt = FALSE;

                if(is_xmajor(params.octant))
                    err = 2 * params.dy - params.dx + m * 2 * params.dy - n * 2 * params.dx;
                else
                    err = 2 * params.dx - params.dy + n * 2 * params.dx - m * 2 * params.dy;

                if(clip_status == 1 && (end->x != clipped_end.x || end->y != clipped_end.y)) last_pt = TRUE;

                bres_line_with_bias(clipped_start.x, clipped_start.y, clipped_end.x, clipped_end.y, &params,
                                    err, last_pt, solid_pen_line_callback, pdev);

                if(clip_status == 2) break; /* completely unclipped, so we can finish */
            }
        }

    }
    release_wine_region(pdev->clip);
    return TRUE;
}

void reset_dash_origin(dibdrv_physdev *pdev)
{
    pdev->dash_pos.cur_dash = 0;
    pdev->dash_pos.left_in_dash = pdev->pen_pattern.dashes[0];
    pdev->dash_pos.mark = TRUE;
}

static inline void skip_dash(dibdrv_physdev *pdev, unsigned int skip)
{
    skip %= pdev->pen_pattern.total_len;
    while(skip)
    {
        if(pdev->dash_pos.left_in_dash > skip)
        {
            pdev->dash_pos.left_in_dash -= skip;
            return;
        }
        skip -= pdev->dash_pos.left_in_dash;
        pdev->dash_pos.cur_dash++;
        if(pdev->dash_pos.cur_dash == pdev->pen_pattern.count) pdev->dash_pos.cur_dash = 0;
        pdev->dash_pos.left_in_dash = pdev->pen_pattern.dashes[pdev->dash_pos.cur_dash];
        pdev->dash_pos.mark = !pdev->dash_pos.mark;
    }
}

static inline void get_dash_colors(const dibdrv_physdev *pdev, DWORD *and, DWORD *xor)
{
    if(pdev->dash_pos.mark)
    {
        *and = pdev->pen_and;
        *xor = pdev->pen_xor;
    }
    else /* space */
    {
        *and = pdev->bkgnd_and;
        *xor = pdev->bkgnd_xor;
    }
}

static void dashed_pen_line_callback(dibdrv_physdev *pdev, INT x, INT y)
{
    RECT rect;
    DWORD and, xor;

    get_dash_colors(pdev, &and, &xor);
    skip_dash(pdev, 1);
    rect.left   = x;
    rect.right  = x + 1;
    rect.top    = y;
    rect.bottom = y + 1;
    pdev->dib.funcs->solid_rects(&pdev->dib, 1, &rect, and, xor);
    return;
}

static BOOL dashed_pen_line(dibdrv_physdev *pdev, POINT *start, POINT *end)
{
    const WINEREGION *clip = get_wine_region(pdev->clip);
    DWORD and, xor;
    int i, dash_len;
    RECT rect;
    const dash_pos start_pos = pdev->dash_pos;

    if(start->y == end->y) /* hline */
    {
        BOOL l_to_r;
        INT left, right, cur_x;

        rect.top = start->y;
        rect.bottom = start->y + 1;

        if(start->x <= end->x)
        {
            left = start->x;
            right = end->x - 1;
            l_to_r = TRUE;
        }
        else
        {
            left = end->x + 1;
            right = start->x;
            l_to_r = FALSE;
        }

        for(i = 0; i < clip->numRects; i++)
        {
            if(clip->rects[i].top > start->y) break;
            if(clip->rects[i].bottom <= start->y) continue;

            if(clip->rects[i].right > left && clip->rects[i].left <= right)
            {
                int clipped_left  = max(clip->rects[i].left, left);
                int clipped_right = min(clip->rects[i].right - 1, right);

                pdev->dash_pos = start_pos;

                if(l_to_r)
                {
                    cur_x = clipped_left;
                    if(cur_x != left)
                        skip_dash(pdev, clipped_left - left);

                    while(cur_x <= clipped_right)
                    {
                        get_dash_colors(pdev, &and, &xor);
                        dash_len = pdev->dash_pos.left_in_dash;
                        if(cur_x + dash_len > clipped_right + 1)
                            dash_len = clipped_right - cur_x + 1;
                        rect.left = cur_x;
                        rect.right = cur_x + dash_len;

                        pdev->dib.funcs->solid_rects(&pdev->dib, 1, &rect, and, xor);
                        cur_x += dash_len;
                        skip_dash(pdev, dash_len);
                    }
                }
                else
                {
                    cur_x = clipped_right;
                    if(cur_x != right)
                        skip_dash(pdev, right - clipped_right);

                    while(cur_x >= clipped_left)
                    {
                        get_dash_colors(pdev, &and, &xor);
                        dash_len = pdev->dash_pos.left_in_dash;
                        if(cur_x - dash_len < clipped_left - 1)
                            dash_len = cur_x - clipped_left + 1;
                        rect.left = cur_x - dash_len + 1;
                        rect.right = cur_x + 1;

                        pdev->dib.funcs->solid_rects(&pdev->dib, 1, &rect, and, xor);
                        cur_x -= dash_len;
                        skip_dash(pdev, dash_len);
                    }
                }
            }
        }
        pdev->dash_pos = start_pos;
        skip_dash(pdev, right - left + 1);
    }
    else if(start->x == end->x) /* vline */
    {
        BOOL t_to_b;
        INT top, bottom, cur_y;

        rect.left = start->x;
        rect.right = start->x + 1;

        if(start->y <= end->y)
        {
            top = start->y;
            bottom = end->y - 1;
            t_to_b = TRUE;
        }
        else
        {
            top = end->y + 1;
            bottom = start->y;
            t_to_b = FALSE;
        }

        for(i = 0; i < clip->numRects; i++)
        {
            if(clip->rects[i].top > bottom) break;
            if(clip->rects[i].bottom <= top) continue;
            if(clip->rects[i].right > start->x && clip->rects[i].left <= start->x)
            {
                int clipped_top    = max(clip->rects[i].top, top);
                int clipped_bottom = min(clip->rects[i].bottom - 1, bottom);

                pdev->dash_pos = start_pos;

                if(t_to_b)
                {
                    cur_y = clipped_top;
                    if(cur_y != top)
                        skip_dash(pdev, clipped_top - top);

                    while(cur_y <= clipped_bottom)
                    {
                        get_dash_colors(pdev, &and, &xor);
                        dash_len = pdev->dash_pos.left_in_dash;
                        if(cur_y + dash_len > clipped_bottom + 1)
                            dash_len = clipped_bottom - cur_y + 1;
                        rect.top = cur_y;
                        rect.bottom = cur_y + dash_len;

                        pdev->dib.funcs->solid_rects(&pdev->dib, 1, &rect, and, xor);
                        cur_y += dash_len;
                        skip_dash(pdev, dash_len);
                    }
                }
                else
                {
                    cur_y = clipped_bottom;
                    if(cur_y != bottom)
                        skip_dash(pdev, bottom - clipped_bottom);

                    while(cur_y >= clipped_top)
                    {
                        get_dash_colors(pdev, &and, &xor);
                        dash_len = pdev->dash_pos.left_in_dash;
                        if(cur_y - dash_len < clipped_top - 1)
                            dash_len = cur_y - clipped_top + 1;
                        rect.top = cur_y - dash_len + 1;
                        rect.bottom = cur_y + 1;

                        pdev->dib.funcs->solid_rects(&pdev->dib, 1, &rect, and, xor);
                        cur_y -= dash_len;
                        skip_dash(pdev, dash_len);
                    }
                }
            }
        }
        pdev->dash_pos = start_pos;
        skip_dash(pdev, bottom - top + 1);
    }
    else
    {
        bres_params params;
        INT dx = end->x - start->x;
        INT dy = end->y - start->y;
        INT i;

        params.dx = abs(dx);
        params.dy = abs(dy);
        params.octant = get_octant_mask(dx, dy);
        /* Octants 3, 5, 6 and 8 take a bias */
        params.bias = (params.octant & 0xb4) ? 1 : 0;

        for(i = 0; i < clip->numRects; i++)
        {
            POINT clipped_start, clipped_end;
            int clip_status;
            clip_status = clip_line(start, end, clip->rects + i, &params, &clipped_start, &clipped_end);

            if(clip_status)
            {
                int m = abs(clipped_start.x - start->x);
                int n = abs(clipped_start.y - start->y);
                int err;
                BOOL last_pt = FALSE;

                pdev->dash_pos = start_pos;

                if(is_xmajor(params.octant))
                {
                    err = 2 * params.dy - params.dx + m * 2 * params.dy - n * 2 * params.dx;
                    skip_dash(pdev, m);
                }
                else
                {
                    err = 2 * params.dx - params.dy + n * 2 * params.dx - m * 2 * params.dy;
                    skip_dash(pdev, n);
                }
                if(clip_status == 1 && (end->x != clipped_end.x || end->y != clipped_end.y)) last_pt = TRUE;

                bres_line_with_bias(clipped_start.x, clipped_start.y, clipped_end.x, clipped_end.y, &params,
                                    err, last_pt, dashed_pen_line_callback, pdev);

                if(clip_status == 2) break; /* completely unclipped, so we can finish */
            }
        }
        pdev->dash_pos = start_pos;
        if(is_xmajor(params.octant))
            skip_dash(pdev, params.dx);
        else
            skip_dash(pdev, params.dy);
    }

    release_wine_region(pdev->clip);
    return TRUE;
}

static BOOL null_pen_line(dibdrv_physdev *pdev, POINT *start, POINT *end)
{
    return TRUE;
}

static const dash_pattern dash_patterns[5] =
{
    {0, {0}, 0},                  /* PS_SOLID - a pseudo-pattern used to initialise unpatterned pens. */
    {2, {18, 6}, 24},             /* PS_DASH */
    {2, {3,  3}, 6},              /* PS_DOT */
    {4, {9, 6, 3, 6}, 24},        /* PS_DASHDOT */
    {6, {9, 3, 3, 3, 3, 3}, 24}   /* PS_DASHDOTDOT */
};

/***********************************************************************
 *           dibdrv_SelectPen
 */
HPEN CDECL dibdrv_SelectPen( PHYSDEV dev, HPEN hpen )
{
    PHYSDEV next = GET_NEXT_PHYSDEV( dev, pSelectPen );
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);
    LOGPEN logpen;
    DWORD style;

    TRACE("(%p, %p)\n", dev, hpen);

    if (!GetObjectW( hpen, sizeof(logpen), &logpen ))
    {
        /* must be an extended pen */
        EXTLOGPEN *elp;
        INT size = GetObjectW( hpen, 0, NULL );

        if (!size) return 0;

        elp = HeapAlloc( GetProcessHeap(), 0, size );

        GetObjectW( hpen, size, elp );
        /* FIXME: add support for user style pens */
        logpen.lopnStyle = elp->elpPenStyle;
        logpen.lopnWidth.x = elp->elpWidth;
        logpen.lopnWidth.y = 0;
        logpen.lopnColor = elp->elpColor;

        HeapFree( GetProcessHeap(), 0, elp );
    }

    if (hpen == GetStockObject( DC_PEN ))
        logpen.lopnColor = GetDCPenColor( dev->hdc );

    pdev->pen_color = pdev->dib.funcs->colorref_to_pixel(&pdev->dib, logpen.lopnColor);
    calc_and_xor_masks(GetROP2(dev->hdc), pdev->pen_color, &pdev->pen_and, &pdev->pen_xor);

    pdev->pen_pattern = dash_patterns[PS_SOLID];

    pdev->defer |= DEFER_PEN;

    style = logpen.lopnStyle & PS_STYLE_MASK;

    switch(style)
    {
    case PS_SOLID:
        if(logpen.lopnStyle & PS_GEOMETRIC) break;
        if(logpen.lopnWidth.x > 1) break;
        pdev->pen_line = solid_pen_line;
        pdev->defer &= ~DEFER_PEN;
        break;

    case PS_DASH:
    case PS_DOT:
    case PS_DASHDOT:
    case PS_DASHDOTDOT:
        if(logpen.lopnStyle & PS_GEOMETRIC) break;
        if(logpen.lopnWidth.x > 1) break;
        pdev->pen_line = dashed_pen_line;
        pdev->pen_pattern = dash_patterns[style];
        pdev->defer &= ~DEFER_PEN;
        break;

    case PS_NULL:
        pdev->pen_line = null_pen_line;
        pdev->defer &= ~DEFER_PEN;
        break;

    default:
        break;
    }

    return next->funcs->pSelectPen( next, hpen );
}

/***********************************************************************
 *           dibdrv_SetDCPenColor
 */
COLORREF CDECL dibdrv_SetDCPenColor( PHYSDEV dev, COLORREF color )
{
    PHYSDEV next = GET_NEXT_PHYSDEV( dev, pSetDCPenColor );
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);

    if (GetCurrentObject(dev->hdc, OBJ_PEN) == GetStockObject( DC_PEN ))
    {
        pdev->pen_color = pdev->dib.funcs->colorref_to_pixel(&pdev->dib, color);
        calc_and_xor_masks(GetROP2(dev->hdc), pdev->pen_color, &pdev->pen_and, &pdev->pen_xor);
    }

    return next->funcs->pSetDCPenColor( next, color );
}

/**********************************************************************
 *             solid_brush
 *
 * Fill a number of rectangles with the solid brush
 * FIXME: Should we insist l < r && t < b?  Currently we assume this.
 */
static BOOL solid_brush(dibdrv_physdev *pdev, int num, RECT *rects)
{
    int i, j;
    const WINEREGION *clip = get_wine_region(pdev->clip);

    for(i = 0; i < num; i++)
    {
        for(j = 0; j < clip->numRects; j++)
        {
            RECT rect = rects[i];

            /* Optimize unclipped case */
            if(clip->rects[j].top <= rect.top && clip->rects[j].bottom >= rect.bottom &&
               clip->rects[j].left <= rect.left && clip->rects[j].right >= rect.right)
            {
                pdev->dib.funcs->solid_rects(&pdev->dib, 1, &rect, pdev->brush_and, pdev->brush_xor);
                break;
            }

            if(clip->rects[j].top >= rect.bottom) break;
            if(clip->rects[j].bottom <= rect.top) continue;

            if(clip->rects[j].right > rect.left && clip->rects[j].left < rect.right)
            {
                rect.left   = max(rect.left,   clip->rects[j].left);
                rect.top    = max(rect.top,    clip->rects[j].top);
                rect.right  = min(rect.right,  clip->rects[j].right);
                rect.bottom = min(rect.bottom, clip->rects[j].bottom);

                pdev->dib.funcs->solid_rects(&pdev->dib, 1, &rect, pdev->brush_and, pdev->brush_xor);
            }
        }
    }
    release_wine_region(pdev->clip);
    return TRUE;
}

static void free_pattern_brush_bits( dibdrv_physdev *pdev )
{
    HeapFree(GetProcessHeap(), 0, pdev->brush_and_bits);
    HeapFree(GetProcessHeap(), 0, pdev->brush_xor_bits);
    pdev->brush_and_bits = NULL;
    pdev->brush_xor_bits = NULL;
}

void free_pattern_brush( dibdrv_physdev *pdev )
{
    free_pattern_brush_bits( pdev );
    free_dib_info( &pdev->brush_dib, TRUE );
}

static BOOL create_pattern_brush_bits(dibdrv_physdev *pdev)
{
    DWORD size = pdev->brush_dib.height * abs(pdev->brush_dib.stride);
    DWORD *brush_bits = pdev->brush_dib.bits;
    DWORD *and_bits, *xor_bits;

    assert(pdev->brush_and_bits == NULL);
    assert(pdev->brush_xor_bits == NULL);

    and_bits = pdev->brush_and_bits = HeapAlloc(GetProcessHeap(), 0, size);
    xor_bits = pdev->brush_xor_bits = HeapAlloc(GetProcessHeap(), 0, size);

    if(!and_bits || !xor_bits)
    {
        ERR("Failed to create pattern brush bits\n");
        free_pattern_brush_bits( pdev );
        return FALSE;
    }

    if(pdev->brush_dib.stride < 0)
        brush_bits = (DWORD*)((BYTE*)brush_bits + (pdev->brush_dib.height - 1) * pdev->brush_dib.stride);

    while(size)
    {
        calc_and_xor_masks(pdev->brush_rop, *brush_bits++, and_bits++, xor_bits++);
        size -= 4;
    }

    if(pdev->brush_dib.stride < 0)
    {
        /* Update the bits ptrs if the dib is bottom up.  The subtraction is because stride is -ve */
        pdev->brush_and_bits = (BYTE*)pdev->brush_and_bits - (pdev->brush_dib.height - 1) * pdev->brush_dib.stride;
        pdev->brush_xor_bits = (BYTE*)pdev->brush_xor_bits - (pdev->brush_dib.height - 1) * pdev->brush_dib.stride;
    }

    return TRUE;
}

/**********************************************************************
 *             pattern_brush
 *
 * Fill a number of rectangles with the pattern brush
 * FIXME: Should we insist l < r && t < b?  Currently we assume this.
 */
static BOOL pattern_brush(dibdrv_physdev *pdev, int num, RECT *rects)
{
    int i, j;
    const WINEREGION *clip;
    POINT origin;

    if(pdev->brush_and_bits == NULL)
        if(!create_pattern_brush_bits(pdev))
            return FALSE;

    GetBrushOrgEx(pdev->dev.hdc, &origin);

    clip = get_wine_region(pdev->clip);
    for(i = 0; i < num; i++)
    {
        for(j = 0; j < clip->numRects; j++)
        {
            RECT rect = rects[i];

            /* Optimize unclipped case */
            if(clip->rects[j].top <= rect.top && clip->rects[j].bottom >= rect.bottom &&
               clip->rects[j].left <= rect.left && clip->rects[j].right >= rect.right)
            {
                pdev->dib.funcs->pattern_rects(&pdev->dib, 1, &rect, &origin, &pdev->brush_dib, pdev->brush_and_bits, pdev->brush_xor_bits);
                break;
            }

            if(clip->rects[j].top >= rect.bottom) break;
            if(clip->rects[j].bottom <= rect.top) continue;

            if(clip->rects[j].right > rect.left && clip->rects[j].left < rect.right)
            {
                rect.left   = max(rect.left,   clip->rects[j].left);
                rect.top    = max(rect.top,    clip->rects[j].top);
                rect.right  = min(rect.right,  clip->rects[j].right);
                rect.bottom = min(rect.bottom, clip->rects[j].bottom);

                pdev->dib.funcs->pattern_rects(&pdev->dib, 1, &rect, &origin, &pdev->brush_dib, pdev->brush_and_bits, pdev->brush_xor_bits);
            }
        }
    }
    release_wine_region(pdev->clip);
    return TRUE;
}

static BOOL null_brush(dibdrv_physdev *pdev, int num, RECT *rects)
{
    return TRUE;
}

void update_brush_rop( dibdrv_physdev *pdev, INT rop )
{
    pdev->brush_rop = rop;
    if(pdev->brush_style == BS_SOLID)
        calc_and_xor_masks(rop, pdev->brush_color, &pdev->brush_and, &pdev->brush_xor);
    free_pattern_brush_bits( pdev );
}

/***********************************************************************
 *           dibdrv_SelectBrush
 */
HBRUSH CDECL dibdrv_SelectBrush( PHYSDEV dev, HBRUSH hbrush )
{
    PHYSDEV next = GET_NEXT_PHYSDEV( dev, pSelectBrush );
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);
    LOGBRUSH logbrush;

    TRACE("(%p, %p)\n", dev, hbrush);

    if (!GetObjectW( hbrush, sizeof(logbrush), &logbrush )) return 0;

    if (hbrush == GetStockObject( DC_BRUSH ))
        logbrush.lbColor = GetDCBrushColor( dev->hdc );

    pdev->brush_style = logbrush.lbStyle;

    pdev->defer |= DEFER_BRUSH;

    free_pattern_brush( pdev );

    switch(logbrush.lbStyle)
    {
    case BS_SOLID:
        pdev->brush_color = pdev->dib.funcs->colorref_to_pixel(&pdev->dib, logbrush.lbColor);
        calc_and_xor_masks(GetROP2(dev->hdc), pdev->brush_color, &pdev->brush_and, &pdev->brush_xor);
        pdev->brush_rects = solid_brush;
        pdev->defer &= ~DEFER_BRUSH;
        break;

    case BS_NULL:
        pdev->brush_rects = null_brush;
        pdev->defer &= ~DEFER_BRUSH;
        break;

    case BS_DIBPATTERN:
    {
        BITMAPINFOHEADER *bi = GlobalLock((HGLOBAL)logbrush.lbHatch);
        dib_info orig_dib;

        if(!bi) return NULL;
        if(init_dib_info_from_packed(&orig_dib, bi, LOWORD(logbrush.lbColor)))
        {
            copy_dib_color_info(&pdev->brush_dib, &pdev->dib);
            if(convert_dib(&pdev->brush_dib, &orig_dib))
            {
                pdev->brush_rects = pattern_brush;
                pdev->defer &= ~DEFER_BRUSH;
            }
            free_dib_info(&orig_dib, FALSE);
        }
        GlobalUnlock((HGLOBAL)logbrush.lbHatch);
        break;
    }

    default:
        break;
    }

    return next->funcs->pSelectBrush( next, hbrush );
}

/***********************************************************************
 *           dibdrv_SetDCBrushColor
 */
COLORREF CDECL dibdrv_SetDCBrushColor( PHYSDEV dev, COLORREF color )
{
    PHYSDEV next = GET_NEXT_PHYSDEV( dev, pSetDCBrushColor );
    dibdrv_physdev *pdev = get_dibdrv_pdev(dev);

    if (GetCurrentObject(dev->hdc, OBJ_BRUSH) == GetStockObject( DC_BRUSH ))
    {
        pdev->brush_color = pdev->dib.funcs->colorref_to_pixel(&pdev->dib, color);
        calc_and_xor_masks(GetROP2(dev->hdc), pdev->brush_color, &pdev->brush_and, &pdev->brush_xor);
    }

    return next->funcs->pSetDCBrushColor( next, color );
}
