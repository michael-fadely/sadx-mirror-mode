#include "stdafx.h"

#define EXPORT __declspec(dllexport)

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

// basically NJS_MATRIX
DataArray(float, BaseTransformationMatrix, 0x0389D318, 16);
DataPointer(float, ViewPortWidth_Half, 0x03D0FA0C);
DataPointer(float, ViewPortHeight_Half, 0x03D0FA10);
DataPointer(Bool, TransformAndViewportInvalid, 0x03D0FD1C);
DataPointer(NJS_OBJECT, JumpPanelDigit_OBJECT, 0x008C536C);

static bool AppliedChaoFix  = false;
static Uint8 LastMirrorMode = None;
static Uint8 MirrorMode     = None;

static void __fastcall PolyBuff_DrawTriangleStrip_r(PolyBuff *_this);
static void __fastcall PolyBuff_DrawTriangleList_r(PolyBuff *_this);
static void __fastcall njProjectScreen_r(NJS_MATRIX *m, NJS_VECTOR *p3, NJS_POINT2 *p2);
static char __cdecl SetPauseDisplayOptions_r(Uint8 *a1);
static void __cdecl njDrawSprite3D_3_r(NJS_SPRITE *a1, int n, NJD_SPRITE attr, float a7);

static Trampoline PolyBuff_DrawTriangleStrip_trampoline(0x00794760, 0x00794767, PolyBuff_DrawTriangleStrip_r);
static Trampoline PolyBuff_DrawTriangleList_trampoline(0x007947B0, 0x007947B7, PolyBuff_DrawTriangleList_r);
static Trampoline njProjectScreen_trampoline(0x00788700, 0x00788705, njProjectScreen_r);
static Trampoline SetPauseDisplayOptions_trampoline(0x004582E0, 0x004582E8, SetPauseDisplayOptions_r);
static Trampoline njDrawSprite3D_3_trampoline(0x0077E390, 0x0077E398, njDrawSprite3D_3_r);

inline bool IsMirrored()
{
	return MirrorMode != None && MirrorMode != MirrorXY;
}

// Toggles the provided mirror directions and applies the Chao render fix (more info below).
static void ToggleMirror(Uint8 direction)
{
	if (direction & MirrorX)
	{
		MirrorMode ^= MirrorX;
		BaseTransformationMatrix[M00] *= -1.0f;
		TransformAndViewportInvalid = true;
	}

	if (direction & MirrorY)
	{
		MirrorMode ^= MirrorY;
		BaseTransformationMatrix[M11] *= -1.0f;
		TransformAndViewportInvalid = true;
	}

	if (!TransformAndViewportInvalid)
		return;

	// The model used for the Chao are triangle strips, so if the
	// transformation matrix is only flipped in one direction,
	// the culling modes for the triangle strips must be swapped.
	// It uses separate culling modes depending on which way the faces
	// are pointing (forward or backward).

	bool mirrored = IsMirrored();
	auto clockwise = (int)(mirrored ? D3DCULL_CCW : D3DCULL_CW);
	auto counterClockwise = (int)(mirrored ? D3DCULL_CW : D3DCULL_CCW);

	WriteData((int*)0x00795BD3, counterClockwise);
	WriteData((int*)0x00795BDE, clockwise);

	WriteData((int*)0x00795CD3, counterClockwise);
	WriteData((int*)0x00795CDE, clockwise);

	WriteData((int*)0x00795DF3, counterClockwise);
	WriteData((int*)0x00795DFE, clockwise);

	WriteData((int*)0x00795F28, counterClockwise);
	WriteData((int*)0x00795F33, clockwise);

	WriteData((int*)0x00796068, counterClockwise);
	WriteData((int*)0x00796073, clockwise);

	WriteData((int*)0x007961BB, clockwise);
	WriteData((int*)0x007961CE, counterClockwise);

	WriteData((int*)0x00796328, counterClockwise);
	WriteData((int*)0x00796333, clockwise);

	WriteData((int*)0x00796483, counterClockwise);
	WriteData((int*)0x0079648E, clockwise);
}

