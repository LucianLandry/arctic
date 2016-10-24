//--------------------------------------------------------------------------
//            list.h - (Yet Another) intrusive list implementation
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
// Its ownership field is used for sanity-checking.  It also enables us to
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

typedef void (*LIST_DEBUGFUNC)(void *);
typedef int (*LIST_COMPAREFUNC)(void *el1, void *el2);
typedef int (*LIST_FINDFUNC)(void *el1, void *userDefined);

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
#ifdef __cplusplus
#define LIST_DOFOREACH(list, element)  \
    for ((element) = (decltype(element)) ListHead((list)); \
         (element) != NULL; \
         (element) = (decltype(element)) ListNext((list), (element)))
#else
#define LIST_DOFOREACH(list, element)  \
    for ((element) = ListHead((list)); \
         (element) != NULL; \
         (element) = ListNext((list), (element)))
#endif
    
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
