#ifndef _PERLIN_H_
#define _PERLIN_H_

#include <perlin/perlin.h>
#include <dragontype/number.h>

typedef enum
{
	SO_NONE,
	SO_HEIGHT,
	SO_MOUNTAIN,
	SO_OCEAN,
	SO_MOUNTAIN_HEIGHT,
	SO_BOULDER,
	SO_WETNESS,
	SO_TEXTURE_OFFSET_S,
	SO_TEXTURE_OFFSET_T,
	SO_TEMPERATURE,
	SO_VULCANO,
	SO_VULCANO_HEIGHT,
	SO_VULCANO_STONE,
	SO_VULCANO_CRATER_TOP,
	SO_HILLYNESS,
	SO_VOXELCTX,
	SO_OAKTREE,
	SO_OAKTREE_AREA,
	SO_PINETREE,
	SO_PINETREE_AREA,
	SO_PINETREE_HEIGHT,
	SO_PINETREE_BRANCH,
	SO_PINETREE_BRANCH_DIR,
	SO_PALMTREE,
	SO_PALMTREE_AREA,
} SeedOffset;

extern s32 seed;

#endif
