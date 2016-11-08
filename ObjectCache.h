//--------------------------------------------------------------------------
//              ObjectCache.h - Object cache (for object reuse)
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef OBJECTCACHE_H
#define OBJECTCACHE_H

// An ObjectCache is not thread-safe.  You may consider using thread_local
//  ObjectCaches as a workaround.

// Do not tie a global variable's destructor to a thread-local ObjectCache,
//  since according to:
//  http://en.cppreference.com/w/cpp/utility/program/exit
//  ... thread-local destructors always run first, leaving the ObjectCache's
//  vector in an undefined (and unusable) state.

// maxObjects '0' implies no limit to the amount of objects in the cache.
// We pass this as a template parameter to optimize compilation, and because
//  we do not anticipate using different sizes for the same type (which would
//  result in code bloat).
template <typename objType, int maxObjects>
class ObjectCache
{
public:
    ~ObjectCache();
    objType *Alloc();
    // This variant fills in 'wasCached': 'true' implies a reused object was
    //  returned; 'false' implies a fresh object was returned.
    objType *Alloc(bool &wasCached);
    void Free(objType *obj);
private:
    std::vector<objType *> freeList;
};

template <typename objType, int maxObjects>
ObjectCache<objType, maxObjects>::~ObjectCache()
{
    while (!freeList.empty())
    {
        objType *obj = freeList.back();
        freeList.pop_back();
        delete obj;
    }
}

template <typename objType, int maxObjects>
inline objType *ObjectCache<objType, maxObjects>::Alloc(bool &wasCached)
{
    if (!freeList.empty())
    {
        objType *result = freeList.back();
        freeList.pop_back();
        wasCached = true;
        return result;
    }
    wasCached = false;
    return new objType;
}

template <typename objType, int maxObjects>
inline objType *ObjectCache<objType, maxObjects>::Alloc()
{
    bool ignore;
    return Alloc(ignore);
}

template <typename objType, int maxObjects>
inline void ObjectCache<objType, maxObjects>::Free(objType *obj)
{
    if (maxObjects == 0 || freeList.size() < maxObjects)
        freeList.push_back(obj);
    else
        delete obj;
}

#endif // OBJECTCACHE_H
