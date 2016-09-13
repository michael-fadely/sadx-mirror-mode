#include "stdafx.h"

#define EXPORT __declspec(dllexport)

// basically NJS_MATRIX
DataArray(float, BaseTransformationMatrix, 0x0389D318, 16);

DataPointer(float, ViewPortWidth_Half, 0x03D0FA0C);
DataPointer(float, ViewPortHeight_Half, 0x03D0FA10);
DataPointer(Bool, TransformAndViewportInvalid, 0x03D0FD1C);

#pragma pack(push, 1)
struct __declspec(align(2)) PolyBuff_RenderArgs
{
	Uint32 StartVertex;
	Uint32 PrimitiveCount;
	Uint32 CullMode;
	Uint32 d;
};
static_assert(sizeof(PolyBuff_RenderArgs) == 0x10, "PolyBuff_RenderArgs size is incorrect");

struct PolyBuff
{
	void *pStreamData; // IDirect3DVertexBuffer8
	Uint32 TotalSize;
	Uint32 CurrentSize;
	Uint32 Stride;
	Uint32 FVF;
	PolyBuff_RenderArgs *RenderArgs;
	Uint32 LockCount;
	const char *name;
	int i;
};
static_assert(sizeof(PolyBuff) == 0x24, "PolyBuff size is incorrect");
#pragma pack(pop)

enum MirrorDirection : Uint8
{
	None,
	MirrorX  = 1 << 0,
	MirrorY  = 1 << 1,
	MirrorXY = MirrorX | MirrorY
};

static Uint8 mirror = MirrorX;

constexpr bool is_mirrored(Uint8 direction)
{
	return direction != None && direction != MirrorXY;
}

static void toggle_mirror(Uint8 direction)
{
	if (direction & MirrorX)
	{
		// Chao triangle strip patches go here
		mirror ^= MirrorX;
		BaseTransformationMatrix[M00] *= -1.0f;
		TransformAndViewportInvalid = true;
	}

	if (direction & MirrorY)
	{
		// Chao triangle strip patches go here
		mirror ^= MirrorY;
		BaseTransformationMatrix[M11] *= -1.0f;
		TransformAndViewportInvalid = true;
	}
}

void __fastcall PolyBuff_DrawTriangleStrip_r(PolyBuff *_this);
static Trampoline PolyBuff_DrawTriangleStrip_trampoline(0x00794760, 0x00794767, PolyBuff_DrawTriangleStrip_r);
void __fastcall PolyBuff_DrawTriangleStrip_r(PolyBuff *_this)
{
	FastcallFunctionPointer(void, original, (PolyBuff*), PolyBuff_DrawTriangleStrip_trampoline.Target());
	if (!mirror || mirror == MirrorXY || _this->RenderArgs->CullMode == D3DCULL_NONE)
	{
		original(_this);
		return;
	}

	auto args = _this->RenderArgs;
	auto last = args->CullMode;

	if (args->CullMode == D3DCULL_CW)
	{
		args->CullMode = D3DCULL_CCW;
	}
	else if (args->CullMode == D3DCULL_CCW)
	{
		args->CullMode = D3DCULL_CW;
	}

	original(_this);
	args->CullMode = last;
}

void __fastcall PolyBuff_DrawTriangleList_r(PolyBuff *_this);
static Trampoline PolyBuff_DrawTriangleList_trampoline(0x007947B0, 0x007947B7, PolyBuff_DrawTriangleList_r);
void __fastcall PolyBuff_DrawTriangleList_r(PolyBuff *_this)
{
	FastcallFunctionPointer(void, original, (PolyBuff*), PolyBuff_DrawTriangleList_trampoline.Target());
	if (!mirror || mirror == MirrorXY || _this->RenderArgs->CullMode == D3DCULL_NONE)
	{
		original(_this);
		return;
	}

	auto args = _this->RenderArgs;
	auto last = args->CullMode;

	if (args->CullMode == D3DCULL_CW)
	{
		args->CullMode = D3DCULL_CCW;
	}
	else if (args->CullMode == D3DCULL_CCW)
	{
		args->CullMode = D3DCULL_CW;
	}

	original(_this);
	args->CullMode = last;
}

