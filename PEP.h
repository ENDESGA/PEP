/////// /////// /////// /////// /////// /////// ///////
// Prediction-Encoded Pixels (.pep)
// version 0.2
// -------
// made with love by ENDESGA : 2025 : always CC0 : FOSS forever
// -------
// please contribute to make this format the best it can be!

// The .pep type is a pixel art format made to compress as small as possible.
// It uses a custom "Prediction by Partial Matching, Order-2" compression
// method designed from the ground up for pixel art that has a minimal palette.

/////// /////// /////// /////// /////// /////// ///////

// Might find a way to eliminate the use of memset to eliminate string.h,
// but for now these are required:
#include <stdint.h> // uint*_t
#include <stdlib.h> // mem-alloc
#include <stdio.h> // FILE
#include <string.h> // memset

// Copy and add this define ONCE before an `#include "PEP.h"`.
// This makes the compiler actually define the functions, allowing you to
// include this header multiple times for bigger build-tool projects.
/*
#define PEP_IMPLEMENTATION
*/
/////// /////// /////// /////// /////// /////// ///////

// .pep can automatically convert from RGBA to BGRA (little/big endian),
// which is often for low-level/backend rendering pipelines.
// Windows only supports ARGB for example, and so sometimes it's easier to
// make the whole application use that format.
typedef enum
{
	pep_rgba,
	pep_bgra,

	pep_abgr,
	pep_argb
}
pep_format;

// This is the main struct-type that contains values for using this format.
// The only weird one is `max_symbols`, which is a unique value per-image for
// how many prediction-symbols were used to compress it.
//
// `is_4bit` is something you can set after `pep_compress()` but before
// `pep_to_bytes()` which quantizes the palette colors to 4bits per channel,
// making the file slightly smaller, but limits the color-range.
typedef struct
{
	uint8_t* bytes;
	uint64_t bytes_size;
	uint16_t width;
	uint16_t height;
	pep_format format;
	uint32_t palette[ 256 ];
	uint8_t palette_size;
	uint8_t max_symbols;
	uint8_t is_4bit;
}
pep;

// This is the amount of frequencies per context, and the amount of contexts,
// with [256] being the order0 context.
// Originally there were 256*256 contexts, but I found the image didn't get
// much bigger with the same amount of frequencies. Theoretically the more
// contexts you have the smaller the image is...
#define PEP_FREQ_N 257
#define PEP_FREQ_END ( PEP_FREQ_N - 1 )
#define PEP_CONTEXTS_MAX PEP_FREQ_END

// These contants are for the 63bit arithmetic-coding, specifically not 64bit
// because of overflow.
#define PEP_ARITH_MAX 0x7fffffffffffffffu
#define PEP_ARITH_LOW 0x2000000000000000u
#define PEP_ARITH_MID 0x4000000000000000u
#define PEP_ARITH_HIGH 0x6000000000000000u

// During the compression process the context per frequency-group needs to be
// tracked, with the sum of all frequencies being stored.
typedef struct
{
	uint16_t freq[ PEP_FREQ_N ];
	uint32_t sum;
}
_pep_context;

// PEP_FREQ_MAX is the maximum accumulative frequency. I couldn't find specific
// information regarding what this could be in regards to an image. Huge values
// mostly work, but there seems to be an upper and lower boundary unique to
// each image. This could be improved...
// This current value came from pure brute-force. It used to involve the area
// of the image, and at some point it used the palette-size.
// I think the real solution is a pre-count of the frequency of each color, and
// have a look-up table that this uses. But that might counteract the
// optimization of making this smaller :shrugs:
#define PEP_FREQ_MAX ( PEP_FREQ_N << 3 )

// Sets the internal `accum` variable to the accumulated symbol-frequencies
#define PEP_ACCUM( FREQ_REF, SYMBOL ) for( uint64_t _accum = 0; _accum < ( SYMBOL ); _accum++ ) accum += FREQ_REF[ _accum ]

// This encodes a symbol into the arithmetic-coding range. It scales the
// current range based on the symbol's frequency and total frequency count.
// The delta check is for precision, if the range is too small we calculate
// differently to avoid rounding errors.
#define PEP_ENCODE( SYMBOL, TOTAL )\
	do\
	{\
		const uint64_t delta = high - low + 1;\
		if( delta < TOTAL )\
		{\
			high = low + ( accum + SYMBOL ) - 1;\
			low = low + accum;\
		}\
		else\
		{\
			high = low + ( delta / TOTAL ) * ( accum + SYMBOL ) + ( ( delta % TOTAL ) * ( accum + SYMBOL ) ) / TOTAL - 1;\
			low = low + ( delta / TOTAL ) * accum + ( ( delta % TOTAL ) * accum ) / TOTAL;\
		}\
	}\
	while( 0 )

