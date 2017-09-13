#ifndef METATILESET_H
#define METATILESET_H

#include "palette-map.h"
#include "tileset.h"
#include "map.h"
#include "metatile.h"

#define MAX_NUM_METATILES 256

class Metatileset {
public:
	enum Result { META_OK, META_NO_GFX, META_BAD_FILE, META_TOO_SHORT, META_NULL };
private:
	Tileset _tileset;
	Metatile *_metatiles[MAX_NUM_METATILES];
	size_t _num_metatiles;
	Result _result;
public:
	Metatileset();
	~Metatileset();
	size_t num_metatiles(void) const { return _num_metatiles; }
	Result result(void) const { return _result; }
	void clear(void);
	void draw_metatile(int x, int y, uint8_t id, bool z);
	uchar *print_rgb(const Map &map) const;
	Palette_Map::Result read_palette_map(const char *f);
	Tileset::Result read_graphics(const char *f, Tileset::Lighting l, bool skip_60_7f = false);
	Result read_metatiles(const char *f);
	static const char *error_message(Result result);
};

#endif
