//--------------------------------------------------------------------------
//                   PieceCache.h - caches the piece images.
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU Library General Public License as
//   published by the Free Software Foundation; either version 2 of the
//   License, or (at your option) any later version.
//
//--------------------------------------------------------------------------

#ifndef PIECECACHE_H
#define PIECECACHE_H

#include "../AppConfig.h"
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
