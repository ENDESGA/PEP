<p align="center">
  <img width="725" height="383" alt="pep_logo" src="https://github.com/user-attachments/assets/db7a8101-77d9-4e85-9229-63a338fb8591" />
</p>

# Prediction-Encoded Pixels
This format is specifically designed to be for low-color pixel art (<=16 colors works best, up to 256 colors is supported).

It uses **"Prediction by Partial Matching"** compression, which is able to compress packed-palette-indices smaller than GIF, PNG, and QOI, while sacrificing a bit of time.
It's 2-10x slower than GIF/PNG (fast)/QOI (depending on the image), but often compresses the image 20-50% smaller than GIF/PNG (and multiple-times smaller than QOI).

If you care about compressed image size, this is for you. It's somewhere between GIF and WEBP (.webp can compress better at times, but it's painfully slow).

-------

### *( this is currently in the EXPERIMENTAL phase! )*

-------

## How to use it:

Use the C header like:
```c
#define PEP_IMPLEMENTATION
#include "pep.h"
```

-------

# Results:

> "PNG ***max***" is the maximum compression via pngslim, and is a lot slower (10s of seconds) than the usual "PNG ***fast***" that comes from exporting via Aseprite/magick/etc.

## tree1

<img width="112" height="96" alt="tree1" src="https://github.com/user-attachments/assets/c4ef770b-7ec0-4738-a86f-44c012dedf22" />

112x96 : 4 colors

| Format | Size (bytes) | Compression Ratio | Speed |
|--------|-------------|-------------------|-------|
| **PEP** | 849    | 0.155x | Fast |
| **PNG (max)** | 906    | 0.165x | Very Slow |
| **GIF** | 1,047  | 0.191x | Faster |
| **PNG (fast)** | 1,081    | 0.197x | Faster |
| **QOI** | 2,425  | 0.441x | Very Fast |
| **BMP** | 5,494 | 1.00x  | Very Fast |


## font

<img width="192" height="144" alt="font" src="https://github.com/user-attachments/assets/f2c6a5bc-d516-4947-b888-491afe014a57" />

192x144 : 2 colors

| Format | Size (bytes) | Compression Ratio | Speed |
|--------|-------------|-------------------|-------|
| **PNG (max)** | 1,256   | 0.350x | Very Slow |
| **PEP** | 1,298   | 0.362x | Fast |
| **PNG (fast)** | 1,835   | 0.512x | Faster |
| **GIF** | 1,919   | 0.535x | Faster |
| **BMP** | 3,586   | 1.00x  | Very Fast |
| **QOI** | 6,669   | 1.860x | Very Fast |


## nz_scene

<img width="640" height="200" alt="nz_scene" src="https://github.com/user-attachments/assets/24392eb5-888c-4d47-ae56-222c9f9e712c" />

640x200 : 251 colors

| Format | Size (bytes) | Compression Ratio | Speed |
|--------|-------------|-------------------|-------|
| **PEP** | 70,991 | 0.550x | Fast |
| **PNG (max)** | 81,038 | 0.628x | Very Slow |
| **PNG (fast)** | 85,375 | 0.661x | Faster |
| **GIF** | 96,997 | 0.751x | Faster |
| **BMP** | 129,078 | 1.00x | Very Fast |
| **QOI** | 180,533 | 1.399x | Very Fast |


## baboon

<img width="512" height="512" alt="baboon" src="https://github.com/user-attachments/assets/3766d769-2bb9-4498-b0da-6f36283bc59b" />

512x512 : 256 colors

| Format | Size (bytes) | Compression Ratio | Speed |
|--------|-------------|-------------------|-------|
| **PEP** | 154,967 | 0.589x | Fast |
| **PNG (max)** | 184,219 | 0.700x | Very Slow |
| **PNG (fast)** | 195,130 | 0.741x | Faster |
| **GIF** | 221,165 | 0.840x | Faster |
| **BMP** | 263,222 | 1.00x | Very Fast |
| **QOI** | 457,978 | 1.739x | Very Fast |

-------

## Mini-documentation:

