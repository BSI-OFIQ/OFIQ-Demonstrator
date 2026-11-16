#pragma once

#include <string>
#include <vector>

// A connected camera as offered in the selection dropdown.
// `index` is the value passed to cv::VideoCapture(index).
struct CameraDevice
{
    int index;
    std::string name;
};

// Enumerate the connected cameras with human-readable names where the platform
// allows it. The returned `index` matches the OpenCV device index, so the
// enumeration order must follow the backend OpenCV uses:
//   - Windows: DirectShow / Media Foundation video input devices (FriendlyName)
//   - Linux:   /dev/videoN nodes reporting V4L2 capture capability (cap.card)
//   - other:   generic probe of indices 0..4 ("Camera N")
inline std::vector<CameraDevice> EnumerateCameras();

#if defined(_WIN32)

#include <windows.h>
#include <dshow.h>

inline std::vector<CameraDevice> EnumerateCameras()
{
    std::vector<CameraDevice> devices;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool comInitialized = SUCCEEDED(hr);

    ICreateDevEnum* pDevEnum = nullptr;
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&pDevEnum));
    if (SUCCEEDED(hr) && pDevEnum != nullptr)
    {
        IEnumMoniker* pEnum = nullptr;
        hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
        if (hr == S_OK && pEnum != nullptr)
        {
            IMoniker* pMoniker = nullptr;
            int index = 0;
            while (pEnum->Next(1, &pMoniker, nullptr) == S_OK)
            {
                std::string name = "Camera " + std::to_string(index);

                IPropertyBag* pPropBag = nullptr;
                if (SUCCEEDED(pMoniker->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&pPropBag)))
                    && pPropBag != nullptr)
                {
                    VARIANT var;
                    VariantInit(&var);
                    if (SUCCEEDED(pPropBag->Read(L"FriendlyName", &var, nullptr)) && var.vt == VT_BSTR)
                    {
                        int len = WideCharToMultiByte(CP_UTF8, 0, var.bstrVal, -1,
                            nullptr, 0, nullptr, nullptr);
                        if (len > 1)
                        {
                            std::string utf8(static_cast<size_t>(len - 1), '\0');
                            WideCharToMultiByte(CP_UTF8, 0, var.bstrVal, -1,
                                &utf8[0], len, nullptr, nullptr);
                            name = utf8;
                        }
                    }
                    VariantClear(&var);
                    pPropBag->Release();
                }

                devices.push_back({ index, name });
                pMoniker->Release();
                index++;
            }
            pEnum->Release();
        }
        pDevEnum->Release();
    }

    if (comInitialized)
    {
        CoUninitialize();
    }

    return devices;
}

#elif defined(__linux__)

#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/videodev2.h>

inline std::vector<CameraDevice> EnumerateCameras()
{
    std::vector<CameraDevice> devices;

    // cv::VideoCapture(i) with the V4L2 backend opens /dev/video<i>, so probe
    // the same node numbers and keep those that can actually capture video.
    for (int i = 0; i < 64; i++)
    {
        std::string path = "/dev/video" + std::to_string(i);
        int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0)
        {
            continue;
        }

        v4l2_capability cap;
        std::memset(&cap, 0, sizeof(cap));
        if (::ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0)
        {
            unsigned int caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS)
                ? cap.device_caps : cap.capabilities;
            if (caps & V4L2_CAP_VIDEO_CAPTURE)
            {
                std::string name(reinterpret_cast<const char*>(cap.card));
                devices.push_back({ i, name.empty() ? path : name });
            }
        }

        ::close(fd);
    }

    return devices;
}

#else

#include <opencv2/opencv.hpp>

inline std::vector<CameraDevice> EnumerateCameras()
{
    std::vector<CameraDevice> devices;

    // No portable name API on this platform: probe the first few indices.
    for (int i = 0; i < 5; i++)
    {
        cv::VideoCapture cap(i);
        if (cap.isOpened())
        {
            devices.push_back({ i, "Camera " + std::to_string(i) });
            cap.release();
        }
    }

    return devices;
}

#endif