// Update the frequency table after encoding/decoding a symbol.
// This increments the symbol's frequency and the total sum.
// When we hit freq_max, we scale everything down by half
// to keep the frequencies manageable.
// This dynamic part helps the compression adapt to the image's patterns.
#define PEP_UPDATE( CONTEXT, SYMBOL )\
	do\
	{\
		CONTEXT->freq[ SYMBOL ]++;\
		CONTEXT->sum++;\
		if( CONTEXT->sum > freq_max )\
		{\
			CONTEXT->sum = 0;\
			for( uint64_t f = 0; f < PEP_FREQ_N; f++ )\
			{\
				CONTEXT->freq[ f ] = ( CONTEXT->freq[ f ] + 1 ) >> 1;\
				CONTEXT->sum += CONTEXT->freq[ f ];\
			}\
		}\
	}\
	while( 0 )

// This calculates which symbol a value corresponds to during decompression.
// Similar to PEP_ENCODE it has two calculation methods depending on the range
// size to maintain precision.
#define PEP_TARGET( VALUE, TOTAL ) ( ( high - low + 1 < ( TOTAL ) ) ? ( VALUE - low ) : ( ( ( VALUE - low ) / ( ( high - low + 1 ) / ( TOTAL ) ) ) + ( ( ( VALUE - low ) % ( ( high - low + 1 ) / ( TOTAL ) ) ) * ( TOTAL ) ) / ( high - low + 1 ) ) )

// Outputs a single bit to the compressed stream. Writing a full byte when
// we've collected 8 bits.
#define PEP_BIT_OUT( BIT )\
	do\
	{\
		buffer = ( buffer >> 1 ) | ( ( BIT ) ? 0x80u : 0 );\
		if( --bits_left == 0 )\
		{\
			*data_ref++ = buffer;\
			bits_left = 8;\
			buffer = 0;\
		}\
	}\
	while( 0 )

// Reads a single bit from the compressed stream during decompression.
// Refills the buffer when empty and shifts the LSB out.
#define PEP_BIT_IN( VAR )\
	do\
	{\
		if( bits_left == 0 )\
		{\
			buffer = *data_ref++;\
			bits_left = 8;\
		}\
		VAR = ( VAR << 1 ) | ( buffer & 1 );\
		buffer >>= 1;\
		bits_left--;\
	}\
	while( 0 )

// Adjusts the arithmetic-coding range by removing a boundary value and
// scaling up by 2x.
// This is part of the renormalization process to keep our range from getting
// too small.
#define PEP_ADJUST_RANGE( BOUNDARY )\
	do\
	{\
		low = ( low - BOUNDARY ) << 1;\
		high = ( ( high - BOUNDARY ) << 1 ) | 1;\
	}\
	while( 0 )

// Outputs a bit along with any pending underflow bits.
// When we have underflow situations, we need to output the opposite bit
// for each underflow count.
// This handles the carry propagation in arithmetic-coding.
#define PEP_OUTPUT_UNDERFLOW( BIT, OPPOSITE_BIT )\
	do\
	{\
		PEP_BIT_OUT( BIT );\
		while( underflow > 0 )\
		{\
			PEP_BIT_OUT( OPPOSITE_BIT );\
			underflow--;\
		}\
	}\
	while( 0 )

// Renormalizes the range during compression.
// Keeps outputting bits and adjusting the range until it's a good size.
// The three conditions handle:
// 1) Both in lower half - output 0
// 2) Both in upper half - output 1  
// 3) In the middle - increment underflow counter
// This is the heart of making arithmetic-coding work with finite precision.
#define PEP_COMPRESS_RENORM()\
	do\
	{\
		while( 1 )\
		{\
			if( high < PEP_ARITH_MID )\
			{\
				PEP_OUTPUT_UNDERFLOW( 0, 1 );\
				low <<= 1;\
				high = ( high << 1 ) | 1;\
			}\
			else if( low >= PEP_ARITH_MID )\
			{\
				PEP_OUTPUT_UNDERFLOW( 1, 0 );\
				PEP_ADJUST_RANGE( PEP_ARITH_MID );\
			}\
			else if( low >= PEP_ARITH_LOW && high < PEP_ARITH_HIGH )\
			{\
				underflow++;\
				PEP_ADJUST_RANGE( PEP_ARITH_LOW );\
			}\
			else break;\
		}\
	}\
	while( 0 )

