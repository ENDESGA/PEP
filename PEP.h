/////// /////// /////// /////// /////// /////// ///////
// Prediction-Encoded Pixels (.pep)
// version 0.3
// -------
// made with love by ENDESGA : 2025 : always CC0 : FOSS forever
// -------
// please contribute to make this format the best it can be!

// The .pep type is a pixel art format made to compress as small as possible.
// It uses a custom "Prediction by Partial Matching, Order-2" compression
// method designed from the ground up for pixel art that has a minimal palette.

/////// /////// /////// /////// /////// /////// ///////

// Include guard.
#ifndef _PEP_H_
#define _PEP_H_

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

// Palette colors can be restricted in the serialization phase to a maximum
// amount of bits per channel.
// The default is 8 bits per channel (standard 32 bit colors)
typedef enum
{
	_pep_1bit,
	_pep_2bit,
	_pep_4bit,
	_pep_8bit
}
_pep_color_bits;

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
	_pep_color_bits color_bits;
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
#define PEP_CODE_BITS 24lu
#define PEP_FREQ_MAX_BITS 14lu
#define PEP_PROB_MAX_VALUE ( 1 << PEP_FREQ_MAX_BITS )
#define PEP_CODE_MAX_VALUE ( ( 1 << PEP_CODE_BITS ) - 1 )
#define PEP_ARITH_MAX ( ( 1 << PEP_CODE_BITS ) - 1 )
#define PEP_ARITH_LOW ( 1 << ( PEP_CODE_BITS - 2 ) )
#define PEP_ARITH_MID ( PEP_ARITH_LOW * 2 )
#define PEP_ARITH_HIGH ( PEP_ARITH_LOW * 3 )

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
#define PEP_FREQ_MAX ( PEP_FREQ_N << 2 )

// Arithmetic coding structures:
typedef struct
{
	uint8_t* data_ref;
	uint32_t low;
	uint32_t range;
}
_pep_ac_encode;

typedef struct
{
	uint8_t* data_ref;
	uint8_t* end_of_data;
	uint32_t low;
	uint32_t range;
	uint32_t code;
}
_pep_ac_decode;

typedef struct
{
	uint32_t high;
	uint32_t low;
	uint32_t scale;
}
_pep_prob;

typedef struct
{
	_pep_prob prob;
	uint32_t symbol;
}
_pep_sym_decode;

// Update the frequency table after encoding/decoding a symbol.
// This increments the symbol's frequency and the total sum.
// When we hit freq_max, we scale everything down to a quarter
// to keep the frequencies manageable.
// This dynamic part helps the compression adapt to the image's patterns.
#define PEP_UPDATE( CONTEXT, SYMBOL )\
	do\
	{\
		CONTEXT->freq[ SYMBOL ] += 2;\
		CONTEXT->sum += 2;\
		if( CONTEXT->freq[ SYMBOL ] > PEP_FREQ_MAX )\
		{\
			CONTEXT->sum = 0;\
			for( uint64_t f = 0; f < PEP_FREQ_N; f++ )\
			{\
				const uint16_t _f = CONTEXT->freq[ f ];\
				if( _f == 0 ) continue;\
				else if( _f <= 2 )\
				{\
					CONTEXT->freq[ f ] = 1;\
					CONTEXT->sum++;\
					continue;\
				}\
				CONTEXT->sum += ( CONTEXT->freq[ f ] = ( _f + 3 ) >> 2 );\
			}\
		}\
	}\
	while( 0 )

// This defines a set of macros that serve as wrappers for the standard
// C library memory management functions: `malloc`, `realloc`, and `free`.
// These macros can be used to easily replace the underlying memory allocation
// implementation in a project, for example, with a custom allocator or a
// debug-enabled version, without modifying all call sites.
#ifndef PEP_MALLOC
	#define PEP_MALLOC( size ) malloc( size )
	#define PEP_REALLOC( ptr, size ) realloc( ptr, size )
	#define PEP_FREE( ptr ) free( ptr )
