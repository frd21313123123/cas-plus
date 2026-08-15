// Minimal memory primitive for the /NODEFAULTLIB payload.
// This file is compiled with intrinsics and optimization disabled so MSVC does
// not reinterpret the implementation as its own memset intrinsic.

using SIZE_T = unsigned long long;
using BYTE = unsigned char;

extern "C" void* memset(void* destination, int value, SIZE_T count)
{
    volatile BYTE* cursor = reinterpret_cast<volatile BYTE*>(destination);
    const BYTE byteValue = static_cast<BYTE>(value);
    for (SIZE_T i = 0; i < count; ++i)
        cursor[i] = byteValue;
    return destination;
}
