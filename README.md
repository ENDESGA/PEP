<p align="center">
  <img width="725" height="383" alt="pep_logo" src="https://github.com/user-attachments/assets/9b8922e7-cc9f-45e8-8ec7-206d92d50e8d" />
</p>

# Prediction-Encoded Pixels
This format is specifically designed to be for low-color pixel art (<=16 colors works best, up to 256 colors is supported).

It uses **"Prediction by Partial Matching, Order-2"** compression, which is able to compress packed-palette-indices smaller than GIF, PNG, and QOI, while sacrificing a bit of time.
It's 2-10x slower than GIF/PNG/QOI (depending on the image), but often compresses the image 20-50% smaller than GIF/PNG (and multiple-times smaller than QOI).

If you care about compressed image size, this is for you. It's somewhere between GIF and WEBP (.webp can compress better at times, but it's painfully slow).

-------

### *( this is currently in the EXPERIMENTAL phase! )*

-------

## How to use it:

Use the C header like:
```c
#define PEP_IMPLEMENTATION
#include "PEP.h"
```

-------

# Results:


## tree1

<img width="112" height="96" alt="tree1" src="https://github.com/user-attachments/assets/c4ef770b-7ec0-4738-a86f-44c012dedf22" />

112x96 : 4 colors

| Format | Size (bytes) | Compression Ratio | Compression (ms) | Decompression (ms) |
|--------|-------------|-------------------|------------------|-------------------|
| **JPEG XL** | 652    | 0.015x |
| **WebP**    | 874    | 0.020x |
| **PEP**     | 901    | 0.021x | 0.383 | 0.412 |
| **PNG**     | 984    | 0.023x |
| **GIF**     | 1,047  | 0.024x |
| **QOI**     | 2,425  | 0.056x | 0.023 | 0.028 |
| **BMP**     | 43,130 | 1.00x  |


## font

<img width="192" height="144" alt="font" src="https://github.com/user-attachments/assets/f2c6a5bc-d516-4947-b888-491afe014a57" />

192x144 : 2 colors

| Format | Size (bytes) | Compression Ratio | Compression (ms) | Decompression (ms) |
|--------|-------------|-------------------|------------------|-------------------|
| **JPEG XL** | 913   | 0.255x |
| **WebP**    | 1,252 | 0.349x |
| **PNG**     | 1,318 | 0.368x |
| **PEP**     | 1,357 | 0.378x | 0.419 | 0.602 |
| **GIF**     | 1,919 | 0.535x |
| **BMP**     | 3,586 | 1.00x  |
| **QOI**     | 6,669 | 1.860x | 0.071 | 0.078 |


## nz_scene

<img width="640" height="200" alt="nz_scene" src="https://github.com/user-attachments/assets/24392eb5-888c-4d47-ae56-222c9f9e712c" />

640x200 : 251 colors

| Format | Size (bytes) | Compression Ratio | Compression (ms) | Decompression (ms) |
|--------|-------------|-------------------|------------------|-------------------|
| **WebP**    | 71,722  | 0.556x |
| **PEP**     | 73,542  | 0.570x | 25.652 | 32.121 |
| **JPEG XL** | 81,276  | 0.630x |
| **PNG**     | 84,657  | 0.656x |
| **GIF**     | 96,997  | 0.751x |
| **BMP**     | 129,078 | 1.00x  |
| **QOI**     | 180,533 | 1.399x | 1.03 | 1.004 |

-------

## Mini-documentation:

PEP is designed for games too, so the compression outputs a structure that has useful elements. The PEP.data pointer ONLY contains the bytes for the pixels.
This library doesn't have a BMP loader, so this is specifically designed for setups where the color bytes already exist.
You just feed it into pep_compress() with the correct in_format of the bytes, and then you're able to use it however you like! Often you just use pep_save() after compressing, and then pep_load() + pep_decompress() to access the image data.

```c
/*
pep_compress() parameters:
	uint32_t*  PIXEL_BYTES = raw RGBA/BGRA pixels
	uint16_t   WIDTH       = width of the image
	uint16_t   HEIGHT      = height of the image
	pep_format IN_FORMAT   = channel-order of PIXEL_BYTES, either pep_rgba or pep_bgra
	pep_format OUT_FORMAT  = channel-order of the new PEP, either pep_rgba or pep_bgra
returns:
	a pep struct
*/
pep p = pep_compress( PIXEL_BYTES, WIDTH, HEIGHT, IN_FORMAT, OUT_FORMAT );

/*
pep_decompress() parameters:
	pep*       IN_PEP     = pep struct-pointer to decompress
	pep_format OUT_FORMAT = channel-order of the new pixels, either pep_rgba or pep_bgra
returns:
	a uint32_t* with the uncompressed pixel data
*/
uint32_t* pixels = pep_decompress( IN_PEP, OUT_FORMAT );

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

