
/* Copyright (c) 2009-2010, Cedric Stalder <cedric.stalder@gmail.com> 
 *               2009-2010, Stefan Eilemann <eile@equalizergraphics.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 2.1 as published
 * by the Free Software Foundation.
 *  
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "compressor.h"

#include "compressorRLE4B.h"
#include "compressorRLEB.h"
#include "compressorRLE4HF.h"
#include "compressorRLE4BU.h"
#include "compressorRLE565.h"
#include "compressorRLE10A2.h"
#include "compressorYUV.h"
#include "compressorReadDrawPixels.h"
#include "compressorRLEYUV.h"
namespace eq
{
namespace plugin
{
namespace
{
    typedef std::vector< Compressor::Functions > Compressors;
    static Compressors* _functions;

    const Compressor::Functions& _findFunctions( const unsigned name )
    {
        for( Compressors::const_iterator i = _functions->begin();
             i != _functions->end(); ++i )
        {
            const Compressor::Functions& functions = *i;
            EqCompressorInfo info;
            functions.getInfo( &info );
            if( info.name == name )
                return functions;
        }

        assert( 0 ); // UNREACHABLE
        return _functions->front();
    }
}

Compressor::Compressor( const EqCompressorInfo* info )
{}

Compressor::~Compressor()
{
    for ( size_t i = 0; i < _results.size(); i++ )
        delete ( _results[i] );

    _results.clear();
}

Compressor::Functions::Functions( CompressorGetInfo_t getInfo_,
                                  NewCompressor_t newCompressor_,
                                  NewCompressor_t newDecompressor_,
                                  Decompress_t decompress_,
                                  IsCompatible_t isCompatible_ )
        : getInfo( getInfo_ )
        , newCompressor( newCompressor_ )
        , newDecompressor( newDecompressor_ )
        , decompress( decompress_ )
        , isCompatible( isCompatible_ )
{}

void Compressor::registerEngine( const Compressor::Functions& functions )
{
    if( !_functions ) // resolve 'static initialization order fiasco'
        _functions = new Compressors;
    _functions->push_back( functions );
}

}
}

size_t EqCompressorGetNumCompressors()
{
    return eq::plugin::_functions->size();
}
           
void EqCompressorGetInfo( const size_t n, EqCompressorInfo* const info )
{
    assert( eq::plugin::_functions->size() > n );
    (*eq::plugin::_functions)[ n ].getInfo( info );
}

void* EqCompressorNewCompressor( const unsigned name )
{
    const eq::plugin::Compressor::Functions& functions = 
        eq::plugin::_findFunctions( name );
    
    EqCompressorInfo info;
    functions.getInfo( &info );
    return functions.newCompressor( &info );
}

void EqCompressorDeleteCompressor( void* const compressor )
{
    delete reinterpret_cast< eq::plugin::Compressor* >( compressor );
}

void* EqCompressorNewDecompressor( const unsigned name ) 
{
    const eq::plugin::Compressor::Functions& functions = 
        eq::plugin::_findFunctions( name );
    
    EqCompressorInfo info;
    functions.getInfo( &info );
    return functions.newDecompressor( &info );
}

void EqCompressorDeleteDecompressor( void* const decompressor ) 
{
    delete reinterpret_cast< eq::plugin::Compressor* >( decompressor );
}

void EqCompressorCompress( void* const ptr, const unsigned name,
                           void* const in, const eq_uint64_t* inDims,
                           const eq_uint64_t flags )
{
    assert( ptr );
    const bool useAlpha = !(flags & EQ_COMPRESSOR_IGNORE_ALPHA);
    const eq_uint64_t nPixels = (flags & EQ_COMPRESSOR_DATA_1D) ?
                                  inDims[1]: inDims[1] * inDims[3];

    eq::plugin::Compressor* compressor = 
        reinterpret_cast< eq::plugin::Compressor* >( ptr );
    compressor->compress( in, nPixels, useAlpha );
}

unsigned EqCompressorGetNumResults( void* const ptr,
                                    const unsigned name )
{
    assert( ptr );
    eq::plugin::Compressor* compressor = 
        reinterpret_cast< eq::plugin::Compressor* >( ptr );
    return compressor->getNResults();
}

void EqCompressorGetResult( void* const ptr, const unsigned name,
                            const unsigned i, void** const out, 
                            eq_uint64_t* const outSize )
{
    assert( ptr );
    eq::plugin::Compressor* compressor = 
        reinterpret_cast< eq::plugin::Compressor* >( ptr );
    eq::plugin::Compressor::Result* result = compressor->getResults()[ i ];
    
    *out = result->getData();
    *outSize = result->getSize();
    assert( result->getMaxSize() >= result->getSize( ));
}


void EqCompressorDecompress( void* const decompressor, const unsigned name,
                             const void* const* in,
                             const eq_uint64_t* const inSizes,
                             const unsigned nInputs,
                             void* const out, eq_uint64_t* const outDims,
                             const eq_uint64_t flags )
{
    assert( !decompressor );
    const bool useAlpha = !(flags & EQ_COMPRESSOR_IGNORE_ALPHA);
    const eq_uint64_t nPixels = ( flags & EQ_COMPRESSOR_DATA_1D) ?
                           outDims[1] : outDims[1] * outDims[3];

    const eq::plugin::Compressor::Functions& functions = 
        eq::plugin::_findFunctions( name );
    functions.decompress( in, inSizes, nInputs, out, nPixels, useAlpha );
}

bool EqCompressorIsCompatible( const unsigned     name,
                               const GLEWContext* glewContext )
{
    const eq::plugin::Compressor::Functions& functions = 
        eq::plugin::_findFunctions( name );
    
    if ( functions.isCompatible == 0 )
    {
        assert( false );
        return false;
    }

    return functions.isCompatible( glewContext );
}

void EqCompressorDownload( void* const        ptr,
                           const unsigned     name,
                           const GLEWContext* glewContext,
                           const eq_uint64_t  inDims[4],
                           const unsigned     source,
                           const eq_uint64_t  flags,
                           eq_uint64_t        outDims[4],
                           void**             out )
{
    assert( ptr );
    eq::plugin::Compressor* compressor = 
        reinterpret_cast< eq::plugin::Compressor* >( ptr );
    compressor->download( glewContext, inDims, source, flags, outDims, out );
}


void EqCompressorUpload( void* const        ptr,
                         const unsigned     name,
                         const GLEWContext* glewContext, 
                         const void*        buffer,
                         const eq_uint64_t  inDims[4],
                         const eq_uint64_t  flags,
                         const eq_uint64_t  outDims[4],  
                         const unsigned     destination )
{
    assert( ptr );
    eq::plugin::Compressor* compressor = 
        reinterpret_cast< eq::plugin::Compressor* >( ptr );
    compressor->upload( glewContext, buffer, inDims, flags, outDims,
                        destination );
}