// Renormalizes during decompression. Similar to compression-renorm but
// instead of outputting bits, we're reading them in to match with what the
// compressor did.
#define PEP_DECOMPRESS_RENORM( VALUE )\
	do\
	{\
		while( 1 )\
		{\
			if( high < PEP_ARITH_MID )\
			{\
				low <<= 1;\
				high = ( high << 1 ) | 1;\
				PEP_BIT_IN( VALUE );\
			}\
			else if( low >= PEP_ARITH_MID )\
			{\
				PEP_ADJUST_RANGE( PEP_ARITH_MID );\
				VALUE = ( VALUE - PEP_ARITH_MID );\
				PEP_BIT_IN( VALUE );\
			}\
			else if( low >= PEP_ARITH_LOW && high < PEP_ARITH_HIGH )\
			{\
				PEP_ADJUST_RANGE( PEP_ARITH_LOW );\
				VALUE = ( VALUE - PEP_ARITH_LOW );\
				PEP_BIT_IN( VALUE );\
			}\
			else break;\
		}\
	}\
	while( 0 )

// How many bits do we need to fit N values?
#define bits_to_fit( N ) ( ( ( N ) <= 1 ) ? 1 : ( 32 - __builtin_clz( ( N ) - 1 ) ) )

/////// /////// /////// /////// /////// /////// ///////

static inline uint32_t _pep_reformat( const uint32_t in_color, const pep_format in_format, const pep_format out_format );
static inline pep pep_compress( const uint32_t* in_pixels, const uint16_t width, const uint16_t height, const pep_format in_format, const pep_format out_format );
static inline uint32_t* pep_decompress( const pep* const in_pep, const pep_format out_format, const uint8_t first_color_transparent );
static inline void pep_free( pep* in_pep );

static inline uint8_t* pep_serialize( const pep* in_pep, uint32_t* const out_size );
static inline pep pep_deserialize( const uint8_t* const in_bytes );

static inline uint8_t pep_save( const pep* const in_pep, const char* const file_path );
static inline pep pep_load( const char* const file_path );

/////// /////// /////// /////// /////// /////// ///////

#ifdef PEP_IMPLEMENTATION

// PEP supports "dynamic formats", where you can specify what the in-bytes are,
// and reformat to a different channel-order.
// This means two "identical" PEP files can have different formats, but you
// can choose how to reformat it when it decompresses!
static inline uint32_t _pep_reformat( const uint32_t in_color, const pep_format in_format, const pep_format out_format )
{
	if( in_format == out_format ) return in_color;

	if( in_format <= pep_bgra && out_format <= pep_bgra )
	{
		return ( in_color & 0x00ff00ff ) | ( ( in_color & 0xff000000 ) >> 16 ) | ( ( in_color & 0x0000ff00 ) << 16 );
	}
	else if( in_format >= pep_abgr && out_format >= pep_abgr )
	{
		return ( in_color & 0xff00ff00 ) | ( ( in_color & 0x00ff0000 ) >> 16 ) | ( ( in_color & 0x000000ff ) << 16 );
	}
	else if( ( in_format ^ out_format ) == 2 )
	{
		return ( ( in_color & 0x000000ff ) << 24 ) | ( ( in_color & 0x0000ff00 ) << 8 ) | ( ( in_color & 0x00ff0000 ) >> 8 ) | ( ( in_color & 0xff000000 ) >> 24 );
	}
	else if( in_format < out_format )
	{
		return ( ( in_color & 0x000000ff ) << 24 ) | ( ( in_color & 0xffffff00 ) >> 8 );
	}
	else
	{
		return ( ( in_color & 0xff000000 ) >> 24 ) | ( ( in_color & 0x00ffffff ) << 8 );
	}
}

