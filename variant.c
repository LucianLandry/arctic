//--------------------------------------------------------------------------
//                 variant.c - (rudimentary) variant support
//                           -------------------
//  copyright            : (C) 2013 by Lucian Landry
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

#include "variant.h"

static VariantT gChess =
{
    .castling =
    {
	// White
	{ .start  = { .king = 4, .rookOO = 7, .rookOOO = 0 },
	  .endOO  = { .king = 6, .rook = 5 },
	  .endOOO = { .king = 2, .rook = 3 }
	},
	// Black
	{ .start  = { .king = 60, .rookOO = 63, .rookOOO = 56 },
	  .endOO  = { .king = 62, .rook = 61 },
	  .endOOO = { .king = 58, .rook = 59 }
	},
    }
};

#if 0 // unused currently
static VariantT gWild0 =
{
    .castling =
    {
	// White
	{ .start  = { .king = 4, .rookOO = 7, .rookOOO = 0 },
	  .endOO  = { .king = 6, .rook = 5 },
	  .endOOO = { .king = 2, .rook = 3 }
	},
	// Black
	{ .start  = { .king = 59, .rookOO = 56, .rookOOO = 63 },
	  .endOO  = { .king = 57, .rook = 58 },
	  .endOOO = { .king = 61, .rook = 60 }
	},
    }
};
#endif

VariantT *gVariant = &gChess;
