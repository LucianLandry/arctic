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

#ifndef LIST_H
#define LIST_H

#include <stddef.h> // offsetof()

// Our list implementation is double-linked.  Anybody who wants to go into
// the list needs a ListElementT in their struct.  This is to prevent
// insert (etc.) operations from failing with OOM.
//
// The ListElementT does not need to be at the beginning of the struct -- if
// it is not, you can use ListInitEx() to calculate the offset of the element
// in question.
// 
// A ListElementT can only be on one list at a time.
// Its ownership field is used for idiot-checking.  It also enables us to
// implicitly remove ourselves from one list before adding ourselves to
// another.
// For this reason, *any* list_element_t must be memset(0)d before use!

#ifdef __cplusplus
extern "C" {
#endif

typedef struct list_element_t
{
    struct list_t *pOwner;
    struct list_element_t *pNext;
    struct list_element_t *pPrev;
} ListElementT;

typedef void (*LIST_DEBUGFUNC)(ListElementT *);
typedef int (*LIST_COMPAREFUNC)(ListElementT *el1, ListElementT *el2);
typedef int (*LIST_FINDFUNC)(ListElementT *el1, void *userDefined);

typedef struct list_t
{
    int length;
    int elemOffset;
    LIST_DEBUGFUNC pDebugFunc;
    ListElementT *pHead;
    ListElementT *pTail;
} ListT; 

// Prototypes.

// Initialize list.
// It is guaranteed that memset(0) will work just as well.
#define ListInit(list) \
    privListInit((list), 0, NULL)
#define ListInitDebug(list, debugFunc) \
    privListInit((list), 0, (debugFunc))
#define ListInitEx(list, myStruct, myStructsListElementField, debugFunc) \
    privListInit((list), \
                 offsetof((myStruct), (myStructsListElementField)), \
                 (debugFunc))

// Loop syntactic sugar.
#define LIST_DOFOREACH(list, element) \
    for ((element) = ListHead((list)); \
         (element) != NULL; \
         (element) = ListNext((list), (element)))
                 
// Initialize list element.
// It is guaranteed that memset(0) will work just as well.
void ListElementInit(ListElementT *listElement);

// Private, do not use externally.
void privListInit(ListT *list, int listElemOffset, LIST_DEBUGFUNC debugFunc);

// Insert a list element 'myElem' into a list (after 'myPrevElem').
// If 'myPrevElem' == NULL, inserts into head of list.
void ListInsert(ListT *list, void *myElem, void *myPrevElem);
// Insert a list element 'myElem' into a list (before 'myNextElem').
// If 'myNextElem' == NULL, inserts into tail of list.
void ListInsertBefore(ListT *list, void *myElem, void *myNextElem);

// Returns the first element from the list that matches
// findFunc(elem, userDefined), or NULL if no element found.  
void *ListFindBy(ListT *list, LIST_FINDFUNC findFunc, void *userDefined);

// Remove 'myElem' from a list.  'myElem' must be on the list, or NULL.
// if "myElem' == NULL, is a no-op.
// Returns the removed element.
void *ListRemove(ListT *list, void *myElem);

// Like ListFindBy(), but also removes the element from the list.
void *ListRemoveBy(ListT *list, LIST_FINDFUNC findFunc, void *userDefined);

void *ListHead(ListT *list);
void *ListPop(ListT *list);
void ListEnq(ListT *list, void *elem);
void ListPush(ListT *list, void *elem);
void *ListNext(ListT *list, void *elem);
void *ListPrevious(ListT *list, void *elem);
int ListLength(ListT *list);
void ListSortBy(ListT *list, LIST_COMPAREFUNC compareFunc);
void ListMove(ListT *destList, ListT *sourceList, int numElements);

// debug.
void ListElementPrint(ListElementT *listElement);
void ListPrint(ListT *list); 

#ifdef __cplusplus
}
#endif

#endif // LIST_H