pep is designed for games too, so the compression outputs a structure that has useful elements. The pep.data pointer ONLY contains the bytes for the pixels.
This library doesn't have a BMP loader, so this is specifically designed for setups where the color bytes already exist.
You just feed it into pep_compress() with the correct in_format of the bytes, and then you're able to use it however you like! Often you just use pep_save() after compressing, and then pep_load() + pep_decompress() to access the image data.

```c
/*
pep_compress() parameters:
	uint32_t*        PIXEL_BYTES = raw RGBA/BGRA pixels
	uint16_t         WIDTH       = width of the image
	uint16_t         HEIGHT      = height of the image
	pep_format       IN_FORMAT   = channel-byte-order of PIXEL_BYTES: pep_rgba, pep_bgra, pep_argb, or pep_abgr
	pep_channel_bits BITS  = bits-per-channel: pep_1bit, pep_2bit, pep_4bit, pep_8bit (default)
returns:
	a pep struct
*/
pep p = pep_compress( PIXEL_BYTES, WIDTH, HEIGHT, IN_FORMAT, BITS );

/*
pep_decompress() parameters:
	pep*       IN_PEP                  = pep struct-pointer to decompress
	pep_format OUT_FORMAT              = channel-byte-order of the new pixels: pep_rgba, pep_bgra, pep_argb, or pep_abgr
	uint8_t    FIRST_COLOR_TRANSPARENT = 0 or 1 to make the first color have 0 Alpha
returns:
	a uint32_t* with the uncompressed pixel data
*/
uint32_t* pixels = pep_decompress( IN_PEP, OUT_FORMAT, FIRST_COLOR_TRANSPARENT );

/*
pep_free() parameters:
	pep* IN_PEP = pep struct-pointer to free
note:
	frees the internal bytes buffer and resets bytes_size to 0
*/
pep_free( IN_PEP );

/*
pep_serialize() parameters:
	pep*      IN_PEP   = pep struct-pointer to serialize
	uint32_t* OUT_SIZE = pointer to store the resulting byte array size
returns:
	a uint8_t* byte array containing the serialized pep data
note:
	caller must free() the returned byte array when done
*/
uint8_t* bytes = pep_serialize( IN_PEP, OUT_SIZE );

/*
pep_deserialize() parameters:
	uint8_t* IN_BYTES = byte array containing serialized pep data
returns:
	a pep struct reconstructed from the byte array
*/
pep p = pep_deserialize( IN_BYTES );

/*
pep_save() parameters:
	pep*  IN_PEP    = pep struct-pointer to save
	char* FILE_PATH = path to save the .pep file (e.g. "image.pep")
returns:
	uint8_t - 1 on success, 0 on failure
*/
uint8_t success = pep_save( IN_PEP, FILE_PATH );

/*
pep_load() parameters:
	char* FILE_PATH = path to the .pep file to load (e.g. "image.pep")
returns:
	a pep struct loaded from the file
note:
	returns an empty pep struct on failure
*/
pep p = pep_load( FILE_PATH );
```

-------

### References

Though a lot of this was bashed together with love and tweaked with brute-force, many underlying structures were inspired/referenced from these sources:

1. **Wikipedia:** [Prediction by Partial Matching](https://en.wikipedia.org/wiki/Prediction_by_partial_matching).
2. **Cleary, J. G., & Witten, I. H. (1984):** [Data compression using adaptive coding and partial string matching](https://ieeexplore.ieee.org/document/1096090). *IEEE Transactions on Communications*, 32(4), 396-402.
3. **Moffat, A. (1990):** [Implementing the PPM data compression scheme](https://ieeexplore.ieee.org/document/61469/). *IEEE Transactions on Communications*, 38(11), 1917-1921.
4. **Cleary, J. G., Teahan, W. J., & Witten, I. H. (1995):** [Unbounded length contexts for PPM](https://ieeexplore.ieee.org/document/515495). *Proceedings DCC-95, IEEE Computer Society Press*, 52-61.
5. **Shkarin, D. (2002):** [PPM: One step to practicality](https://ieeexplore.ieee.org/document/1232887/). *Proceedings of Data Compression Conference 2002*, 202-211.

-------

Please contribute to make PEP the best pixel art format there is!

-End :::.

