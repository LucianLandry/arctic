//--------------------------------------------------------------------------
//            aList.h - (Yet Another) intrusive list implementation
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

#ifndef ALIST_H
#define ALIST_H

// Our list implementation is double-linked.  Anybody who wants to go into
//  the list needs a ListElement member in their class (or, they may inherit
//  from ListElement).  This prevents insert operations from needing heap
//  allocations (and possibly failing with OOM).
// The ListElement does not need to be at the beginning of the class.  If it is
//  not, you must use the custom List() constructor (and stddef.h::offsetof() to
//  calculate the offset of the element in question.
// A ListElement may only be on one list at a time.  Its 'pOwner' field is used
//  for sanity-checking.  It also enables us to implicitly remove ourselves
//  from one list before adding ourselves to another.  The latter is why,
//  despite its limitations, we utilize this class instead of
//  boost::intrusive::list.

namespace arctic
{

class List;

class ListElement
{
    friend class List;
public:
    ListElement();
    ~ListElement();
    void Clear();
    void Print() const; // (debug) dump via printf()
private:
    List *pOwner;
    ListElement *pNext;
    ListElement *pPrev;
};

typedef void (*LIST_DEBUGFUNC)(void *);
typedef int (*LIST_COMPAREFUNC)(void *el1, void *el2);
typedef int (*LIST_FINDFUNC)(void *el1, void *userDefined);

class List
{
public:
    List();
    List(int listElemOffset, LIST_DEBUGFUNC debugFunc);
    ~List();
    void Clear(); // Removes every element from the list.

    // Insert list element 'myElem' into list (after 'myPrevElem').
    // If 'myPrevElem' == nullptr, inserts into head of list.
    void Insert(void *myElem, void *myPrevElem);

    // Insert list element 'myElem' into list (before 'myNextElem').
    // If 'myNextElem' == nullptr, inserts into tail of list.
    void InsertBefore(void *myElem, void *myNextElem);

    // Remove 'myElem' from a list.  'myElem' must be on the list, or nullptr.
    // If "myElem' == nullptr, is a no-op.
    // Returns the removed element.
    void *Remove(void *myElem);

    void *Pop();
    void Push(void *elem);
    void Enq(void *elem);
    
    void *Head() const;
    void *Tail() const;
    void *Next(void *elem) const;
    void *Previous(void *elem) const;

    int Length() const;
    int ElemOffset() const;
    
    void SortBy(LIST_COMPAREFUNC compareFunc);

    // Loops through each list element "listElem" until
    //  compareFunc('elem', listElem) <= 0.  Inserts 'elem' before "listElem",
    //  or at tail of list if no "listElem" was found.
    void InsertBy(LIST_COMPAREFUNC compareFunc, void *elem);
    
    // Returns the first element from the list that matches
    // findFunc(elem, userDefined), or nullptr if no element found.  
    void *FindBy(LIST_FINDFUNC findFunc, void *userDefined) const;

    // Like FindBy(), but also removes the element from the list.
    void *RemoveBy(LIST_FINDFUNC findFunc, void *userDefined);

    void MoveTo(List *destList, int numElements);
    void Print() const; // (debug) dump via printf()

private:
    int length;
    int elemOffset;
    ListElement *pHead;
    ListElement *pTail;
    LIST_DEBUGFUNC pDebugFunc;

    void sanityCheck() const;
    void sanityCheckElement(ListElement *elem) const;
}; 

// Loop syntactic sugar.
#define LIST_DOFOREACH(list, element) \
    for ((element) = (decltype(element)) (list)->Head(); \
         (element) != nullptr; \
         (element) = (decltype(element)) (list)->Next((element)))

inline int List::Length() const
{
    return length;
}

inline int List::ElemOffset() const
{
    return elemOffset;
}

inline void List::Push(void *elem)
{
    Insert(elem, nullptr);
}

inline void List::Enq(void *elem)
{
    Insert(elem, Tail());
}

} // end namespace 'arctic'

#endif // ALIST_H
