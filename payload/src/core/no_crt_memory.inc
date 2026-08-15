// Minimal memory primitives for the payload's /NODEFAULTLIB build.
// MSVC /O2 may lower otherwise ordinary aggregate initialization/copy loops to
// memcpy/memset calls. Supplying local implementations keeps that optimization
// compatible with the project's no-CRT/manual-map contract.

extern "C" void* __cdecl memcpy(void* destination, const void* source,
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

extern "C" void* __cdecl memset(void* destination, int value, SIZE_T count)
{
    if (!destination || count == 0)
        return destination;
    volatile BYTE* out = reinterpret_cast<volatile BYTE*>(destination);
    const BYTE byteValue = static_cast<BYTE>(value);
    for (SIZE_T i = 0; i < count; ++i)
        out[i] = byteValue;
    return destination;
}
