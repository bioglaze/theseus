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

[numthreads( 8, 8, 1 )]
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

}