static void __cdecl njDrawSprite3D_3_r(NJS_SPRITE *a1, int n, NJD_SPRITE attr, float a7);
static Trampoline njDrawSprite3D_3_trampoline(0x0077E390, 0x0077E398, njDrawSprite3D_3_r);
static void __cdecl njDrawSprite3D_3_r(NJS_SPRITE *a1, int n, NJD_SPRITE attr, float a7)
{
	FunctionPointer(void, original, (NJS_SPRITE *a1, int n, NJD_SPRITE attr, float a7),
		njDrawSprite3D_3_trampoline.Target());

	/*if (mirror & MirrorX)
	{
		attr = (attr & NJD_SPRITE_HFLIP) ? attr & ~NJD_SPRITE_HFLIP : attr | NJD_SPRITE_HFLIP;
	}
	if (mirror & MirrorY)
	{
		attr = (attr & NJD_SPRITE_VFLIP) ? attr & ~NJD_SPRITE_VFLIP : attr | NJD_SPRITE_VFLIP;
	}*/

	original(a1, n, attr, a7);
}

static void __stdcall sprite_flip_c(NJS_VECTOR* v)
{
	auto v8 = _nj_screen_.dist / v->z;

	v->x = v->x * v8 + ViewPortWidth_Half;
	v->y = v->y * v8 + ViewPortHeight_Half;

	if (mirror & MirrorX)
	{
		v->x = (float)HorizontalResolution - v->x;
	}

	if (mirror & MirrorY)
	{
		v->y = (float)VerticalResolution - v->y;
	}
}

constexpr auto sprite_flip_retn = (void*)0x0077E46E;
static void __declspec(naked) sprite_flip()
{
	__asm
	{
		lea     ecx, [esp + 30h]
		push    ecx
		call    sprite_flip_c

		mov     ecx, dword ptr [_nj_screen_.dist]
		fld     dword ptr [ecx]
		fdiv    dword ptr [esp + 38h]

		jmp     sprite_flip_retn
	}
}

static bool chao_fix = false;
static Uint8 last_mirror = 0;

static bool do_chao_fix()
{
	if (!chao_fix)
	{
		if (!is_mirrored(mirror) || !IsChaoStage)
			return false;

		auto current = GetCurrentChaoStage();
		chao_fix = current == SADXChaoStage_RaceEntry || current == SADXChaoStage_BlackMarket || current == SADXChaoStage_Race;

		if (chao_fix)
		{
			last_mirror = mirror;
			toggle_mirror(mirror);
		}
	}
	else
	{
		auto current = GetCurrentChaoStage();

		if (!IsChaoStage || current != SADXChaoStage_RaceEntry && current != SADXChaoStage_BlackMarket && current != SADXChaoStage_Race)
		{
			toggle_mirror(last_mirror);
			chao_fix = false;
		}
	}

	return chao_fix;
}

extern "C"
{
	EXPORT ModInfo SADXModInfo = { ModLoaderVer };
	EXPORT void __cdecl Init()
	{
		WriteJump((void*)0x0077E444, sprite_flip);
		BaseTransformationMatrix[M00] *= -1.0f;
	}

	EXPORT void __cdecl OnInput()
	{
		if (do_chao_fix())
			return;

		for (Uint32 i = 0; i < 8; i++)
		{
			ControllerData* pad = ControllerPointers[i];
			if (pad == nullptr)
				continue;

#ifdef _DEBUG
			auto pressed = pad->PressedButtons;
			if (pressed & Buttons_D)
			{
				toggle_mirror(MirrorX);
			}
			if (pressed & Buttons_C)
			{
				toggle_mirror(MirrorY);
			}
#endif

			// We want to skip axis flipping if we're on a menu or the game is paused.
			if (!(mirror & MirrorX) || GameState == 21 || GameState == 16)
				continue;

			pad->LeftStickX = -pad->LeftStickX;
			pad->RightStickX = -pad->RightStickX;

			auto lt = pad->LTriggerPressure;
			auto rt = pad->RTriggerPressure;

			// TODO: handle digital L/R triggers

			pad->LTriggerPressure = rt;
			pad->RTriggerPressure = lt;
		}
	}
}
