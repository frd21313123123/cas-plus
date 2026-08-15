// Minimal memory primitives for the payload's /NODEFAULTLIB build.
// MSVC /O2 may lower ordinary aggregate initialization/copy loops to external
// memcpy/memset calls even though cas+ intentionally does not link a CRT.
//
// Do not define the reserved intrinsic names in C++: MSVC 18 rejects that with
// C2169. PotatoPayload.vcxproj aliases unresolved memcpy/memset references to
// these ordinary local symbols at link time instead.

extern "C" void* __cdecl CasPlusMemcpy(void* destination, const void* source,
    SIZE_T count)
{
    if (!destination || !source || count == 0)
        return destination;
    volatile BYTE* out = reinterpret_cast<volatile BYTE*>(destination);
    const volatile BYTE* in = reinterpret_cast<const volatile BYTE*>(source);
    for (SIZE_T i = 0; i < count; ++i)
        out[i] = in[i];
    return destination;
}

extern "C" void* __cdecl CasPlusMemset(void* destination, int value,
    SIZE_T count)
{
    if (!destination || count == 0)
        return destination;
    volatile BYTE* out = reinterpret_cast<volatile BYTE*>(destination);
    const BYTE byteValue = static_cast<BYTE>(value);
    for (SIZE_T i = 0; i < count; ++i)
        out[i] = byteValue;
    return destination;
}
