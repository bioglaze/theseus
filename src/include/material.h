#pragma once

enum class teBlendMode { Off, Alpha, Additive };
enum class teCullMode { Off, CCW };

struct teMaterial
{
    unsigned index = 0;
    teBlendMode blendMode = teBlendMode::Off;
    teCullMode cullMode = teCullMode::CCW;
};

teMaterial teCreateMaterial( const struct teShader& shader );
void teMaterialSetTexture2D( const teMaterial& material, const struct teTexture2D& tex, unsigned slot );
teShader& teMaterialGetShader( const teMaterial& material );
int teMaterialGetTexture2D( const teMaterial& material, unsigned slot );
