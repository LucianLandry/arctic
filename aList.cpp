//--------------------------------------------------------------------------
//           aList.cpp - (Yet Another) intrusive list implementation
//                           -------------------
//  begin                : Sun Sep 10 2006
//  copyright            : (C) 2006 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include <assert.h>
#include <stdio.h>
#include <string.h> // memset()
#include <sys/param.h> // MIN()

#include "aList.h"

using arctic::ListElement;
using arctic::List;

// #define USE_SANITY_CHECK 1

// Bumps 'nativeElem' from beginning of structure to the location
//  of its actual list element.
static inline ListElement *toListElement(void *nativeElem, int offset)
{
    return (ListElement *) ((char *) nativeElem + offset);
}

// Converts a ListElement back to whatever native element type is being passed
//  around.
static inline void *toNativeElement(ListElement *elem, int offset)
{
    return (char *) elem - offset;
}

ListElement::ListElement()
{
    Clear();
}

void ListElement::Clear()
{
    memset(this, 0, sizeof(ListElement));
}

List::List()
{
    memset(this, 0, sizeof(List));
}

List::List(int listElemOffset, LIST_DEBUGFUNC debugFunc)
{
    memset(this, 0, sizeof(List));
    elemOffset = listElemOffset;
    pDebugFunc = debugFunc;
}

void List::Clear()
{
    length = 0;
    pHead = nullptr;
    pTail = nullptr;
}

void List::sanityCheck() const
{
    if (length == 0 || pHead == nullptr || pTail == nullptr)
    {
        // Sanity-check: empty-list conditions
        assert(length == 0 && pHead == nullptr && pTail == nullptr);
    }
    else
    {
        // Sanity-check: 'head' and 'tail' are sane.    
        assert(pHead->pPrev == nullptr);
        assert(pTail->pNext == nullptr);
    }
}

void List::sanityCheckElement(ListElement *elem) const
{
    if (elem)
    {
        assert(elem->pOwner == this);
        if (elem->pNext)
            assert(((ListElement *) elem->pNext)->pPrev == elem);
        else
            assert(pTail == elem);

        if (elem->pPrev)
            assert(((ListElement *) elem->pPrev)->pNext == elem);
        else
            assert(pHead == elem);
    }
}

// Insert a list element 'myElem' into a list (after 'myPrevElem').
// If 'myPrevElem' == nullptr, inserts into head of list.
void List::Insert(void *myElem, void *myPrevElem)
{
    ListElement *elem = (ListElement *) myElem;
    ListElement *prevElem = (ListElement *) myPrevElem;

    assert(myElem != nullptr);    
    if (elemOffset)
    {
        // Bump elem and prevElem from beginning of structure to the location
        // of their actual list elements.
        elem = toListElement(elem, elemOffset);
        if (prevElem != nullptr)
            prevElem = toListElement(prevElem, elemOffset);
    }

    // implicitly remove element from any previous list.
    if (elem->pOwner != nullptr)
        elem->pOwner->Remove(myElem);

    // Sanity checks.
#ifdef USE_SANITY_CHECK
    assert(elem->pOwner == nullptr);
    sanityCheck();
    sanityCheckElement(prevElem);
#endif
    
    // perform the actual insert.
    elem->pOwner = this;
    elem->pNext = prevElem ? prevElem->pNext : pHead;
    elem->pPrev = prevElem;

    if (elem->pNext)
        elem->pNext->pPrev = elem;
    else
        pTail = elem;

    if (prevElem)
        prevElem->pNext = elem;
    else
        pHead = elem;
    length++;       
}

void List::InsertBefore(void *myElem, void *myNextElem)
{
    if (myNextElem == nullptr)
        Enq(myElem);
    else
        Insert(myElem, Previous(myNextElem));
}

