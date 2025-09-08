#include "ubo.h"

#define TILE_RES 16
#define NUM_THREADS_PER_TILE (TILE_RES * TILE_RES)
#define MAX_NUM_LIGHTS_PER_TILE 544
#define LIGHT_INDEX_BUFFER_SENTINEL 0x7fffffff
#define USE_MINMAX_Z 0

groupshared uint ldsLightIdxCounter;
groupshared uint ldsLightIdx[ MAX_NUM_LIGHTS_PER_TILE ];
groupshared uint ldsZMax;
groupshared uint ldsZMin;

uint GetNumTilesX()
{
    return (uint) ((uniforms.tilesXY.x + TILE_RES - 1) / (float) TILE_RES);
}

uint GetNumTilesY()
{
    return (uint) ((uniforms.tilesXY.y + TILE_RES - 1) / (float) TILE_RES);
}

// convert a point from post-projection space into view space
float4 ConvertProjToView( float4 p )
{
    p = mul( uniforms.clipToView, p );
    p /= p.w;
    p.y = -p.y;
    
    return p;
}

// this creates the standard Hessian-normal-form plane equation from three points, 
// except it is simplified for the case where the first point is the origin
float4 CreatePlaneEquation( float3 b, float3 c )
{
    float3 n;

    // normalize(cross( b.xyz-a.xyz, c.xyz-a.xyz )), except we know "a" is the origin
    n.xyz = normalize( cross( b.xyz, c.xyz ) );

    // -(n dot a), except we know "a" is the origin
    //n.w = 0;

    return float4( n, 0 );
}

// point-plane distance, simplified for the case where 
// the plane passes through the origin
float GetSignedDistanceFromPlane( float3 p, float3 eqn )
{
    // dot( eqn.xyz, p.xyz ) + eqn.w, , except we know eqn.w is zero 
    // (see CreatePlaneEquation above)
    return dot( eqn, p );
}

void CalculateMinMaxDepthInLds( uint3 globalThreadIdx, uint depthBufferSampleIdx )
{
                            // FIXME: texture index
    float viewPosZ = texture2ds[ 0 ].Load( uint3( globalThreadIdx.x, globalThreadIdx.y, 0 ) ).x;
    
    uint z = asuint( viewPosZ );

    if (viewPosZ != 0.f)
    {
        uint r;
        InterlockedMax( ldsZMax, z, r );
        InterlockedMin( ldsZMin, z, r );
    }
}

