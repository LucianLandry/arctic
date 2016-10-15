//--------------------------------------------------------------------------
//                   PieceCache.h - caches the piece images.
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef PIECECACHE_H
#define PIECECACHE_H

#include "juce_gui_basics/juce_gui_basics.h" // juce::Drawable
#include "Piece.h"
#include "ref.h"

class PieceCache
{
public:
    PieceCache();
    ~PieceCache();
    bool InitSucceeded();
    juce::Drawable *GetNew(Piece piece);
private:
    juce::ScopedPointer<juce::Drawable> cache[kMaxPieces];
    bool loaded;
};

// For now this is meant to be used as a singleton class.
extern PieceCache *gPieceCache;

#endif // PIECECACHE_H