// The format of the in_pixels has to be the same as in_format.
// out_format is the one applied to the newly compressed pep
static inline pep pep_compress( const uint32_t* in_pixels, const uint16_t width, const uint16_t height, const pep_format in_format, const pep_format out_format )
{
	pep out_pep = { 0 };
	uint32_t pixels_area = width * height;

	if( in_pixels == NULL || pixels_area == 0 ) return out_pep;

	const uint32_t* p = in_pixels;
	const uint32_t* p_end = p + pixels_area;

	out_pep.bytes = malloc( pixels_area * sizeof( uint32_t ) * 2 ); // zero chance it will be >2x the size
	out_pep.width = width;
	out_pep.height = height;
	out_pep.format = out_format;

	uint8_t* data_ref = out_pep.bytes;

	///////
	// palette construction

	uint32_t last_p = 0;
	uint32_t this_p = 0;
	uint32_t formatted_p = 0;

	while( p < p_end )
	{
		this_p = *p;

		if( p > in_pixels && this_p == last_p )
		{
			p++;
			continue;
		}

		formatted_p = _pep_reformat( this_p, in_format, out_format );

		uint16_t n = 0;
		while( n < out_pep.palette_size && formatted_p != out_pep.palette[ n ] )
		{
			n++;
		}

		if( n >= out_pep.palette_size && ( ( uint16_t )out_pep.palette_size + 1 ) < 256 )
		{
			out_pep.palette[ out_pep.palette_size++ ] = formatted_p;
		}

		last_p = this_p;
		p++;
	}

	///////
	// pixels to packed-palette-indices and PPM order-2 compression

	uint8_t bits_per_index = bits_to_fit( out_pep.palette_size );
	if( bits_per_index > 8 ) bits_per_index = 8; // only 8 bits in a byte

	const uint8_t indices_per_byte = 8 / bits_per_index;
	const uint8_t index_mask = ( 1 << bits_per_index ) - 1;

	static _pep_context contexts[ PEP_CONTEXTS_MAX + 1 ];
	memset( contexts, 0, sizeof( _pep_context ) * ( PEP_CONTEXTS_MAX + 1 ) );

	_pep_context* order0 = &contexts[ PEP_CONTEXTS_MAX ];
	for( uint64_t i = 0; i < PEP_FREQ_N; i++ ) order0->freq[ i ] = 1;
	order0->sum = PEP_FREQ_N;

	uint64_t low = 0;
	uint64_t high = PEP_ARITH_MAX;
	uint64_t underflow = 0;
	uint32_t context_id = 0;
	uint8_t buffer = 0;
	uint8_t bits_left = 8;
	uint64_t freq_max = PEP_FREQ_MAX;

	p = in_pixels;
	uint8_t indices_in_byte = 0;
	uint8_t symbol = 0;

	while( p < p_end || indices_in_byte > 0 )
	{
		if( p < p_end )
		{
			this_p = _pep_reformat( *p, in_format, out_format );
			uint16_t index = 0;
			while( index < out_pep.palette_size && this_p != out_pep.palette[ index ] )
			{
				if( ++index >= 256 )
				{
					index = 0;
					break;
				}
			}

			symbol |= ( index << ( indices_in_byte * bits_per_index ) );
			++indices_in_byte;
		}

		if( indices_in_byte >= indices_per_byte || ( p >= p_end && indices_in_byte > 0 ) )
		{
			uint64_t accum = 0;
			if( symbol > out_pep.max_symbols ) out_pep.max_symbols = symbol;
			_pep_context* const context_ref = &contexts[ context_id % PEP_CONTEXTS_MAX ];
			const uint32_t context_sum = context_ref->sum;

			if( context_sum != 0 && context_ref->freq[ symbol ] != 0 )
			{
				PEP_ACCUM( context_ref->freq, symbol );
				PEP_ENCODE( context_ref->freq[ symbol ], context_sum );
				PEP_UPDATE( context_ref, symbol );
			}
			else
			{
				if( context_sum != 0 )
				{
					accum = 0;
					PEP_ACCUM( context_ref->freq, PEP_FREQ_END );
					PEP_ENCODE( context_ref->freq[ PEP_FREQ_END ], context_sum );
					PEP_COMPRESS_RENORM();
				}

				accum = 0;
				PEP_ACCUM( order0->freq, symbol );
				PEP_ENCODE( order0->freq[ symbol ], order0->sum );

				if( context_sum == 0 )
				{
					context_ref->freq[ PEP_FREQ_END ] = 1;
					context_ref->sum = 1;
				}
				context_ref->freq[ symbol ] = 1;
				context_ref->sum++;
				PEP_UPDATE( order0, symbol );
			}
			PEP_COMPRESS_RENORM();
			context_id = ( ( context_id << 8 ) | symbol );

			symbol = 0;
			indices_in_byte = 0;
		}

		if( p < p_end )
		{
			++p;
		}
	}

	++underflow;
	if( low < PEP_ARITH_MID )
	{
		PEP_BIT_OUT( 0 );
		while( underflow > 0 )
		{
			PEP_BIT_OUT( 1 );
			underflow--;
		}
	}
	else
	{
		PEP_BIT_OUT( 1 );
		while( underflow > 0 )
		{
			PEP_BIT_OUT( 0 );
			underflow--;
		}
	}

	if( bits_left < 8 )
	{
		*data_ref++ = buffer >> bits_left;
	}

	out_pep.bytes_size = data_ref - out_pep.bytes;
	out_pep.bytes = ( uint8_t* )realloc( out_pep.bytes, out_pep.bytes_size );

	return out_pep;
}

