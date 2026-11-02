#pragma once

#include "SharpArt/VectorCanvas.h"

namespace SharpArt
{

// The handles of the shopping bag on the Wii Shop Channel's banner: an arch of two legs and a round top, in light blue (104 by 134),
// in darker blue (88 by 93), and the white edges behind them (116 by 145 and 96 by 101).
void DrawShopHandle(VectorCanvas& canvas);
void DrawShopSmallHandle(VectorCanvas& canvas);
void DrawShopHandleEdge(VectorCanvas& canvas);
void DrawShopSmallHandleEdge(VectorCanvas& canvas);

// The card of the banner's logo (131 by 127): a blue card with a white edge and the word Wii, seen in perspective; light or dark blue.
void DrawShopLogoCard(VectorCanvas& canvas);
void DrawShopDarkLogoCard(VectorCanvas& canvas);

}
