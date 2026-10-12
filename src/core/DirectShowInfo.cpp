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

struct DirectShowControls::Impl {
    ComScope com;
    IBaseFilter *filter = nullptr;
    IAMCameraControl *camera = nullptr;
    IAMVideoProcAmp *procAmp = nullptr;
    DsControlRange ranges[4];
};

namespace {
bool isCameraControl(DsControl c) { return c == DsControl::Exposure; }

long cameraProperty(DsControl) { return CameraControl_Exposure; }

long procAmpProperty(DsControl c)
{
    switch (c) {
    case DsControl::Gain: return VideoProcAmp_Gain;
    case DsControl::Brightness: return VideoProcAmp_Brightness;
    default: return VideoProcAmp_WhiteBalance;
    }
}
}

DirectShowControls::DirectShowControls(int cameraIndex) : d(new Impl)
{
    ICreateDevEnum *deviceEnum = nullptr;
    if (FAILED(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                                IID_ICreateDevEnum, reinterpret_cast<void **>(&deviceEnum))) || !deviceEnum)
        return;

    IEnumMoniker *monikers = nullptr;
    if (deviceEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &monikers, 0) == S_OK && monikers) {
        IMoniker *moniker = nullptr;
        int index = 0;
        while (monikers->Next(1, &moniker, nullptr) == S_OK) {
            if (index++ == cameraIndex) {
                moniker->BindToObject(nullptr, nullptr, IID_IBaseFilter, reinterpret_cast<void **>(&d->filter));
                moniker->Release();
                break;
            }
            moniker->Release();
        }
        monikers->Release();
    }
    deviceEnum->Release();
    if (!d->filter)
        return;

    d->filter->QueryInterface(IID_IAMCameraControl, reinterpret_cast<void **>(&d->camera));
    d->filter->QueryInterface(IID_IAMVideoProcAmp, reinterpret_cast<void **>(&d->procAmp));

    for (const DsControl control : {DsControl::Exposure, DsControl::Gain, DsControl::Brightness, DsControl::WhiteBalance}) {
        long min = 0, max = 0, step = 1, def = 0, caps = 0;
        HRESULT hr = E_FAIL;
        bool hasAuto = false;
        if (isCameraControl(control) && d->camera) {
            hr = d->camera->GetRange(cameraProperty(control), &min, &max, &step, &def, &caps);
            hasAuto = (caps & CameraControl_Flags_Auto) != 0;
        } else if (!isCameraControl(control) && d->procAmp) {
            hr = d->procAmp->GetRange(procAmpProperty(control), &min, &max, &step, &def, &caps);
            hasAuto = (caps & VideoProcAmp_Flags_Auto) != 0;
        }
        DsControlRange &range = d->ranges[static_cast<int>(control)];
        if (SUCCEEDED(hr) && max > min) {
            range.available = true;
            range.min = min;
            range.max = max;
            range.step = step > 0 ? step : 1;
            range.defaultValue = def;
            range.autoSupported = hasAuto;
        }
    }
}

DirectShowControls::~DirectShowControls()
{
    if (d->camera)
        d->camera->Release();
    if (d->procAmp)
        d->procAmp->Release();
    if (d->filter)
        d->filter->Release();
    delete d;
}

bool DirectShowControls::isValid() const
{
    return d->filter && (d->camera || d->procAmp);
}

DsControlRange DirectShowControls::range(DsControl control) const
{
    return d->ranges[static_cast<int>(control)];
}

bool DirectShowControls::read(DsControl control, long &value, bool &isAuto) const
{
    if (!range(control).available)
        return false;
    long flags = 0;
    HRESULT hr = E_FAIL;
    if (isCameraControl(control) && d->camera) {
        hr = d->camera->Get(cameraProperty(control), &value, &flags);
        isAuto = (flags & CameraControl_Flags_Auto) != 0;
    } else if (!isCameraControl(control) && d->procAmp) {
        hr = d->procAmp->Get(procAmpProperty(control), &value, &flags);
        isAuto = (flags & VideoProcAmp_Flags_Auto) != 0;
    }
    return SUCCEEDED(hr);
}

bool DirectShowControls::writeManual(DsControl control, long value)
{
    const DsControlRange r = range(control);
    if (!r.available)
        return false;
    value = std::max(r.min, std::min(r.max, value));
    if (isCameraControl(control) && d->camera)
        return SUCCEEDED(d->camera->Set(cameraProperty(control), value, CameraControl_Flags_Manual));
    if (!isCameraControl(control) && d->procAmp)
        return SUCCEEDED(d->procAmp->Set(procAmpProperty(control), value, VideoProcAmp_Flags_Manual));
    return false;
}

bool DirectShowControls::writeAuto(DsControl control)
{
    const DsControlRange r = range(control);
    if (!r.available || !r.autoSupported)
        return false;
    long value = r.defaultValue;
    bool isAuto = false;
    read(control, value, isAuto);
    if (isCameraControl(control) && d->camera)
        return SUCCEEDED(d->camera->Set(cameraProperty(control), value, CameraControl_Flags_Auto));
    if (!isCameraControl(control) && d->procAmp)
        return SUCCEEDED(d->procAmp->Set(procAmpProperty(control), value, VideoProcAmp_Flags_Auto));
    return false;
}

#else

QVector<DirectShowDevice> enumerateDirectShowDevices()
{
    return {};
}

struct DirectShowControls::Impl {};
DirectShowControls::DirectShowControls(int) : d(new Impl) {}
DirectShowControls::~DirectShowControls() { delete d; }
bool DirectShowControls::isValid() const { return false; }
DsControlRange DirectShowControls::range(DsControl) const { return {}; }
bool DirectShowControls::read(DsControl, long &, bool &) const { return false; }
bool DirectShowControls::writeManual(DsControl, long) { return false; }
bool DirectShowControls::writeAuto(DsControl) { return false; }

#endif
