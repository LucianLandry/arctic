/***************************************************************************
                     list.h - (Yet Another) list definition
                             -------------------
    begin                : Sun Sep 10 2006
    copyright            : (C) 2006 by Lucian Landry
    email                : lucian_b_landry@yahoo.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 ***************************************************************************/

#include <string.h> // memset()
#include <assert.h>
#include <stdio.h>
#include <sys/param.h> // MIN()
#include "list.h"

// Bumps 'nativeElem' from beginning of structure to the location
//  of its actual list element.
static inline ListElementT *toListElement(ListT *list, void *nativeElem)
{
    return (ListElementT *) ((char *) nativeElem + list->elemOffset);
}

// Converts a ListElementT back to whatever native element type is being passed
//  around.
static inline void *toNativeElement(ListT *list, ListElementT *elem)
{
    return (char *) elem - list->elemOffset;
}

// Initialize list element.
// It is guaranteed that memset(0) will work just as well.
void ListElementInit(ListElementT *listElement)
{
    memset(listElement, 0, sizeof(ListElementT));
}

// Initialize list.
void privListInit(ListT *list, int listElemOffset, LIST_DEBUGFUNC debugFunc)
{
    memset(list, 0, sizeof(ListT));
    list->elemOffset = listElemOffset;
    list->pDebugFunc = debugFunc;
}

void privListSanityCheck(ListT *list)
{
    if (list->length == 0 || list->pHead == NULL || list->pTail == NULL)
    {
        // Sanity-check: empty-list conditions
        assert(list->length == 0 && list->pHead == NULL &&
               list->pTail == NULL);
    }
    else
    {
        // Sanity-check: 'head' and 'tail' are sane.    
        assert(list->pHead->pPrev == NULL);
        assert(list->pTail->pNext == NULL);    
    }
}

void privListElementSanityCheck(ListT *list, ListElementT *elem)
{
    if (elem)
    {
        assert(elem->pOwner == list);
        if (elem->pNext)
        {
            assert(((ListElementT *) elem->pNext)->pPrev == elem);
        }
        else
        {
            assert(list->pTail == elem);
        }
        if (elem->pPrev)
        {
            assert(((ListElementT *) elem->pPrev)->pNext == elem);
        }
        else
        {
            assert(list->pHead == elem);
        }
    }
}


// Insert a list element 'myElem' into a list (after 'myPrevElem').
// If 'myPrevElem' == NULL, inserts into head of list.
void ListInsert(ListT *list, void *myElem, void *myPrevElem)
{
    ListElementT *elem = (ListElementT *) myElem;
    ListElementT *prevElem = (ListElementT *) myPrevElem;

    assert(myElem != NULL);    
    if (list->elemOffset)
    {
        // Bump elem and prevElem from beginning of structure to the location
        // of their actual list elements.
        elem = toListElement(list, elem);
        if (prevElem != NULL)
        {
            prevElem = toListElement(list, prevElem);
        }
    }

    // implicitly remove element from any previous list.
    if (elem->pOwner != NULL)
    {
        ListRemove(elem->pOwner, myElem);
    }

    // Sanity checks.
    assert(elem->pOwner == NULL);
    privListSanityCheck(list);
    privListElementSanityCheck(list, prevElem);

    // perform the actual insert.
    elem->pOwner = list;
    elem->pNext = prevElem ? prevElem->pNext : list->pHead;
    elem->pPrev = prevElem;

    if (elem->pNext)
    {
        elem->pNext->pPrev = elem;
    }
    else
    {
        list->pTail = elem;
    }
    if (prevElem)
    {
        prevElem->pNext = elem;
    }
    else
    {
        list->pHead = elem;
    }
    list->length++;       
}

// Insert a list element 'myElem' into a list (before 'myNextElem').
// If 'myNextElem' == NULL, inserts into tail of list.
void ListInsertBefore(ListT *list, void *myElem, void *myNextElem)
{
    if (myNextElem == NULL)
    {
        ListEnq(list, myElem);
    }
    else
    {
        ListInsert(list, myElem, ListPrevious(list, myNextElem));
    }
}


// Remove 'myElem' from a list.  'myElem' must be on the list, or NULL.
// if "myElem' == NULL, is a no-op.
// Returns the removed element.
void *ListRemove(ListT *list, void *myElem)
{
    ListElementT *elem;

    if (myElem == NULL)
    {
        return NULL;
    }
        
    // Bump elem from beginning of structure to the location
    // of its actual list element.
    elem = toListElement(list, myElem);
    
    // Sanity checks.
    privListSanityCheck(list);
    privListElementSanityCheck(list, elem);

    // perform the actual removal.    
    if (elem->pPrev)
    {
        elem->pPrev->pNext = elem->pNext;
    }
    if (elem->pNext)
    {
        elem->pNext->pPrev = elem->pPrev;
    }
    if (list->pHead == elem)
    {
        list->pHead = elem->pNext;
    }
    if (list->pTail == elem)
    {
        list->pTail = elem->pPrev;
    }
    list->length--;
        
    elem->pOwner = NULL;
    elem->pNext = NULL;  
    elem->pPrev = NULL;    
    
    return toNativeElement(list, elem);
}

