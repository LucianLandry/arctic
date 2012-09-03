//--------------------------------------------------------------------------
//                  PieceCache.cpp - caches the piece images.
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

#include "aTypes.h"     // uint
#include "PieceCache.h"

using namespace juce;

PieceCache *gPieceCache; // global

typedef struct {
    int piece;
    const char *path;
} PieceMapT;

static Drawable *LoadSVGFromPath(String path)
{
    ScopedPointer<XmlElement> xml;
    xml = XmlDocument::parse(juce::File(path));
    return xml != nullptr ? Drawable::createFromSVG(*xml) : nullptr;
}

PieceCache::PieceCache()
{
    PieceMapT pieces[] = {
	{PAWN, "Chess_plt45.svg"},
	{BPAWN, "Chess_pdt45.svg"},
	{NIGHT, "Chess_nlt45.svg"},
	{BNIGHT, "Chess_ndt45.svg"},
	{BISHOP, "Chess_blt45.svg"},
	{BBISHOP, "Chess_bdt45.svg"},
	{ROOK, "Chess_rlt45.svg"},
	{BROOK, "Chess_rdt45.svg"},
	{QUEEN, "Chess_qlt45.svg"},
	{BQUEEN, "Chess_qdt45.svg"},
	{KING, "Chess_klt45.svg"},
	{BKING, "Chess_kdt45.svg"}
    };

    // FIXME this obviously needs to not be hardcoded
    String basePath = "/home/blandry/svg/";

    loaded = true; // assume the best

    for (uint i = 0; i < sizeof(pieces) / sizeof(PieceMapT); i++)
    {
	Drawable *img = LoadSVGFromPath(basePath + String(pieces[i].path));
	if (img == nullptr)
	{
	    loaded = false;
	}
	cache[pieces[i].piece] = img;
    }
}

PieceCache::~PieceCache()
{
}

bool PieceCache::initSucceeded()
{
    return loaded;
}

Drawable *PieceCache::getNew(int pieceType)
{
    Drawable *img;
    return
	(pieceType >= 0 && pieceType < NUM_PIECE_TYPES &&
	 (img = cache[pieceType]) != nullptr) ?
	img->createCopy() :
	nullptr;
}
