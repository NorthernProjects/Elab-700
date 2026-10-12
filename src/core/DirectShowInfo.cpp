#include "DirectShowInfo.h"

#if defined(Q_OS_WIN)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dshow.h>
#include <dvdmedia.h>

#include <algorithm>
#include <cstdlib>
#include <vector>

#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace {

// Apartment-threaded to match the Qt GUI thread; a worker thread simply gets
// its own apartment. RPC_E_CHANGED_MODE (thread already initialized the other
// way) is fine too — COM is usable, we just must not uninitialize it.
struct ComScope {
    HRESULT hr;
    ComScope() : hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ComScope()
    {
        if (SUCCEEDED(hr))
            CoUninitialize();
    }
};

void freeMediaType(AM_MEDIA_TYPE *mt)
{
    if (!mt)
        return;
    if (mt->cbFormat != 0 && mt->pbFormat)
        CoTaskMemFree(mt->pbFormat);
    if (mt->pUnk)
        mt->pUnk->Release();
    CoTaskMemFree(mt);
}

QString readProperty(IPropertyBag *bag, const wchar_t *key)
{
    VARIANT value;
    VariantInit(&value);
    QString result;
    if (SUCCEEDED(bag->Read(key, &value, nullptr)) && value.vt == VT_BSTR && value.bstrVal)
        result = QString::fromWCharArray(value.bstrVal);
    VariantClear(&value);
    return result;
}

void collectResolutions(IAMStreamConfig *config, QVector<QSize> &out)
{
    int count = 0;
    int capsSize = 0;
    if (FAILED(config->GetNumberOfCapabilities(&count, &capsSize)) || capsSize <= 0)
        return;

    std::vector<BYTE> caps(static_cast<size_t>(capsSize));
    for (int i = 0; i < count; ++i) {
        AM_MEDIA_TYPE *mt = nullptr;
        if (config->GetStreamCaps(i, &mt, caps.data()) != S_OK || !mt)
            continue;

        long width = 0;
        long height = 0;
        if (mt->majortype == MEDIATYPE_Video && mt->pbFormat) {
            if (mt->formattype == FORMAT_VideoInfo && mt->cbFormat >= sizeof(VIDEOINFOHEADER)) {
                const auto *info = reinterpret_cast<const VIDEOINFOHEADER *>(mt->pbFormat);
                width = info->bmiHeader.biWidth;
                height = std::labs(info->bmiHeader.biHeight);
            } else if (mt->formattype == FORMAT_VideoInfo2 && mt->cbFormat >= sizeof(VIDEOINFOHEADER2)) {
                const auto *info = reinterpret_cast<const VIDEOINFOHEADER2 *>(mt->pbFormat);
                width = info->bmiHeader.biWidth;
                height = std::labs(info->bmiHeader.biHeight);
            }
        }
        freeMediaType(mt);

        if (width > 0 && height > 0) {
            const QSize size(static_cast<int>(width), static_cast<int>(height));
            if (!out.contains(size))
                out.append(size);
        }
    }
}

void collectPinResolutions(IBaseFilter *filter, QVector<QSize> &out)
{
    IEnumPins *pins = nullptr;
    if (FAILED(filter->EnumPins(&pins)) || !pins)
        return;

    IPin *pin = nullptr;
    while (pins->Next(1, &pin, nullptr) == S_OK) {
        PIN_DIRECTION direction;
        if (SUCCEEDED(pin->QueryDirection(&direction)) && direction == PINDIR_OUTPUT) {
            IAMStreamConfig *config = nullptr;
            if (SUCCEEDED(pin->QueryInterface(IID_IAMStreamConfig, reinterpret_cast<void **>(&config))) && config) {
                collectResolutions(config, out);
                config->Release();
            }
        }
        pin->Release();
        pin = nullptr;
    }
    pins->Release();
}

} // namespace

QVector<DirectShowDevice> enumerateDirectShowDevices()
{
    QVector<DirectShowDevice> devices;
    ComScope com;

    ICreateDevEnum *deviceEnum = nullptr;
    if (FAILED(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                                IID_ICreateDevEnum, reinterpret_cast<void **>(&deviceEnum))) || !deviceEnum)
        return devices;

    IEnumMoniker *monikers = nullptr;
    // S_FALSE means "category is empty", which leaves monikers null.
    if (deviceEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &monikers, 0) == S_OK && monikers) {
        IMoniker *moniker = nullptr;
        int index = 0;
        while (monikers->Next(1, &moniker, nullptr) == S_OK) {
            DirectShowDevice device;
            device.index = index++;

            IPropertyBag *bag = nullptr;
            if (SUCCEEDED(moniker->BindToStorage(nullptr, nullptr, IID_IPropertyBag, reinterpret_cast<void **>(&bag))) && bag) {
                device.name = readProperty(bag, L"FriendlyName");
                device.devicePath = readProperty(bag, L"DevicePath");
                bag->Release();
            }

            IBaseFilter *filter = nullptr;
            if (SUCCEEDED(moniker->BindToObject(nullptr, nullptr, IID_IBaseFilter, reinterpret_cast<void **>(&filter))) && filter) {
                collectPinResolutions(filter, device.resolutions);
                filter->Release();
            }

            std::sort(device.resolutions.begin(), device.resolutions.end(), [](const QSize &a, const QSize &b) {
                const long areaA = static_cast<long>(a.width()) * a.height();
                const long areaB = static_cast<long>(b.width()) * b.height();
                return areaA > areaB;
            });

            devices.append(device);
            moniker->Release();
            moniker = nullptr;
        }
        monikers->Release();
    }
    deviceEnum->Release();
    return devices;
}

#else

QVector<DirectShowDevice> enumerateDirectShowDevices()
{
    return {};
}

#endif
