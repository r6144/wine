/*
 * DIBDRV in-memory bitmap list
 *
 * Copyright 2009 Massimo Del Fedele
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

/* this modules manages association between HBITMAP handles and DIBDRVBITMAP
 * physical bitmap objects. Needed mostly to get palettes from DIBs without
 * resorting to GetDIBColorTable() or GETDIBits(), which are DC dependent.
 * It makes also much easier (and faster) many bitmap operations */

#include "config.h"
#include "wine/port.h"

#include "dibdrv.h"

WINE_DEFAULT_DEBUG_CHANNEL(dibdrv);

static CRITICAL_SECTION BITMAPLIST_CritSection;
static CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &BITMAPLIST_CritSection,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": BITMAPLIST_CritSection") }
};
static CRITICAL_SECTION BITMAPLIST_CritSection = { &critsect_debug, -1, 0, 0, 0, 0 };

typedef struct _BITMAPLIST_NODE
{
    HBITMAP hbmp;
    DIBDRVBITMAP *bmp;
    UINT refCount;
    struct _BITMAPLIST_NODE *prev, *next;
    
} BITMAPLIST_NODE;

/* the list */
static BITMAPLIST_NODE *DIBDRV_BITMAPLIST;

/* initializes bitmap list -- to be called at process attach */
void _BITMAPLIST_Init(void)
{
    DIBDRV_BITMAPLIST = NULL;
}

/* terminates bitmap list -- to be called at process detach */
void _BITMAPLIST_Terminate(void)
{
    BITMAPLIST_NODE *curNode, *nextNode;

    EnterCriticalSection(&BITMAPLIST_CritSection);

    /* frees all stored bitmaps, if any left */
    curNode = DIBDRV_BITMAPLIST;
    while(curNode)
    {
        nextNode = curNode->next;
        ERR("Unfreed DIB found, handle is %p\n", curNode->hbmp);
        HeapFree(GetProcessHeap(), 0, curNode);
        curNode = nextNode;
    }
    DIBDRV_BITMAPLIST = NULL;
    LeaveCriticalSection(&BITMAPLIST_CritSection);
    DeleteCriticalSection(&BITMAPLIST_CritSection);
}

/* scan list for a DIB -- returns node containing it */
static BITMAPLIST_NODE *GetNode(HBITMAP hbmp)
{
    BITMAPLIST_NODE *node = DIBDRV_BITMAPLIST;
    while(node)
    {
        if(node->hbmp == hbmp)
            return node;
        node = node->next;
    }
    return NULL;
}

/* adds a DIB to the list - it adds it on top, as
   usually most recently created DIBs are used first */
BOOL _BITMAPLIST_Add(HBITMAP hbmp, DIBDRVBITMAP *bmp)
{
    BITMAPLIST_NODE *existNode, *node;
    
    EnterCriticalSection(&BITMAPLIST_CritSection);

    /* checks if already there */
    node = NULL;
    existNode = GetNode(hbmp);
    if(!existNode)
    {
        node = HeapAlloc(GetProcessHeap(), 0, sizeof(BITMAPLIST_NODE));
        if(!node)
            ERR("HeapAlloc failed\n");
        else
        {
            node->next = DIBDRV_BITMAPLIST;
            node->prev = NULL;
            DIBDRV_BITMAPLIST = node;
            if(node->next)
                node->next->prev = node;
            node->hbmp = hbmp;
            node->bmp = bmp;
        }
    }
    LeaveCriticalSection(&BITMAPLIST_CritSection);
    return !existNode && node;
}

/* removes a DIB from the list */
DIBDRVBITMAP *_BITMAPLIST_Remove(HBITMAP hbmp)
{
    BITMAPLIST_NODE *node;
    DIBDRVBITMAP *bmp;
    
    /* checks if already there */
    EnterCriticalSection(&BITMAPLIST_CritSection);
    node = GetNode(hbmp);
    if(node)
    {
        if(node->prev)
            node->prev->next = node->next;
        else
            DIBDRV_BITMAPLIST = node->next;
        if(node->next)
            node->next->prev = node->prev;
    }
    LeaveCriticalSection(&BITMAPLIST_CritSection);
    if(node)
    {
        bmp = node->bmp;
        HeapFree(GetProcessHeap(), 0, node);
    }
    else
        bmp = NULL;
    return bmp;    
}

/* scans list for a DIB */
DIBDRVBITMAP *_BITMAPLIST_Get(HBITMAP hbmp)
{
    BITMAPLIST_NODE *node;
    DIBDRVBITMAP *bmp;

    EnterCriticalSection(&BITMAPLIST_CritSection);
    node = GetNode(hbmp);
    if(!node)
        bmp = NULL;
    else
        bmp = node->bmp;
    LeaveCriticalSection(&BITMAPLIST_CritSection);
    return bmp;
}