// You can decompress a pep into any format via out_format, it will correctly
// do it for you via in_pep->format.
// If you want the first color to be 0 alpha, set transparent_first_color to 1
// otherwise just make it 0
static inline uint32_t* pep_decompress( const pep* const in_pep, const pep_format out_format, const uint8_t transparent_first_color )
{
	if( in_pep == NULL ) return NULL;
	if( in_pep->bytes == NULL || in_pep->bytes_size == 0 || in_pep->width == 0 || in_pep->height == 0 ) return NULL;

	const uint32_t area = in_pep->width * in_pep->height;
	uint8_t* data_ref = in_pep->bytes;
	uint32_t* out_pixels = ( uint32_t* )malloc( area * sizeof( uint32_t ) );

	uint64_t canvas_pos = 0;

	uint8_t bits_per_index = bits_to_fit( in_pep->palette_size );
	if( bits_per_index > 8 ) bits_per_index = 8; // only 8 bits in a byte

	const uint8_t indices_per_byte = 8 / bits_per_index;
	const uint8_t index_mask = ( 1 << bits_per_index ) - 1;

	static _pep_context contexts[ PEP_CONTEXTS_MAX + 1 ];
	memset( contexts, 0, sizeof( _pep_context ) * ( PEP_CONTEXTS_MAX + 1 ) );

	_pep_context* order0 = &contexts[ PEP_CONTEXTS_MAX ];
	for( uint64_t i = 0; i < PEP_FREQ_N; i++ ) order0->freq[ i ] = 1;
	order0->sum = PEP_FREQ_N;

	uint8_t buffer = 0;
	uint8_t bits_left = 0;
	uint64_t value = 0;

	for( char i = 0; i < 63; i++ )
	{
		PEP_BIT_IN( value );
	}

	///////
	// decompress PPM order-2 structure into packed-palette-indices

	uint64_t low = 0;
	uint64_t high = PEP_ARITH_MAX;
	uint32_t context_id = 0;
	const uint16_t max_symbols = in_pep->max_symbols + 1;
	uint64_t freq_max = PEP_FREQ_MAX;

	static uint32_t palette[ 256 ];
	memcpy( palette, in_pep->palette, in_pep->palette_size * sizeof( uint32_t ) );
	const uint64_t packed_indices_size = area / indices_per_byte;

	if( transparent_first_color != 0 )
	{
		if( in_pep->format <= pep_bgra )
		{
			palette[ 0 ] = palette[ 0 ] & 0xffffff00;
		}
		else
		{
			palette[ 0 ] = palette[ 0 ] & 0x00ffffff;
		}
	}

	for( uint64_t b = 0; b < packed_indices_size; b++ )
	{
		_pep_context* const context_ref = &contexts[ context_id % PEP_CONTEXTS_MAX ];
		const uint32_t context_sum = context_ref->sum;
		uint64_t target = PEP_TARGET( value, ( context_sum != 0 ) ? context_sum : 1 );
		uint64_t accum = 0;
		uint8_t symbol = 0;

		if( context_sum != 0 )
		{
			for( uint64_t s = 0; s < max_symbols; s++ )
			{
				uint16_t freq = context_ref->freq[ s ];
				if( freq )
				{
					if( accum + freq > target )
					{
						symbol = s;
						PEP_ENCODE( freq, context_sum );
						PEP_UPDATE( context_ref, s );
						goto done_decode;
					}
					accum += freq;
				}
			}

			if( context_ref->freq[ PEP_FREQ_END ] )
			{
				if( accum + context_ref->freq[ PEP_FREQ_END ] > target )
				{
					PEP_ENCODE( context_ref->freq[ PEP_FREQ_END ], context_sum );
					PEP_DECOMPRESS_RENORM( value );
				}
			}
		}

		target = PEP_TARGET( value, order0->sum );
		accum = 0;

		for( uint64_t j = 0; j < max_symbols; j++ )
		{
			accum += order0->freq[ j ];
			if( accum > target )
			{
				symbol = j;
				accum -= order0->freq[ j ];
				PEP_ENCODE( order0->freq[ j ], order0->sum );

				if( context_sum == 0 )
				{
					context_ref->freq[ PEP_FREQ_END ] = 1;
					context_ref->sum = 1;
				}
				context_ref->freq[ symbol ] = 1;
				context_ref->sum++;
				PEP_UPDATE( order0, symbol );
				break;
			}
		}

		done_decode:
		PEP_DECOMPRESS_RENORM( value );

		///////
		// convert packed-palette-indices to pixels

		if( indices_per_byte > 1 )
		{
			uint8_t indices_in_byte = 0;
			while( indices_in_byte < indices_per_byte && canvas_pos < area )
			{
				const uint8_t palette_idx = ( symbol >> ( indices_in_byte * bits_per_index ) ) & index_mask;
				out_pixels[ canvas_pos ] = _pep_reformat( palette[ palette_idx ], in_pep->format, out_format );
				++canvas_pos;
				++indices_in_byte;
			}
		}
		else
		{
			if( canvas_pos < area )
			{
				out_pixels[ canvas_pos ] = _pep_reformat( palette[ symbol ], in_pep->format, out_format );
				++canvas_pos;
			}
		}

		context_id = ( ( context_id << 8 ) | symbol );
	}

	return out_pixels;
}

