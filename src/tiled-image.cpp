#pragma warning(push, 0)
#include <FL/fl_utf8.h>
#include <FL/Fl_PNG_Image.H>
#pragma warning(pop)

#include "utils.h"
#include "tileset.h"
#include "tiled-image.h"

Tiled_Image::Tiled_Image(const char *f) : _tile_hues(NULL), _num_tiles(0), _result(IMG_NULL) {
	if (ends_with(f, ".png")) { read_png_graphics(f); }
	else if (ends_with(f, ".2bpp")) { read_2bpp_graphics(f); }
	else if (ends_with(f, ".2bpp.lz")) { read_lz_graphics(f); }
}

Tiled_Image::~Tiled_Image() {
	delete [] _tile_hues;
}

Tiled_Image::Result Tiled_Image::read_png_graphics(const char *f) {
	Fl_PNG_Image png(f);
	if (png.fail()) { return (_result = IMG_BAD_FILE); }

	int w = png.w(), h = png.h();
	if (w % TILE_SIZE || h % TILE_SIZE) { return (_result = IMG_BAD_DIMS); }

	w /= TILE_SIZE;
	h /= TILE_SIZE;
	_num_tiles = w * h;
	if (_num_tiles > MAX_NUM_TILES) { return (_result = IMG_TOO_LARGE); }

	png.desaturate();
	if (png.count() != 1) { return (_result = IMG_NOT_GRAYSCALE); }

	delete [] _tile_hues;
	_tile_hues = new Hue[_num_tiles * TILE_AREA]();

	size_t hi = 0;
	int d = png.d();
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			for (int ty = 0; ty < TILE_SIZE; ty++) {
				for (int tx = 0; tx < TILE_SIZE; tx++) {
					long ti = ((y * TILE_SIZE + ty) * (w * TILE_SIZE) + (x * TILE_SIZE + tx)) * d;
					_tile_hues[hi++] = Color::mono_hue(png.array[ti]);
				}
			}
		}
	}

	return (_result = IMG_OK);
}

Tiled_Image::Result Tiled_Image::read_2bpp_graphics(const char *f) {
	FILE *file = fl_fopen(f, "rb");
	if (!file) { return (_result = IMG_BAD_FILE); }

	fseek(file, 0, SEEK_END);
	long n = ftell(file);
	rewind(file);
	if (n % BYTES_PER_2BPP_TILE) { fclose(file); return (_result = IMG_BAD_DIMS); }

	uchar *data = new uchar[n];
	size_t r = fread(data, 1, n, file);
	fclose(file);
	if (r != (size_t)n) { delete [] data; return (_result = IMG_BAD_FILE); }

	return (_result = parse_2bpp_data(n, data));
}

// A rundown of Pokemon Crystal's LZ compression scheme:
enum Lz_Command {
	// Control commands occupy bits 5-7.
	// Bits 0-4 serve as the first parameter n for each command.
	LZ_LITERAL,   // n values for n bytes
	LZ_ITERATE,   // one value for n bytes
	LZ_ALTERNATE, // alternate two values for n bytes
	LZ_BLANK,     // zero for n bytes
	// Repeater commands repeat any data that was just decompressed.
	// They take an additional signed parameter s to mark a relative starting point.
	// These wrap around (positive from the start, negative from the current position).
	LZ_REPEAT,    // n bytes starting from s
	LZ_FLIP,      // n bytes in reverse bit order starting from s
	LZ_REVERSE,   // n bytes backwards starting from s
	// The long command is used when 5 bits aren't enough. Bits 2-4 contain a new control code.
	// Bits 0-1 are appended to a new byte as 8-9, allowing a 10-bit parameter.
	LZ_LONG       // n is now 10 bits for a new control code
};

// If 0xff is encountered instead of a command, decompression ends.
#define LZ_END 0xff

