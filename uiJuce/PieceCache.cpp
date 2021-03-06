//--------------------------------------------------------------------------
//                  PieceCache.cpp - caches the piece images.
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include "aTypes.h"     // uint
#include "PieceCache.h"

using namespace juce;

PieceCache *gPieceCache; // global

struct PieceMapT
{
    Piece piece;
    const char *path;
};

static Drawable *loadSVGFromPath(String path)
{
    ScopedPointer<XmlElement> xml;
    xml = XmlDocument::parse(juce::File(path));
    return xml != nullptr ? Drawable::createFromSVG(*xml) : nullptr;
}

PieceCache::PieceCache()
{
    PieceMapT pieceMap[] =
    {
        {Piece(0, PieceType::Pawn),   "Chess_plt45.svg"},
        {Piece(1, PieceType::Pawn),   "Chess_pdt45.svg"},
        {Piece(0, PieceType::Knight), "Chess_nlt45.svg"},
        {Piece(1, PieceType::Knight), "Chess_ndt45.svg"},
        {Piece(0, PieceType::Bishop), "Chess_blt45.svg"},
        {Piece(1, PieceType::Bishop), "Chess_bdt45.svg"},
        {Piece(0, PieceType::Rook),   "Chess_rlt45.svg"},
        {Piece(1, PieceType::Rook),   "Chess_rdt45.svg"},
        {Piece(0, PieceType::Queen),  "Chess_qlt45.svg"},
        {Piece(1, PieceType::Queen),  "Chess_qdt45.svg"},
        {Piece(0, PieceType::King),   "Chess_klt45.svg"},
        {Piece(1, PieceType::King),   "Chess_kdt45.svg"}
    };

    char *homePath = getenv("HOME");
    String basePath = homePath != nullptr ? homePath : "";
    basePath += "/svg";
    
    loaded = true; // assume the best

    for (uint i = 0; i < sizeof(pieceMap) / sizeof(PieceMapT); i++)
    {
        Drawable *img = loadSVGFromPath(basePath + "/" +
                                        String(pieceMap[i].path));
        if (img == nullptr)
            loaded = false;
        cache[pieceMap[i].piece.ToIndex()] = img;
    }
}

PieceCache::~PieceCache()
{
}

bool PieceCache::InitSucceeded()
{
    return loaded;
}

Drawable *PieceCache::GetNew(Piece piece)
{
    Drawable *img;
    int index = piece.ToIndex();
    return
        (index >= 0 && index < kMaxPieces &&
         (img = cache[index]) != nullptr) ?
        img->createCopy() :
        nullptr;
}