static inline void pep_free( pep* in_pep )
{
	if( in_pep && in_pep->bytes )
	{
		free( in_pep->bytes );
		in_pep->bytes = NULL;
		in_pep->bytes_size = 0;
	}
}

///////

static inline uint8_t* pep_serialize( const pep* in_pep, uint32_t* const out_size )
{
	uint64_t palette_bytes = in_pep->is_4bit ? ( in_pep->palette_size * sizeof( uint32_t ) ) >> 1 : sizeof( uint32_t ) * in_pep->palette_size;
	uint64_t size = sizeof( uint32_t ) + ( sizeof( uint16_t ) << 1 ) + 3 * sizeof( uint8_t ) + palette_bytes + in_pep->bytes_size;

	uint8_t* out_bytes = ( uint8_t* )malloc( size );
	uint8_t* bytes_ref = out_bytes;

	// Pack bytes_size with is_4bit flag in highest bit
	// If your compressed image is >2GB you have bigger problems </3
	*( ( uint32_t* )bytes_ref ) = ( uint32_t ) ( in_pep->bytes_size | ( ( uint32_t ) in_pep->is_4bit << 31 ) );
	bytes_ref += sizeof( uint32_t );

	*( ( uint16_t* )bytes_ref ) = in_pep->width;
	bytes_ref += sizeof( uint16_t );

	*( ( uint16_t* )bytes_ref ) = in_pep->height;
	bytes_ref += sizeof( uint16_t );

	*bytes_ref++ = ( uint8_t ) in_pep->format;
	*bytes_ref++ = in_pep->palette_size;

	if( in_pep->palette_size )
	{
		if( in_pep->is_4bit )
		{
			// Pack 4-bit palette entries (2 bytes per color instead of 4)
			for( uint16_t i = 0; i < in_pep->palette_size; i++ )
			{
				uint32_t c = in_pep->palette[ i ];
				uint8_t rb = ( c >> 24 ) & 0xFF;
				uint8_t g = ( c >> 16 ) & 0xFF;
				uint8_t br = ( c >> 8 ) & 0xFF;
				uint8_t a = c & 0xFF;

				*bytes_ref++ = ( ( rb & 0xF0 ) >> 4 ) | ( g & 0xF0 );
				*bytes_ref++ = ( ( br & 0xF0 ) >> 4 ) | ( a & 0xF0 );
			}
		}
		else
		{
			memcpy( bytes_ref, &in_pep->palette[ 0 ], sizeof( uint32_t ) * in_pep->palette_size );
			bytes_ref += sizeof( uint32_t ) * in_pep->palette_size;
		}
	}

	*bytes_ref++ = in_pep->max_symbols;

	if( in_pep->bytes_size )
	{
		memcpy( bytes_ref, in_pep->bytes, in_pep->bytes_size );
	}

	*out_size = ( uint32_t )size;
	return out_bytes;
}