void *ListPop(ListT *list)
{
    return list->pHead ?
        ListRemove(list, toNativeElement(list, list->pHead)) :
        NULL;
}

void *ListHead(ListT *list)
{
    return list->pHead ? toNativeElement(list, list->pHead) : NULL;
}

void ListPush(ListT *list, void *elem)
{
    ListInsert(list, elem, NULL);
}

void ListEnq(ListT *list, void *elem)
{
    return ListInsert(list, elem,
                      list->pTail ? toNativeElement(list, list->pTail) : NULL);
}

// debug.
void ListElementPrint(ListElementT *elem)
{
    printf("{(ListElementT) %p pOwner %p pNext %p pPrev %p}",
           elem, elem->pOwner, elem->pNext, elem->pPrev);
}

void ListPrint(ListT *list)
{
    int i;
    ListElementT *elem;
        
    printf("{(ListT) %p debugFunc %p offset %d len %d head %p tail %p",
           list, list->pDebugFunc, list->elemOffset, list->length,
           list->pHead, list->pTail);
    for (i = 0, elem = list->pHead;
         i < list->length && elem;
         i++, elem = elem->pNext)
    {
        printf("\nel%d ", i);
        if (list->pDebugFunc)
        {
            (*list->pDebugFunc)(toNativeElement(list, elem));
        }
        else
        {
            ListElementPrint(elem);
        }
    }
    printf("}");
}

void *ListNext(ListT *list, void *elem)
{
    ListElementT *nextElem;
    
    assert(elem != NULL);
    nextElem = toListElement(list, elem)->pNext;
    if (nextElem != NULL)
    {
        assert(nextElem->pOwner == list);
    }
    return nextElem ? toNativeElement(list, nextElem) : NULL;
}

void *ListPrevious(ListT *list, void *elem)
{
    ListElementT *prevElem;
     
    assert(elem != NULL);
    prevElem = toListElement(list, elem)->pPrev;
    if (prevElem != NULL)
    {
        assert(prevElem->pOwner == list);
    }
    return prevElem ? toNativeElement(list, prevElem) : NULL;
}

int ListLength(ListT *list)
{
    return list->length;
}

void ListSortBy(ListT *list, LIST_COMPAREFUNC compareFunc)
{
    void *lastSortedElem = ListHead(list), *el1, *el2;
    int i, j; // loop counters
    int len = ListLength(list);
                    
    // attempt an insert sort.  This is O(N^2).
    // For every element at index 'i'...
    for (i = 1; i < len; i++)
    {
        el2 = ListNext(list, lastSortedElem);
        // optimistic: assume the element does not need to be inserted.
        lastSortedElem = el2; 
                
        // We see if it goes before any other list element;
        // if it does we extract it then insert it into the proper
        // place.  Note that anything in 'userDefined' is not altered!
        for (j = 0, el1 = ListHead(list);
             j < i;
             j++, el1 = ListNext(list, el1))
        {
            if (compareFunc(el1, el2) > 0)
            {
                // The elements are out of order.  Put 'el2' before 'el1'.
                lastSortedElem = ListPrevious(list, el2);
                ListRemove(list, el2);
                ListInsertBefore(list, el2, el1);
                break;
            }
        }
    }
}

// Returns the first element from the list that matches
// findFunc(elem, userDefined), or NULL if no element found.  
void *ListFindBy(ListT *list, LIST_FINDFUNC findFunc, void *userDefined)
{
    void *el1;
    LIST_DOFOREACH(list, el1)
    {
        if (findFunc(el1, userDefined) == 0)
        {
            return el1;
        }
    }    
    return NULL;
}


// Like ListFindBy(), but also removes the element from the list.
void *ListRemoveBy(ListT *list, LIST_FINDFUNC findFunc, void *userDefined)
{
    return ListRemove(list, ListFindBy(list, findFunc, userDefined));
}


void ListMove(ListT *destList, ListT *sourceList, int numElements)
{
    int i;
    numElements = MIN(numElements, ListLength(sourceList));
    
    for (i = 0; i < numElements; i++)
    {
        ListEnq(destList, ListPop(sourceList));
    }
}