[numthreads( TILE_RES, TILE_RES, 1 )]
void cullLights( uint3 globalIdx : SV_DispatchThreadID, uint3 localIdx : SV_GroupThreadID, uint3 groupIdx : SV_GroupID )
{
    uint localIdxFlattened = localIdx.x + localIdx.y * TILE_RES;
    uint tileIdxFlattened = groupIdx.x + groupIdx.y * GetNumTilesX();

    if (localIdxFlattened == 0)
    {
        ldsZMin = 0x7f7fffff; // FLT_MAX as uint
        ldsZMax = 0;
        ldsLightIdxCounter = 0;
    }

    float3 frustumEqn[ 4 ];
    {  
        // construct frustum for this tile
        uint pxm = TILE_RES * groupIdx.x;
        uint pym = TILE_RES * groupIdx.y;
        uint pxp = TILE_RES * (groupIdx.x + 1);
        uint pyp = TILE_RES * (groupIdx.y + 1);

        uint uWindowWidthEvenlyDivisibleByTileRes = TILE_RES * GetNumTilesX();
        uint uWindowHeightEvenlyDivisibleByTileRes = TILE_RES * GetNumTilesY();

        // four corners of the tile, clockwise from top-left
        float4 frustum[ 4 ];
        frustum[ 0 ] = ConvertProjToView( float4( pxm / (float) uWindowWidthEvenlyDivisibleByTileRes * 2.0f - 1.0f, (uWindowHeightEvenlyDivisibleByTileRes - pym) / (float) uWindowHeightEvenlyDivisibleByTileRes * 2.0f - 1.0f, 1.0f, 1.0f ) );
        frustum[ 1 ] = ConvertProjToView( float4( pxp / (float) uWindowWidthEvenlyDivisibleByTileRes * 2.0f - 1.0f, (uWindowHeightEvenlyDivisibleByTileRes - pym) / (float) uWindowHeightEvenlyDivisibleByTileRes * 2.0f - 1.0f, 1.0f, 1.0f ) );
        frustum[ 2 ] = ConvertProjToView( float4( pxp / (float) uWindowWidthEvenlyDivisibleByTileRes * 2.0f - 1.0f, (uWindowHeightEvenlyDivisibleByTileRes - pyp) / (float) uWindowHeightEvenlyDivisibleByTileRes * 2.0f - 1.0f, 1.0f, 1.0f ) );
        frustum[ 3 ] = ConvertProjToView( float4( pxm / (float) uWindowWidthEvenlyDivisibleByTileRes * 2.0f - 1.0f, (uWindowHeightEvenlyDivisibleByTileRes - pyp) / (float) uWindowHeightEvenlyDivisibleByTileRes * 2.0f - 1.0f, 1.0f, 1.0f ) );

        // create plane equations for the four sides of the frustum, 
        // with the positive half-space outside the frustum (and remember, 
        // view space is left handed, so use the left-hand rule to determine 
        // cross product direction)
        for (uint i = 0; i < 4; ++i)
        {
            frustumEqn[ i ] = CreatePlaneEquation( frustum[ i ].xyz, frustum[ (i + 1) & 3 ].xyz ).xyz;
        }
    }

    // Blocks execution of all threads in a group until all group shared accesses 
    // have been completed and all threads in the group have reached this call.
    GroupMemoryBarrierWithGroupSync();
    
#if USE_MINMAX_Z
    // calculate the min and max depth for this tile, 
    // to form the front and back of the frustum

    float minZ = FLT_MAX;
    float maxZ = 0.0f;

    CalculateMinMaxDepthInLds( globalIdx, 0 );

    GroupMemoryBarrierWithGroupSync();

    // FIXME: swapped min and max to prevent false culling near depth discontinuities.
    //        AMD's ForwarPlus11 sample uses reverse depth test so maybe that's why this swap is needed. [2014-07-07]
    minZ = asfloat( ldsZMax );
    maxZ = asfloat( ldsZMin );
#endif

    // loop over the lights and do a sphere vs. frustum intersection test

    for (uint i = 0; i < uniforms.pointLightCount; i += NUM_THREADS_PER_TILE)
    {
        uint il = localIdxFlattened + i;
        if (il < uniforms.pointLightCount)
        {
            float4 cen = vk::RawBufferLoad< float4 >( pushConstants.pointLightCenterAndRadiusBuf + 16 * il );
            float4 center = cen;
            float radius = center.w;
            center.xyz = mul( uniforms.localToView, float4( center.xyz, 1 ) ).xyz;

            // test if sphere is intersecting or inside frustum
#if USE_MINMAX_Z
            if (-center.z + minZ < radius && center.z - maxZ < radius)
#else
            if (center.z < radius)
#endif
            {
                if ((GetSignedDistanceFromPlane( center.xyz, frustumEqn[ 0 ] ) < radius) &&
                    (GetSignedDistanceFromPlane( center.xyz, frustumEqn[ 1 ] ) < radius) &&
                    (GetSignedDistanceFromPlane( center.xyz, frustumEqn[ 2 ] ) < radius) &&
                    (GetSignedDistanceFromPlane( center.xyz, frustumEqn[ 3 ] ) < radius))
                {
                    // do a thread-safe increment of the list counter 
                    // and put the index of this light into the list
                    uint dstIdx = 0;
                    InterlockedAdd( ldsLightIdxCounter, 1, dstIdx );
                    ldsLightIdx[ dstIdx ] = il;
                }
            }
        }
    }
    
    GroupMemoryBarrierWithGroupSync();

    // Spot lights.
    uint pointLightsInThisTile = ldsLightIdxCounter;

    /*for (uint j = 0; j < uniforms.spotLightCount; j += NUM_THREADS_PER_TILE)
    {
        uint jl = localIdxFlattened + j;

        if (jl < uniforms.spotLightCount)
        {
            // FIXME: replace pointLight with spotLight
            float4 cen = vk::RawBufferLoad < float4 > (pushConstants.pointLightCenterAndRadiusBuf + 16 * jl);
            float4 center = cen;

            float radius = center.w;
            center.xyz = mul( uniforms.localToView, float4( center.xyz, 1 ) ).xyz;

            // test if sphere is intersecting or inside frustum
#if USE_MINMAX_Z
            if (-center.z - minZ < radius && center.z - maxZ < radius)
#else
            // Nothing to do here
#endif
            {
                if ((GetSignedDistanceFromPlane( center.xyz, frustumEqn[ 0 ] ) < radius) &&
                    (GetSignedDistanceFromPlane( center.xyz, frustumEqn[ 1 ] ) < radius) &&
                    (GetSignedDistanceFromPlane( center.xyz, frustumEqn[ 2 ] ) < radius) &&
                    (GetSignedDistanceFromPlane( center.xyz, frustumEqn[ 3 ] ) < radius))
                {
                    // do a thread-safe increment of the list counter 
                    // and put the index of this light into the list
                    uint dstIdx = 0;
                    InterlockedAdd( ldsLightIdxCounter, 1, dstIdx );
                    ldsLightIdx[ dstIdx ] = jl;
                }
            }
        }
    }*/

    GroupMemoryBarrierWithGroupSync();

    // write back
    uint startOffset = uniforms.maxLightsPerTile * tileIdxFlattened;

    for (uint i = localIdxFlattened; i < pointLightsInThisTile; i += NUM_THREADS_PER_TILE)
    {
        //perTileLightIndexBuffer[ startOffset + i ] = ldsLightIdx[ i ];
        vk::RawBufferStore< uint > (pushConstants.lightIndexBuf + 4 * (startOffset + i), ldsLightIdx[ i ] );
    }

    for (uint j = (localIdxFlattened + pointLightsInThisTile); j < ldsLightIdxCounter; j += NUM_THREADS_PER_TILE)
    {
        //perTileLightIndexBuffer[ startOffset + j + 1 ] = ldsLightIdx[ j ];
        vk::RawBufferStore< uint > (pushConstants.lightIndexBuf + 4 * (startOffset + j + 1), ldsLightIdx[ j ] );
    }

    if (localIdxFlattened == 0)
    {
        // mark the end of each per-tile list with a sentinel (point lights)
        //perTileLightIndexBuffer[ startOffset + pointLightsInThisTile ] = LIGHT_INDEX_BUFFER_SENTINEL;
        vk::RawBufferStore< uint > (pushConstants.lightIndexBuf + 4 * pointLightsInThisTile, LIGHT_INDEX_BUFFER_SENTINEL);
        // mark the end of each per-tile list with a sentinel (spot lights)
        //perTileLightIndexBuffer[ startOffset + ldsLightIdxCounter + 1 ] = LIGHT_INDEX_BUFFER_SENTINEL;
        vk::RawBufferStore< uint > (pushConstants.lightIndexBuf + 4 * (ldsLightIdxCounter + 1), LIGHT_INDEX_BUFFER_SENTINEL);
    }
}
