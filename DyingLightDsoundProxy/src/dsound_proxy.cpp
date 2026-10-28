#include <Windows.h>
#include <mmsystem.h>
#include <dsound.h>

#include "dsound_proxy_runtime.h"

namespace {

template <typename Function>
Function Resolve(const char* name)
{
    return reinterpret_cast<Function>(ResolveDsoundExport(name));
}

}

extern "C" HRESULT WINAPI DirectSoundCreate(LPCGUID device, LPDIRECTSOUND* sound, LPUNKNOWN outer)
{
    const auto function = Resolve<decltype(&::DirectSoundCreate)>("DirectSoundCreate");
    return function != nullptr ? function(device, sound, outer) : E_FAIL;
}

extern "C" HRESULT WINAPI DirectSoundCaptureCreate(LPCGUID device, LPDIRECTSOUNDCAPTURE* capture, LPUNKNOWN outer)
{
    const auto function = Resolve<decltype(&::DirectSoundCaptureCreate)>("DirectSoundCaptureCreate");
    return function != nullptr ? function(device, capture, outer) : E_FAIL;
}

extern "C" HRESULT WINAPI DirectSoundCaptureEnumerateA(LPDSENUMCALLBACKA callback, LPVOID context)
{
    const auto function = Resolve<decltype(&::DirectSoundCaptureEnumerateA)>("DirectSoundCaptureEnumerateA");
    return function != nullptr ? function(callback, context) : E_FAIL;
}

extern "C" HRESULT WINAPI DirectSoundCaptureEnumerateW(LPDSENUMCALLBACKW callback, LPVOID context)
{
    const auto function = Resolve<decltype(&::DirectSoundCaptureEnumerateW)>("DirectSoundCaptureEnumerateW");
    return function != nullptr ? function(callback, context) : E_FAIL;
}

extern "C" HRESULT WINAPI DirectSoundCreate8(LPCGUID device, LPDIRECTSOUND8* sound, LPUNKNOWN outer)
{
    const auto function = Resolve<decltype(&::DirectSoundCreate8)>("DirectSoundCreate8");
    return function != nullptr ? function(device, sound, outer) : E_FAIL;
}

extern "C" HRESULT WINAPI DirectSoundCaptureCreate8(LPCGUID device, LPDIRECTSOUNDCAPTURE8* capture, LPUNKNOWN outer)
{
    const auto function = Resolve<decltype(&::DirectSoundCaptureCreate8)>("DirectSoundCaptureCreate8");
    return function != nullptr ? function(device, capture, outer) : E_FAIL;
}

extern "C" HRESULT WINAPI DirectSoundEnumerateA(LPDSENUMCALLBACKA callback, LPVOID context)
{
    const auto function = Resolve<decltype(&::DirectSoundEnumerateA)>("DirectSoundEnumerateA");
    return function != nullptr ? function(callback, context) : E_FAIL;
}

extern "C" HRESULT WINAPI DirectSoundEnumerateW(LPDSENUMCALLBACKW callback, LPVOID context)
{
    const auto function = Resolve<decltype(&::DirectSoundEnumerateW)>("DirectSoundEnumerateW");
    return function != nullptr ? function(callback, context) : E_FAIL;
}

extern "C" HRESULT WINAPI DirectSoundFullDuplexCreate(
    LPCGUID captureDevice,
    LPCGUID renderDevice,
    LPCDSCBUFFERDESC captureBufferDescription,
    LPCDSBUFFERDESC renderBufferDescription,
    HWND window,
    DWORD level,
    LPDIRECTSOUNDFULLDUPLEX* fullDuplex,
    LPDIRECTSOUNDCAPTUREBUFFER8* captureBuffer,
    LPDIRECTSOUNDBUFFER8* renderBuffer,
    LPUNKNOWN outer)
{
    const auto function = Resolve<decltype(&::DirectSoundFullDuplexCreate)>("DirectSoundFullDuplexCreate");
    return function != nullptr
        ? function(captureDevice, renderDevice, captureBufferDescription, renderBufferDescription, window, level,
            fullDuplex, captureBuffer, renderBuffer, outer)
        : E_FAIL;
}

extern "C" HRESULT WINAPI GetDeviceID(LPCGUID source, LPGUID destination)
{
    const auto function = Resolve<decltype(&::GetDeviceID)>("GetDeviceID");
    return function != nullptr ? function(source, destination) : E_FAIL;
}

extern "C" HRESULT WINAPI DllCanUnloadNow()
{
    using Function = HRESULT(WINAPI*)(void);
    const auto function = Resolve<Function>("DllCanUnloadNow");
    return function != nullptr ? function() : E_FAIL;
}

extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID classId, REFIID interfaceId, LPVOID* object)
{
    using Function = HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*);
    const auto function = Resolve<Function>("DllGetClassObject");
    return function != nullptr ? function(classId, interfaceId, object) : E_FAIL;
}
