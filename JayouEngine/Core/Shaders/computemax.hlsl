//
// computemax.hlsl
//

struct Data
{
    float4 Color;
};

cbuffer UsefulData : register(b2)
{
    int gOffscreenWidth;
};

Texture2D gOffscreenOutput : register(t1);
RWStructuredBuffer<Data> gOutput : register(u0);

[numthreads(1, 1, 1)]
void CS(int3 groupID : SV_GroupID)
{
    int2 pos;
    pos.y = groupID.x;
    Data maxPixel;
    maxPixel.Color = 0.0f;
    for (int i = 0; i < gOffscreenWidth; ++i)
    {
        pos.x = i;      
        maxPixel.Color = max(maxPixel.Color, gOffscreenOutput[pos]);

    }
    gOutput[groupID.x] = maxPixel;
}