static void __fastcall PolyBuff_DrawTriangleStrip_r(PolyBuff *_this)
{
	FastcallFunctionPointer(void, original, (PolyBuff*), PolyBuff_DrawTriangleStrip_trampoline.Target());
	if (!MirrorMode || MirrorMode == MirrorXY || _this->RenderArgs->CullMode == D3DCULL_NONE)
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
static void __fastcall PolyBuff_DrawTriangleList_r(PolyBuff *_this)
{
	FastcallFunctionPointer(void, original, (PolyBuff*), PolyBuff_DrawTriangleList_trampoline.Target());
	if (!MirrorMode || MirrorMode == MirrorXY || _this->RenderArgs->CullMode == D3DCULL_NONE)
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

// Overrides njProjectScreen and flips the screen space coordinates before returning.
static void __fastcall njProjectScreen_r(NJS_MATRIX *m, NJS_VECTOR *p3, NJS_POINT2 *p2)
{
	auto original = (decltype(njProjectScreen_r)*)njProjectScreen_trampoline.Target();
	original(m, p3, p2);

	if (!IsMirrored())
		return;

	if (MirrorMode & MirrorX)
	{
		p2->x = (float)HorizontalResolution - p2->x;
	}

	if (MirrorMode & MirrorY)
	{
		p2->y = (float)VerticalResolution - p2->y;
	}
}

// Removes the map option from the pause menu because
// it's wrong when mirrored and nobody uses it anyway.
static char __cdecl SetPauseDisplayOptions_r(Uint8 *a1)
{
	auto original = (decltype(SetPauseDisplayOptions_r)*)SetPauseDisplayOptions_trampoline.Target();
	auto result = original(a1);

	if (*a1 & PauseOptions_Map)
	{
		*a1 &= ~PauseOptions_Map;
		--result;
	}

	return result;
}

// Used to check if any sprites in 3D space need their sprite flipped (not cooridnates).
static void __cdecl njDrawSprite3D_3_r(NJS_SPRITE *a1, int n, NJD_SPRITE attr, float a7)
{
	FunctionPointer(void, original, (NJS_SPRITE *a1, int n, NJD_SPRITE attr, float a7),
		njDrawSprite3D_3_trampoline.Target());

	// Flips the combo count on Gamma's targets only
	if (a1 && a1->tlist == (NJS_TEXLIST*)0x0091D5E0)
	{
		if (MirrorMode & MirrorX)
		{
			attr = (attr & NJD_SPRITE_HFLIP) ? attr & ~NJD_SPRITE_HFLIP : attr | NJD_SPRITE_HFLIP;
		}

		if (MirrorMode & MirrorY)
		{
			attr = (attr & NJD_SPRITE_VFLIP) ? attr & ~NJD_SPRITE_VFLIP : attr | NJD_SPRITE_VFLIP;
		}
	}

	original(a1, n, attr, a7);
}

// Intercepts the code that sets the screen space coordinates for 3D sprites
// (projected from 3D space to screen space) and flips the coordinates.
static void __stdcall Flip3DSprites_C(NJS_VECTOR* v)
{
	auto v8 = _nj_screen_.dist / v->z;

	v->x = v->x * v8 + ViewPortWidth_Half;
	v->y = v->y * v8 + ViewPortHeight_Half;

	if (MirrorMode & MirrorX)
	{
		v->x = (float)HorizontalResolution - v->x;
	}

	if (MirrorMode & MirrorY)
	{
		v->y = (float)VerticalResolution - v->y;
	}
}
static const auto loc_77E46E = (void*)0x0077E46E;
static void __declspec(naked) Flip3DSprites()
{
	__asm
	{
		lea     ecx, [esp + 30h]
		push    ecx
		call    Flip3DSprites_C

		mov     ecx, dword ptr [_nj_screen_.dist]
		fld     dword ptr [ecx]
		fdiv    dword ptr [esp + 38h]

		jmp     loc_77E46E
	}
}

// Disables mirroring in the Chao Race Entrance, Chao Race, and Black Market. Otherwise, enables mirroring.
static bool CheckMirrorState()
{
	if (!AppliedChaoFix)
	{
		if (!IsMirrored() || !IsChaoStage)
			return false;

		auto current = GetCurrentChaoStage();
		AppliedChaoFix = current == SADXChaoStage_RaceEntry || current == SADXChaoStage_BlackMarket || current == SADXChaoStage_Race;

		if (AppliedChaoFix)
		{
			LastMirrorMode = MirrorMode;
			ToggleMirror(MirrorMode);
		}
	}
	else
	{
		auto current = GetCurrentChaoStage();

		if (!IsChaoStage || current != SADXChaoStage_RaceEntry && current != SADXChaoStage_BlackMarket && current != SADXChaoStage_Race)
		{
			ToggleMirror(LastMirrorMode);
			AppliedChaoFix = false;
		}
	}

	return AppliedChaoFix;
}

static constexpr auto TRIGGER_MASK = Buttons_L | Buttons_R;
inline void SwapTriggers(uint32_t& buttons)
{
	if (!(buttons & TRIGGER_MASK) || (buttons & TRIGGER_MASK) == TRIGGER_MASK)
		return;

	if (buttons & Buttons_L)
	{
		buttons = (buttons & ~Buttons_L) | Buttons_R;
	}
	else if (buttons & Buttons_R)
	{
		buttons = (buttons & ~Buttons_R) | Buttons_L;
	}
}

template <typename T>
static void XOR_SWAP(T& a, T& b)
{
	a ^= b;
	b ^= a;
	a ^= b;
}

static void FlipUVs(NJS_OBJECT* object)
{
	if (object->child)
	{
		FlipUVs(object->child);
	}

	if (object->sibling)
	{
		FlipUVs(object->sibling);
	}

	NJS_MODEL_SADX* model = object->getbasicdxmodel();
	if (model == nullptr)
		throw;

	auto meshsets = model->meshsets;
	if (meshsets == nullptr)
		throw;

	for (size_t i = 0; i < model->nbMeshset; i++)
	{
		const auto& meshset = meshsets[i];
		auto uvs = meshset.vertuv;

		if (uvs == nullptr)
			throw;
		
		size_t uv_count = 0;

		switch (meshset.type_matId & NJD_MESHSET_MASK)
		{
			case NJD_MESHSET_TRIMESH:
			case NJD_MESHSET_N:
			{
				size_t index = 0;
				for (size_t j = 0; j < meshset.nbMesh; j++)
				{
					const auto& n = meshset.meshes[index++];
					uv_count += n;
					index += n;
				}
				break;
			}

			case NJD_MESHSET_3:
				uv_count = meshset.nbMesh * 3;
				break;

			case NJD_MESHSET_4:
				uv_count = meshset.nbMesh * 4;
				break;

			default:
				throw;
		}

		for (size_t j = 0; j < uv_count; j++)
		{
			auto& uv = uvs[j];
			XOR_SWAP(uv.u, uv.v);
		}
	}
}

extern "C"
{
	EXPORT ModInfo SADXModInfo = { ModLoaderVer };

	EXPORT void __cdecl Init()
	{
		WriteJump((void*)0x0077E444, Flip3DSprites);
		ToggleMirror(MirrorX);
		FlipUVs(&JumpPanelDigit_OBJECT);
	}

	EXPORT void __cdecl OnInput()
	{
		if (CheckMirrorState())
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
				ToggleMirror(MirrorX);
			}

			/*
			if (pressed & Buttons_C)
			{
				ToggleMirror(MirrorY);
			}
			*/
#endif

			// We want to skip axis flipping if we're on a menu or the game is paused.
			if (!(MirrorMode & MirrorX) || GameState == 21 || GameState == 16)
				continue;

			// This flips the X axis of both sticks and swaps the triggers.

			pad->LeftStickX = -pad->LeftStickX;
			pad->RightStickX = -pad->RightStickX;

			auto lt = pad->LTriggerPressure;
			auto rt = pad->RTriggerPressure;

			pad->LTriggerPressure = rt;
			pad->RTriggerPressure = lt;

			SwapTriggers(pad->HeldButtons);

			auto not = ~pad->NotHeldButtons;
			SwapTriggers(not);
			pad->NotHeldButtons = ~not;

			SwapTriggers(pad->PressedButtons);
			SwapTriggers(pad->ReleasedButtons);
			SwapTriggers(pad->Old);
		}
	}
}