#endif

// Provides a cross-platform macro to count leading zeros in a 32-bit integer.
#ifndef PEP_COUNT_LEADING_ZEROS
	#ifdef _MSC_VER
		// Microsoft Visual C++ compiler.
		#define PEP_COUNT_LEADING_ZEROS( x ) __lzcnt( x )
	#else
		// GCC/Clang compilers.
		#define PEP_COUNT_LEADING_ZEROS( x ) __builtin_clz( x )
	#endif
#endif

// How many bits do we need to fit N values?
#define PEP_BITS_TO_FIT( N )( ( ( N ) <= 1 ) ? 1 : ( 32 - PEP_COUNT_LEADING_ZEROS( ( N ) - 1 ) ) )

/////// /////// /////// /////// /////// /////// ///////

static inline _pep_prob _pep_get_prob_from_ctx( const _pep_context* const ctx, const uint32_t symbol );
static inline void _pep_arith_encode( _pep_ac_encode* const ac, const _pep_prob prob );
static inline void _pep_arith_encode_normalize( _pep_ac_encode* const ac );
static inline uint32_t _pep_arith_decode_curr_freq( _pep_ac_decode* const ac, const uint32_t scale );
static inline void _pep_arith_decode_update( _pep_ac_decode* const ac, const _pep_prob prob );
static inline _pep_sym_decode _pep_get_sym_from_freq( const _pep_context* const ctx, const uint32_t target_freq, const uint32_t max_symbol );

static inline uint32_t _pep_reformat( const uint32_t in_color, const pep_format in_format, const pep_format out_format );
static inline pep pep_compress( const uint32_t* in_pixels, const uint16_t width, const uint16_t height, const pep_format in_format, const pep_format out_format );
static inline uint32_t* pep_decompress( const pep* const in_pep, const pep_format out_format, const uint8_t first_color_transparent );
static inline void pep_free( pep* in_pep );

static inline uint8_t* pep_serialize( const pep* in_pep, uint32_t* const out_size );
static inline pep pep_deserialize( const uint8_t* const in_bytes );

static inline uint8_t pep_save( const pep* const in_pep, const char* const file_path );
static inline pep pep_load( const char* const file_path );

#endif // _PEP_H_

/////// /////// /////// /////// /////// /////// ///////

#ifdef PEP_IMPLEMENTATION

#ifdef _MSC_VER
	//	Intrin header is only needed for implementation.
	#include <intrin.h> // __lzcnt

	//	Disable unsafe fopen usage warning on MSVC compiler.
	#pragma warning( push )
	#pragma warning( disable : 4996 )
#endif

// Getting cumulative frequnce of symbol
static inline _pep_prob _pep_get_prob_from_ctx( const _pep_context* const ctx, const uint32_t symbol )
{
	_pep_prob prob = { 0 };
	prob.scale = ctx->sum;

	for( uint32_t i = 0; i < symbol; ++i )
	{
		prob.low += ctx->freq[ i ];
	}

	prob.high = prob.low + ctx->freq[ symbol ];
	return prob;
}

// This encodes a symbol into the arithmetic-coding range. It scales the
// current range based on the symbol's frequency and total frequency count.
static inline void _pep_arith_encode( _pep_ac_encode* const ac, const _pep_prob prob )
{
	ac->range /= prob.scale;
	ac->low += prob.low * ac->range;
	ac->range *= prob.high - prob.low;
}

// Adjusts the arithmetic-coding range by removing a boundary value
// Main goal of this process is to keep our range from getting
// too small.
static inline void _pep_arith_encode_normalize( _pep_ac_encode* const ac )
{
	while( 1 )
	{
		if( ( ac->low ^ ( ac->low + ac->range ) ) >= PEP_CODE_MAX_VALUE )
		{
			if( ac->range < PEP_PROB_MAX_VALUE )
			{
				ac->range = PEP_PROB_MAX_VALUE - (ac->low & (PEP_PROB_MAX_VALUE - 1));
			}
			else break;
		}

		uint8_t byte = ac->low >> 24;
		ac->low <<= 8;
		ac->range <<= 8;
		*ac->data_ref++ = byte;
	}
}

