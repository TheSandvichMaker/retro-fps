#pragma once

// MIT License
//
// Copyright (c) 2024 Missing Deadlines (Benjamin Wrensch)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// All values used to derive this implementation are sourced from Troy’s initial AgX implementation/OCIO config file available here:
//   https://github.com/sobotka/AgX

// Ported to HLSL by Daniël Cornelisse (2024)

// 0: Default, 1: Golden, 2: Punchy
#define AGX_LOOK 0

// Mean error^2: 3.6705141e-06
float3 agxDefaultContrastApprox(float3 x) 
{
	float3 x2 = x * x;
	float3 x4 = x2 * x2;

	return + 15.5  * x4 * x2
		- 40.14    * x4 * x
		+ 31.96    * x4
		- 6.868    * x2 * x
		+ 0.4298   * x2
		+ 0.1191   * x
		- 0.00232;
}

float3 agx(float3 val) 
{
	const float3x3 agx_mat = {
		0.842479062253094,  0.0423282422610123, 0.0423756549057051,
		0.0784335999999992, 0.878468636469772,  0.0784336,
		0.0792237451477643, 0.0791661274605434, 0.879142973793104,
	};

	const float min_ev = -12.47393f;
	const float max_ev = 4.026069f;

	// Input transform (inset)
	val = mul(agx_mat, val);

	// Log2 space encoding
	val = clamp(log2(val), min_ev, max_ev);
	val = (val - min_ev) / (max_ev - min_ev);

	// Apply sigmoid function approximation
	val = agxDefaultContrastApprox(val);

	return val;
}

float3 agxEotf(float3 val) 
{
	const float3x3 agx_mat_inv = {
		 1.19687900512017,   -0.0528968517574562, -0.0529716355144438,
		-0.0980208811401368,  1.15190312990417,   -0.0980434501171241,
		-0.0990297440797205, -0.0989611768448433,  1.15107367264116,
	};

	// Inverse input transform (outset)
	val = mul(agx_mat_inv, val);

	// sRGB IEC 61966-2-1 2.2 Exponent Reference EOTF Display
	// NOTE: We're linearizing the output here. Comment/adjust when
	// *not* using a sRGB render target
	val = pow(val, 2.2);

	return val;
}

float3 agxLook(float3 val) 
{
	const float3 lw = float3(0.2126, 0.7152, 0.0722);
	float luma = dot(val, lw);

	// Default
	float3 offset = 0.0;
	float3 slope = 1.0;
	float3 power = 1.0;
	float sat = 1.0;

#if AGX_LOOK == 1
	// Golden
	slope = float3(1.0, 0.9, 0.5);
	power = 0.8;
	sat = 0.8;
#elif AGX_LOOK == 2
	// Punchy
	slope = 1.0;
	power = float3(1.35, 1.35, 1.35);
	sat = 1.4;
#endif

	// ASC CDL
	val = pow(val * slope + offset, power);
	return luma + sat * (val - luma);
}
