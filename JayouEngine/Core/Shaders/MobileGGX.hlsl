/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define MOBILE_GGX_USE_FP16 1
#define MEDIUMP_FLT_MAX		65504.0
#define MEDIUMP_FLT_MIN		0.00006103515625

#if MOBILE_GGX_USE_FP16
#define saturateMediump(x) min(x, MEDIUMP_FLT_MAX)
#else
#define saturateMediump(x) (x)
#endif

// Taken from https://gist.github.com/romainguy/a2e9208f14cae37c579448be99f78f25
// Modified by Epic Games, Inc. To account for premultiplied light color and code style rules.

half GGX_Mobile(half Roughness, half NoH, half3 H, half3 N)
{
    // Walter et al. 2007, "Microfacet Models for Refraction through Rough Surfaces"

    // In mediump, there are two problems computing 1.0 - NoH^2
    // 1) 1.0 - NoH^2 suffers floating point cancellation when NoH^2 is close to 1 (highlights)
    // 2) NoH doesn't have enough precision around 1.0
    // Both problem can be fixed by computing 1-NoH^2 in highp and providing NoH in highp as well

    // However, we can do better using Lagrange's identity:
    //      ||a x b||^2 = ||a||^2 ||b||^2 - (a . b)^2
    // since N and H are unit vectors: ||N x H||^2 = 1.0 - NoH^2
    // This computes 1.0 - NoH^2 directly (which is close to zero in the highlights and has
    // enough precision).
    // Overall this yields better performance, keeping all computations in mediump

#if MOBILE_GGX_USE_FP16
    half3 NxH = cross(N, H);
    half OneMinusNoHSqr = dot(NxH, NxH);
#else
    half OneMinusNoHSqr = 1.0 - NoH * NoH;
#endif

    half a = Roughness * Roughness;
    half n = NoH * a;
    half p = a / (OneMinusNoHSqr + n * n);
    half d = p * p;
    return saturateMediump(d);
}