// Getting current frequency by doing reverse trasformation
static inline uint32_t _pep_arith_decode_curr_freq( _pep_ac_decode* const ac, const uint32_t scale )
{
	ac->range /= scale;
	uint32_t result = ( ac->code - ac->low ) / ( ac->range );
	return result;
}

// Same as with the encode_normalize, only on decode we reading in value
static inline void _pep_arith_decode_update( _pep_ac_decode* const ac, const _pep_prob prob )
{
	ac->low += ac->range * prob.low;
	ac->range *= prob.high - prob.low;

	while( 1 )
	{
		if( ( ac->low ^ ( ac->low + ac->range ) ) >= PEP_CODE_MAX_VALUE )
		{
			if( ac->range < PEP_PROB_MAX_VALUE )
			{
				ac->range = PEP_PROB_MAX_VALUE - (ac->low & (PEP_PROB_MAX_VALUE - 1));
			}
			else break;
		}

		uint8_t in_byte = 0;
		if( ac->data_ref != ac->end_of_data )
		{
			in_byte = *ac->data_ref++;
		}

		ac->code = ( ac->code << 8 ) | in_byte;
		ac->range <<= 8;
		ac->low <<= 8;
	}
}

static inline _pep_sym_decode _pep_get_sym_from_freq( const _pep_context* const ctx, const uint32_t target_freq, const uint32_t max_symbol )
{
	_pep_sym_decode result = { };

	uint32_t s = 0;
	uint32_t freq = 0;
	for( ; s < max_symbol; ++s )
	{
		freq += ctx->freq[ s ];
		if( freq > target_freq ) break;
	}

	if( s >= max_symbol )
	{
		s = PEP_FREQ_END;
		freq += ctx->freq[ PEP_FREQ_END ];
	}

	result.prob.high = freq;
	result.prob.low = freq - ctx->freq[ s ];
	result.prob.scale = ctx->sum;
	result.symbol = s;

	return result;
}