// [sum(((b >> i) & 1) << (7 - i) for i in range(8)) for b in range(256)]
static uchar bit_flipped[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

Tiled_Image::Result Tiled_Image::read_lz_graphics(const char *f) {
	FILE *file = fl_fopen(f, "rb");
	if (!file) { return (_result = IMG_BAD_FILE); }

	fseek(file, 0, SEEK_END);
	long n = ftell(file);
	rewind(file);
	uchar *lz_data = new uchar[n];
	size_t r = fread(lz_data, 1, n, file);
	fclose(file);
	if (r != (size_t)n) { delete [] lz_data; return (_result = IMG_BAD_FILE); }

	uchar *twobpp_data = new uchar[MAX_NUM_TILES * BYTES_PER_2BPP_TILE];
	size_t address = 0, marker = 0;
	uchar q[2];
	int offset;
	for (;;) {
		uchar b = lz_data[address++];
		if (b == LZ_END) { break; }
		if (marker >= MAX_NUM_TILES * BYTES_PER_2BPP_TILE) {
			delete [] lz_data;
			delete [] twobpp_data;
			return (_result = IMG_TOO_LARGE);
		}
		Lz_Command cmd = (Lz_Command)((b & 0xe0) >> 5);
		int length = 0;
		if (cmd == LZ_LONG) {
			cmd = (Lz_Command)((b & 0x1c) >> 2);
			length = (int)(b & 0x03) * 0x100;
			b = lz_data[address++];
			length += (int)b + 1;
		}
		else {
			length = (int)(b & 0x1f) + 1;
		}
		switch (cmd) {
		case LZ_LITERAL:
			// Copy data directly.
			for (int i = 0; i < length; i++) {
				twobpp_data[marker++] = lz_data[address++];
			}
			break;
		case LZ_ITERATE:
			// Write one byte repeatedly.
			b = lz_data[address++];
			for (int i = 0; i < length; i++) {
				twobpp_data[marker++] = b;
			}
			break;
		case LZ_ALTERNATE:
			// Write alternating bytes.
			q[0] = lz_data[address++];
			q[1] = lz_data[address++];
			// Copy data directly.
			for (int i = 0; i < length; i++) {
				twobpp_data[marker++] = q[i & 1];
			}
			break;
		case LZ_BLANK:
			// Write zeros.
			for (int i = 0; i < length; i++) {
				twobpp_data[marker++] = 0;
			}
			break;
		case LZ_REPEAT:
			// Repeat bytes from output.
			b = lz_data[address++];
			offset = b >= 0x80 ? (int)marker - (int)(b & 0x7f) - 1 : (int)b * 0x100 + lz_data[address++];
			for (int i = 0; i < length; i++) {
				twobpp_data[marker++] = twobpp_data[offset + i];
			}
			break;
		case LZ_FLIP:
			// Repeat flipped bytes from output.
			b = lz_data[address++];
			offset = b >= 0x80 ? (int)marker - (int)(b & 0x7f) - 1 : (int)b * 0x100 + lz_data[address++];
			for (int i = 0; i < length; i++) {
				b = twobpp_data[offset + i];
				twobpp_data[marker++] = bit_flipped[b];
			}
			break;
		case LZ_REVERSE:
			// Repeat reversed bytes from output.
			b = lz_data[address++];
			offset = b >= 0x80 ? (int)marker - (int)(b & 0x7f) - 1 : (int)b * 0x100 + lz_data[address++];
			for (int i = 0; i < length; i++) {
				twobpp_data[marker++] = twobpp_data[offset - i];
			}
			break;
		case LZ_LONG:
		default:
			delete [] lz_data;
			delete [] twobpp_data;
			return (_result = IMG_BAD_CMD);
		}
	}

	delete [] lz_data;
	return (_result = parse_2bpp_data(marker, twobpp_data));
}

static void convert_2bytes_to_8hues(uchar b1, uchar b2, Hue *hues8) {
	// %ABCD_EFGH %abcd_efgh -> %Aa %Bb %Cc %Dd %Ee %Ff %GG %Hh
	for (int i = 0; i < 8; i++) {
		int j = 7 - i;
		hues8[i] = (Hue)((b1 >> j & 1) * 2 + (b2 >> j & 1));
	}
}

Tiled_Image::Result Tiled_Image::parse_2bpp_data(size_t n, uchar *data) {
	n /= BYTES_PER_2BPP_TILE;
	if (n > MAX_NUM_TILES) { delete [] data; return IMG_TOO_LARGE; }

	_num_tiles = n;
	delete [] _tile_hues;
	_tile_hues = new Hue[_num_tiles * TILE_AREA]();

	for (size_t i = 0; i < _num_tiles; i++) {
		for (int j = 0; j < TILE_SIZE; j++) {
			uchar b1 = data[i * BYTES_PER_2BPP_TILE + j * 2];
			uchar b2 = data[i * BYTES_PER_2BPP_TILE + j * 2 + 1];
			convert_2bytes_to_8hues(b1, b2, _tile_hues + (i * TILE_SIZE + j) * 8);
		}
	}

	delete [] data;
	return IMG_OK;
}