static inline pep pep_deserialize( const uint8_t* const in_bytes )
{
	pep out_pep = { 0 };
	const uint8_t* bytes_ref = in_bytes;

	uint32_t packed_data_size = *( ( uint32_t* )bytes_ref );
	out_pep.is_4bit = ( packed_data_size >> 31 ) & 1;
	out_pep.bytes_size = packed_data_size & 0x7FFFFFFF;
	bytes_ref += sizeof( uint32_t );

	out_pep.width = *( ( uint16_t* )bytes_ref );
	bytes_ref += sizeof( uint16_t );

	out_pep.height = *( ( uint16_t* )bytes_ref );
	bytes_ref += sizeof( uint16_t );

	out_pep.format = ( pep_format ) *bytes_ref++;
	out_pep.palette_size = *bytes_ref++;

	memset( out_pep.palette, 0, sizeof( uint32_t ) * 256 );

	if( out_pep.palette_size )
	{
		if( out_pep.is_4bit )
		{
			// Unpack 4-bit palette colors
			for( int i = 0; i < out_pep.palette_size; i++ )
			{
				uint8_t b1 = *bytes_ref++;
				uint8_t b2 = *bytes_ref++;

				uint8_t rb = ( b1 & 0x0F ) | ( ( b1 & 0x0F ) << 4 );
				uint8_t g = ( b1 & 0xF0 ) | ( ( b1 & 0xF0 ) >> 4 );
				uint8_t br = ( b2 & 0x0F ) | ( ( b2 & 0x0F ) << 4 );
				uint8_t a = ( b2 & 0xF0 ) | ( ( b2 & 0xF0 ) >> 4 );

				out_pep.palette[ i ] = ( ( uint32_t ) rb << 24 ) | ( ( uint32_t ) g << 16 ) | ( ( uint32_t ) br << 8 ) | ( uint32_t ) a;
			}
		}
		else
		{
			memcpy( &out_pep.palette[ 0 ], bytes_ref, sizeof( uint32_t ) * out_pep.palette_size );
			bytes_ref += sizeof( uint32_t ) * out_pep.palette_size;
		}
	}

	out_pep.max_symbols = *bytes_ref++;

	out_pep.bytes = ( out_pep.bytes_size != 0 ) ? ( uint8_t* )malloc( out_pep.bytes_size ) : NULL;
	if( out_pep.bytes_size )
	{
		memcpy( out_pep.bytes, bytes_ref, out_pep.bytes_size );
	}

	return out_pep;
}

///////

// For both save/load, file_path should end in ".pep":
// e.g. "texture.pep", "assets/image.pep"

// Saves pep into a file.
// Returns 0 on failure, 1 on success
static inline uint8_t pep_save( const pep* const in_pep, const char* const file_path )
{
	if( !in_pep || !file_path )
	{
		return 0;
	}

	uint32_t bytes_size = 0;
	uint8_t* bytes = pep_serialize( in_pep, &bytes_size );

	if( !bytes || bytes_size == 0 )
	{
		return 0;
	}

	FILE* file = fopen( file_path, "wb" );
	if( !file )
	{
		free( bytes );
		return 0;
	}

	size_t written = fwrite( bytes, 1, bytes_size, file );

	fclose( file );
	free( bytes );

	return written == bytes_size;
}

// Loads .pep file into returned pep struct
static inline pep pep_load( const char* const file_path )
{
	pep out_pep = { 0 };

	if( !file_path )
	{
		return out_pep;
	}

	FILE* file = fopen( file_path, "rb" );
	if( !file )
	{
		return out_pep;
	}

	fseek( file, 0, SEEK_END );
	long file_size = ftell( file );
	fseek( file, 0, SEEK_SET );

	if( file_size <= 0 )
	{
		fclose( file );
		return out_pep;
	}

	uint8_t* bytes = ( uint8_t* )malloc( file_size );

	size_t read = fread( bytes, 1, file_size, file );
	fclose( file );

	if( read != ( size_t )file_size )
	{
		free( bytes );
		return out_pep;
	}

	out_pep = pep_deserialize( bytes );
	free( bytes );

	return out_pep;
}

#endif

/////// /////// /////// /////// /////// /////// ///////