// PEP supports "dynamic formats", where you can specify what the in-bytes are,
// and reformat to a different channel-order.
// This means two "identical" PEP files can have different formats, but you
// can choose how to reformat it when it decompresses!
static inline uint32_t _pep_reformat( const uint32_t in_color, const pep_format in_format, const pep_format out_format )
{
	if( in_format == out_format ) return in_color;

	if( in_format <= pep_bgra && out_format <= pep_bgra )
	{
		return( in_color & 0x00ff00ff ) | ( ( in_color & 0xff000000 ) >> 16 ) | ( ( in_color & 0x0000ff00 ) << 16 );
	}
	else if( in_format >= pep_abgr && out_format >= pep_abgr )
	{
		return( in_color & 0xff00ff00 ) | ( ( in_color & 0x00ff0000 ) >> 16 ) | ( ( in_color & 0x000000ff ) << 16 );
	}
	else if( ( in_format ^ out_format ) == 2 )
	{
		return( ( in_color & 0x000000ff ) << 24 ) | ( ( in_color & 0x0000ff00 ) << 8 ) | ( ( in_color & 0x00ff0000 ) >> 8 ) | ( ( in_color & 0xff000000 ) >> 24 );
	}
	else if( in_format < out_format )
	{
		return( ( in_color & 0x000000ff ) << 24 ) | ( ( in_color & 0xffffff00 ) >> 8 );
	}
	else
	{
		return( ( in_color & 0xff000000 ) >> 24 ) | ( ( in_color & 0x00ffffff ) << 8 );
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

	out_pep.bytes = ( uint8_t* )PEP_MALLOC( pixels_area * sizeof( uint32_t ) * 2 ); // zero chance it will be >2x the size
	out_pep.width = width;
	out_pep.height = height;
	out_pep.format = out_format;
	out_pep.color_bits = _pep_8bit;

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

	uint8_t bits_per_index = PEP_BITS_TO_FIT( out_pep.palette_size );
	if( bits_per_index > 8 ) bits_per_index = 8; // only 8 bits in a byte

	const uint8_t indices_per_byte = 8 / bits_per_index;
	const uint8_t index_mask = ( 1 << bits_per_index ) - 1;

	static _pep_context contexts[ PEP_CONTEXTS_MAX + 1 ];
	memset( contexts, 0, sizeof( _pep_context ) * ( PEP_CONTEXTS_MAX + 1 ) );

	_pep_context* order0 = &contexts[ PEP_CONTEXTS_MAX ];
	for( uint64_t i = 0; i < PEP_FREQ_N; i++ ) order0->freq[ i ] = 1;
	order0->sum = PEP_FREQ_N;

	_pep_ac_encode ac = { 0 };
	ac.range = ( uint32_t )( ( 1llu << 32 ) - 1 );
	ac.data_ref = data_ref;
	uint32_t context_id = 0;

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
				_pep_prob prob = _pep_get_prob_from_ctx( context_ref, symbol );
				_pep_arith_encode( &ac, prob );
				PEP_UPDATE( context_ref, symbol );
			}
			else
			{
				if( context_sum != 0 )
				{
					_pep_prob prob = _pep_get_prob_from_ctx( context_ref, PEP_FREQ_END );
					_pep_arith_encode( &ac, prob );
					_pep_arith_encode_normalize( &ac );
					context_ref->freq[ PEP_FREQ_END ] ++;
					context_ref->sum++;
				}

				_pep_prob prob = _pep_get_prob_from_ctx( order0, symbol );
				_pep_arith_encode( &ac, prob );

				if( context_sum == 0 )
				{
					context_ref->freq[ PEP_FREQ_END ] = 1;
					context_ref->sum = 1;
				}
				context_ref->freq[ symbol ] = 1;
				context_ref->sum++;
				PEP_UPDATE( order0, symbol );
			}

			_pep_arith_encode_normalize( &ac );
			context_id = ( ( context_id << 8 ) | symbol );

			symbol = 0;
			indices_in_byte = 0;
		}

		if( p < p_end )
		{
			++p;
		}
	}

	for( uint8_t i = 0; i < 4; i++ )
	{
		uint8_t byte = ac.low >> 24;
		ac.low <<= 8;
		*ac.data_ref++ = byte;
	}

	out_pep.bytes_size = ac.data_ref - out_pep.bytes;
	out_pep.bytes = ( uint8_t* )PEP_REALLOC( out_pep.bytes, out_pep.bytes_size );

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
	uint32_t* out_pixels = ( uint32_t* )PEP_MALLOC( area * sizeof( uint32_t ) );

	uint64_t canvas_pos = 0;

	uint8_t bits_per_index = PEP_BITS_TO_FIT( in_pep->palette_size );
	if( bits_per_index > 8 ) bits_per_index = 8; // only 8 bits in a byte

	const uint8_t indices_per_byte = 8 / bits_per_index;
	const uint8_t index_mask = ( 1 << bits_per_index ) - 1;

	static _pep_context contexts[ PEP_CONTEXTS_MAX + 1 ];
	memset( contexts, 0, sizeof( _pep_context ) * ( PEP_CONTEXTS_MAX + 1 ) );

	_pep_context* order0 = &contexts[ PEP_CONTEXTS_MAX ];
	for( uint64_t i = 0; i < PEP_FREQ_N; i++ ) order0->freq[ i ] = 1;
	order0->sum = PEP_FREQ_N;

	///////
	// decompress PPM order-2 structure into packed-palette-indices

	uint32_t context_id = 0;
	const uint16_t max_symbols = in_pep->max_symbols + 1;

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

	_pep_ac_decode ac = { 0 };
	ac.range = ( uint32_t )( ( 1llu << 32 ) - 1 );
	ac.data_ref = data_ref;
	ac.end_of_data = data_ref + in_pep->bytes_size;

	for( uint8_t i = 0; i < 4; ++i )
	{
		uint8_t in_byte = 0;
		if( ac.data_ref != ac.end_of_data )
		{
			in_byte = *ac.data_ref++;
		}

		ac.code = ( ac.code << 8 ) | in_byte;
	}

	_pep_sym_decode decode_result;
	for( uint64_t b = 0; b < packed_indices_size; b++ )
	{
		_pep_context* const context_ref = &contexts[ context_id % PEP_CONTEXTS_MAX ];
		const uint32_t context_sum = context_ref->sum;

		uint8_t symbol_found = 0;
		if( context_sum != 0 )
		{
			uint32_t decode_freq = _pep_arith_decode_curr_freq( &ac, context_sum );
			decode_result = _pep_get_sym_from_freq( context_ref, decode_freq, max_symbols );
			_pep_arith_decode_update( &ac, decode_result.prob );

			if( decode_result.symbol != PEP_FREQ_END )
			{
				symbol_found = 1;
				PEP_UPDATE( context_ref, decode_result.symbol );
			}
			else
			{
				context_ref->freq[ PEP_FREQ_END ] ++;
				context_ref->sum++;
			}
		}

		if( !symbol_found )
		{
			uint32_t decode_freq = _pep_arith_decode_curr_freq( &ac, order0->sum );
			decode_result = _pep_get_sym_from_freq( order0, decode_freq, max_symbols );
			_pep_arith_decode_update( &ac, decode_result.prob );

			if( context_sum == 0 )
			{
				context_ref->freq[ PEP_FREQ_END ] = 1;
				context_ref->sum = 1;
			}
			context_ref->freq[ decode_result.symbol ] = 1;
			context_ref->sum++;
			PEP_UPDATE( order0, decode_result.symbol );
		}

		///////
		// convert packed-palette-indices to pixels

		if( indices_per_byte > 1 )
		{
			uint8_t indices_in_byte = 0;
			while( indices_in_byte < indices_per_byte && canvas_pos < area )
			{
				const uint8_t palette_idx = ( decode_result.symbol >> ( indices_in_byte * bits_per_index ) ) & index_mask;
				out_pixels[ canvas_pos ] = _pep_reformat( palette[ palette_idx ], in_pep->format, out_format );
				++canvas_pos;
				++indices_in_byte;
			}
		}
		else
		{
			if( canvas_pos < area )
			{
				out_pixels[ canvas_pos ] = _pep_reformat( palette[ decode_result.symbol ], in_pep->format, out_format );
				++canvas_pos;
			}
		}

		context_id = ( ( context_id << 8 ) | decode_result.symbol );
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
	if( !in_pep || !in_pep->width || !in_pep->height || !in_pep->bytes_size || !in_pep->bytes )
	{
		*out_size = 0;
		return NULL;
	}
	
	uint16_t palette_count = in_pep->palette_size ? in_pep->palette_size : ( in_pep->palette[ 0 ] ? 256 : 0 );
	
	if( !palette_count )
	{
		*out_size = 0;
		return NULL;
	}
	
	uint64_t palette_bytes = 0;
	switch( in_pep->color_bits )
	{
		case _pep_1bit: palette_bytes = ( palette_count + 1 ) >> 1; break;
		case _pep_2bit: palette_bytes = palette_count; break;
		case _pep_4bit: palette_bytes = palette_count << 1; break;
		case _pep_8bit: palette_bytes = palette_count << 2; break;
	}
	
	uint8_t* out_bytes = ( uint8_t* )PEP_MALLOC( 15 + palette_bytes + in_pep->bytes_size );
	uint8_t* bytes_ref = out_bytes;
	
	*bytes_ref++ = ( in_pep->format & 0x07 ) | ( ( in_pep->color_bits & 0x03 ) << 3 );
	
	*bytes_ref++ = in_pep->palette_size;
	
	uint32_t packed_dims = ( ( in_pep->width & 0xFFF ) << 12 ) | ( in_pep->height & 0xFFF );
	*bytes_ref++ = packed_dims >> 16;
	*bytes_ref++ = packed_dims >> 8;
	*bytes_ref++ = packed_dims;
	
	uint32_t size = in_pep->bytes_size;
	while( size >= 0x80 )
	{
		*bytes_ref++ = ( size | 0x80 ) & 0xFF;
		size >>= 7;
	}
	*bytes_ref++ = size;
	
	*bytes_ref++ = in_pep->max_symbols;
	
	switch( in_pep->color_bits )
	{
		case _pep_1bit:
			for( uint16_t i = 0; i < palette_count; i += 2 )
			{
				uint32_t c1 = in_pep->palette[ i ];
				uint32_t c2 = ( i + 1 < palette_count ) ? in_pep->palette[ i + 1 ] : 0;
				*bytes_ref++ = ( ( c1 >> 24 ) & 0x80 ) | ( ( c1 >> 17 ) & 0x40 ) | 
				               ( ( c1 >> 10 ) & 0x20 ) | ( ( c1 >> 3 ) & 0x10 ) |
				               ( ( c2 >> 28 ) & 0x08 ) | ( ( c2 >> 21 ) & 0x04 ) | 
				               ( ( c2 >> 14 ) & 0x02 ) | ( ( c2 >> 7 ) & 0x01 );
			}
			break;

		case _pep_2bit:
			for( uint16_t i = 0; i < palette_count; i++ )
			{
				uint32_t c = in_pep->palette[ i ];
				*bytes_ref++ = ( ( c >> 24 ) & 0xC0 ) | ( ( c >> 18 ) & 0x30 ) | 
				               ( ( c >> 12 ) & 0x0C ) | ( ( c >> 6 ) & 0x03 );
			}
			break;

		case _pep_4bit:
			for( uint16_t i = 0; i < palette_count; i++ )
			{
				uint32_t c = in_pep->palette[ i ];
				*bytes_ref++ = ( ( c >> 16 ) & 0xF0 ) | ( ( c >> 28 ) & 0x0F );
				*bytes_ref++ = ( c & 0xF0 ) | ( ( c >> 12 ) & 0x0F );
			}
			break;

		case _pep_8bit:
			memcpy( bytes_ref, in_pep->palette, palette_count << 2 );
			bytes_ref += palette_count << 2;
			break;
	}
	
	memcpy( bytes_ref, in_pep->bytes, in_pep->bytes_size );
	
	*out_size = bytes_ref - out_bytes + in_pep->bytes_size;
	return out_bytes;
}