void *List::Remove(void *myElem)
{
    ListElement *elem;

    if (myElem == nullptr)
        return nullptr;
        
    // Bump elem from beginning of structure to the location
    // of its actual list element.
    elem = toListElement(myElem, elemOffset);
    
#ifdef USE_SANITY_CHECK
    // Sanity checks.
    sanityCheck();
    sanityCheckElement(elem);
#endif
    
    // perform the actual removal.    
    if (elem->pPrev)
        elem->pPrev->pNext = elem->pNext;
    if (elem->pNext)
        elem->pNext->pPrev = elem->pPrev;
    if (pHead == elem)
        pHead = elem->pNext;
    if (pTail == elem)
        pTail = elem->pPrev;
    length--;
        
    elem->pOwner = nullptr;
    elem->pNext = nullptr;  
    elem->pPrev = nullptr;    
    
    return toNativeElement(elem, elemOffset);
}

void *List::Pop()
{
    return pHead ?
        Remove(toNativeElement(pHead, elemOffset)) :
        nullptr;
}

void *List::Head() const
{
    return pHead ? toNativeElement(pHead, elemOffset) : nullptr;
}

void *List::Tail() const
{
    return pTail ? toNativeElement(pTail, elemOffset) : nullptr;
}

void ListElement::Print() const
{
    printf("{(ListElement) %p pOwner %p pNext %p pPrev %p}",
           this, pOwner, pNext, pPrev);
}

void List::Print() const
{
    printf("{(List) %p debugFunc %p offset %d len %d head %p tail %p",
           this, pDebugFunc, elemOffset, length,
           pHead, pTail);

    int i;
    ListElement *elem;

    for (i = 0, elem = pHead;
         i < length && elem;
         i++, elem = elem->pNext)
    {
        printf("\nel%d ", i);
        if (pDebugFunc)
            (*pDebugFunc)(toNativeElement(elem, elemOffset));
        else
            elem->Print();
    }
    printf("}");
}

void *List::Next(void *elem) const
{
    assert(elem != nullptr);
    ListElement *nextElem = toListElement(elem, elemOffset)->pNext;
    if (nextElem != nullptr)
        assert(nextElem->pOwner == this);
    return nextElem ? toNativeElement(nextElem, elemOffset) : nullptr;
}

void *List::Previous(void *elem) const
{
    assert(elem != nullptr);
    ListElement *prevElem = toListElement(elem, elemOffset)->pPrev;
    if (prevElem != nullptr)
        assert(prevElem->pOwner == this);
    return prevElem ? toNativeElement(prevElem, elemOffset) : nullptr;
}

void List::SortBy(LIST_COMPAREFUNC compareFunc)
{
    void *lastSortedElem = Head(), *el1, *el2;
    int i, j; // loop counters
    int len = Length();
                    
    // attempt an insert sort.  This is O(N^2).
    // For every element at index 'i'...
    for (i = 1; i < len; i++)
    {
        el2 = Next(lastSortedElem);
        // optimistic: assume the element does not need to be inserted.
        lastSortedElem = el2; 
                
        // We see if it goes before any other list element;
        // if it does we extract it then insert it into the proper
        // place.  Note that anything in 'userDefined' is not altered!
        for (j = 0, el1 = Head();
             j < i;
             j++, el1 = Next(el1))
        {
            if (compareFunc(el1, el2) > 0)
            {
                // The elements are out of order.  Put 'el2' before 'el1'.
                lastSortedElem = Previous(el2);
                Remove(el2);
                InsertBefore(el2, el1);
                break;
            }
        }
    }
}

// Returns the first element from the list that matches
// findFunc(elem, userDefined), or nullptr if no element found.  
void *List::FindBy(LIST_FINDFUNC findFunc, void *userDefined) const
{
    void *el1;
    LIST_DOFOREACH(this, el1)
    {
        if (findFunc(el1, userDefined) == 0)
            return el1;
    }    
    return nullptr;
}

void *List::RemoveBy(LIST_FINDFUNC findFunc, void *userDefined)
{
    return Remove(FindBy(findFunc, userDefined));
}

void List::MoveTo(List *destList, int numElements)
{
    numElements = MIN(numElements, Length());
    
    for (int i = 0; i < numElements; i++)
        destList->Enq(Pop());
}
