#pragma once

enum class teBlendMode { Off, Alpha, Additive };
enum class teCullMode { Off, CCW };
enum class teDepthMode { NoneWriteOff, LessOrEqualWriteOn, LessOrEqualWriteOff };
enum class teTopology { Triangles, Lines };
enum class teFillMode { Solid, Wireframe };

struct teMaterial
{
    unsigned index = 0;
    teBlendMode blendMode = teBlendMode::Off;
    teCullMode cullMode = teCullMode::CCW;
    teDepthMode depthMode = teDepthMode::LessOrEqualWriteOn;
    teTopology topology = teTopology::Triangles;
    teFillMode fillMode = teFillMode::Solid;
};

teMaterial teCreateMaterial( const struct teShader& shader );
void teMaterialSetTexture2D( const teMaterial& material, const struct teTexture2D& tex, unsigned slot );
teShader& teMaterialGetShader( const teMaterial& material );
teTexture2D teMaterialGetTexture2D( const teMaterial& material, unsigned slot );