static inline pep pep_deserialize( const uint8_t* const in_bytes )
{
	pep out_pep = { 0 };
	
	if( !in_bytes )
		return out_pep;
	
	const uint8_t* bytes_ref = in_bytes;
	
	uint8_t packed_flags = *bytes_ref++;
	out_pep.format = ( pep_format )( packed_flags & 0x07 );
	out_pep.color_bits = ( _pep_color_bits )( ( packed_flags >> 3 ) & 0x03 );
	
	out_pep.palette_size = *bytes_ref++;
	
	uint32_t packed_dims = ( *bytes_ref++ << 16 ) | ( *bytes_ref++ << 8 ) | *bytes_ref++;
	out_pep.width = packed_dims >> 12;
	out_pep.height = packed_dims & 0xFFF;
	
	if( !out_pep.width || !out_pep.height )
		return out_pep;
	
	uint8_t shift = 0;
	do
	{
		uint8_t byte = *bytes_ref++;
		out_pep.bytes_size |= ( uint32_t )( byte & 0x7F ) << shift;
		shift += 7;
		if( !( byte & 0x80 ) ) break;
	} while( shift < 32 );
	
	if( !out_pep.bytes_size )
		return out_pep;
	
	out_pep.max_symbols = *bytes_ref++;
	
	memset( out_pep.palette, 0, sizeof( uint32_t ) * 256 );
	
	switch( out_pep.color_bits )
	{
		case _pep_1bit:
			for( uint16_t i = 0; i < out_pep.palette_size; i += 2 )
			{
				uint8_t b = *bytes_ref++;
				out_pep.palette[ i ] = ( ( b & 0x80 ) ? 0xFF000000 : 0 ) | 
				                       ( ( b & 0x40 ) ? 0x00FF0000 : 0 ) |
				                       ( ( b & 0x20 ) ? 0x0000FF00 : 0 ) | 
				                       ( ( b & 0x10 ) ? 0x000000FF : 0 );
				if( i + 1 < out_pep.palette_size )
					out_pep.palette[ i + 1 ] = ( ( b & 0x08 ) ? 0xFF000000 : 0 ) | 
					                           ( ( b & 0x04 ) ? 0x00FF0000 : 0 ) |
					                           ( ( b & 0x02 ) ? 0x0000FF00 : 0 ) | 
					                           ( ( b & 0x01 ) ? 0x000000FF : 0 );
			}
			break;

		case _pep_2bit:
			for( uint16_t i = 0; i < out_pep.palette_size; i++ )
			{
				uint8_t b = *bytes_ref++;
				out_pep.palette[ i ] = ( ( uint32_t )( ( b >> 6 ) * 0x55 ) << 24 ) | 
				                       ( ( uint32_t )( ( ( b >> 4 ) & 0x03 ) * 0x55 ) << 16 ) |
				                       ( ( uint32_t )( ( ( b >> 2 ) & 0x03 ) * 0x55 ) << 8 ) | 
				                       ( ( ( b & 0x03 ) * 0x55 ) );
			}
			break;

		case _pep_4bit:
			for( uint16_t i = 0; i < out_pep.palette_size; i++ )
			{
				uint8_t b1 = *bytes_ref++;
				uint8_t b2 = *bytes_ref++;
				out_pep.palette[ i ] = ( ( uint32_t )( ( b1 & 0x0F ) | ( ( b1 & 0x0F ) << 4 ) ) << 24 ) |
				                       ( ( uint32_t )( ( b1 & 0xF0 ) | ( ( b1 & 0xF0 ) >> 4 ) ) << 16 ) |
				                       ( ( uint32_t )( ( b2 & 0x0F ) | ( ( b2 & 0x0F ) << 4 ) ) << 8 ) |
				                       ( ( b2 & 0xF0 ) | ( ( b2 & 0xF0 ) >> 4 ) );
			}
			break;

		case _pep_8bit:
			memcpy( out_pep.palette, bytes_ref, out_pep.palette_size << 2 );
			bytes_ref += out_pep.palette_size << 2;
			break;
	}
	
	out_pep.bytes = ( uint8_t* )PEP_MALLOC( out_pep.bytes_size );
	memcpy( out_pep.bytes, bytes_ref, out_pep.bytes_size );
	
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

	FILE * file = fopen( file_path, "wb" );
	if( !file )
	{
		PEP_FREE( bytes );
		return 0;
	}

	size_t written = fwrite( bytes, 1, bytes_size, file );

	fclose( file );
	PEP_FREE( bytes );

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

	FILE * file = fopen( file_path, "rb" );
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

	uint8_t* bytes = ( uint8_t* )PEP_MALLOC( file_size );

	size_t read = fread( bytes, 1, file_size, file );
	fclose( file );

	if( read != ( size_t ) file_size )
	{
		PEP_FREE( bytes );
		return out_pep;
	}

	out_pep = pep_deserialize( bytes );
	PEP_FREE( bytes );

	return out_pep;
}

#ifdef _MSC_VER
	#pragma warning( pop )
#endif

#endif

/////// /////// /////// /////// /////// /////// ///////
