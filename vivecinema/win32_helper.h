/*
 * Copyright (C) 2017 HTC Corporation
 *
 * Vive Cinema is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Vive Cinema is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Notice Regarding Standards. HTC does not provide a license or sublicense to
 * any Intellectual Property Rights relating to any standards, including but not
 * limited to any audio and/or video codec technologies such as MPEG-2, MPEG-4;
 * AVC/H.264; HEVC/H.265; AAC decode/FFMPEG; AAC encode/FFMPEG; VC-1; and MP3
 * (collectively, the "Media Technologies"). For clarity, you will pay any
 * royalties due for such third party technologies, which may include the Media
 * Technologies that are owed as a result of HTC providing the Software to you.
 *
 * @file    win32_helper.h
 * @author  andre chen
 * @history 2016/04/21 created
 *
 */
#ifndef WIN32_TOOLKITS
#define WIN32_TOOLKITS

//#include "BLTexture.h"

#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <ShlObj.h>

namespace htc {

// wraper of SHGetKnownFolderPath(), to get library video path
enum KNOWN_PATH {
    KNOWN_PATH_DOCUMENTS, // %USERPROFILE%\My Documents              FOLDERID_Documents
    KNOWN_PATH_MUSIC,     // %USERPROFILE%\My Documents\My Music     FOLDERID_Music
    KNOWN_PATH_PICTURES,  // %USERPROFILE%\My Documents\My Pictures  FOLDERID_Pictures
    KNOWN_PATH_VIDEOS,    // %USERPROFILE%\My Documents\My Videos    FOLDERID_Videos

    KNOWN_PATH_TOTALS
};

class Win32KnownFolderPath
{
    PWSTR path_;

    void FreePath_() {
        if (NULL!=path_) {
            CoTaskMemFree(path_);
            path_ = NULL;
        }
    }

public:
    Win32KnownFolderPath():path_(NULL) {}
    ~Win32KnownFolderPath() { FreePath_(); }

    WCHAR const* GetPath(KNOWN_PATH my_path) {
        FreePath_();

        HRESULT hr = E_FAIL;
        switch (my_path)
        {
        case KNOWN_PATH_MUSIC:
            hr = SHGetKnownFolderPath(FOLDERID_Music, 0, NULL, &path_);
            break;

        case KNOWN_PATH_PICTURES:
            hr = SHGetKnownFolderPath(FOLDERID_Pictures, 0, NULL, &path_);
            break;

        case KNOWN_PATH_VIDEOS:
            hr = SHGetKnownFolderPath(FOLDERID_Videos, 0, NULL, &path_);
            break;

        default:
            hr = SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &path_);
            break;
        }

        if (FAILED(hr)) {
            FreePath_();
        }

        return path_;
    }
};

#if 0
static void CaptureScreen()
{
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);
    HDC hScrDC = GetDC(NULL); // CreateDC("DISPLAY",0,0,0);
    HDC hMemDC = CreateCompatibleDC(hScrDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hMemDC, width, height);
    HGDIOBJ hGDIOBJ = SelectObject(hMemDC, hBitmap);
    if (!BitBlt(hMemDC, 0,0, width, height, hScrDC, 0,0, SRCCOPY)) {
        MessageBox(NULL, L"BitBlt has failed", L"Failed", MB_OK);
    }
    SelectObject(hMemDC, hGDIOBJ);
    uint8* lpBits = (uint8*) malloc(width*height*4);
    memset(lpBits, 255, width*height*4);
    if (!GetBitmapBits(hBitmap, width*height, lpBits)) {
        MessageBox(NULL, L"GetBitmapBits has failed", L"Failed", MB_OK);
    }


    uint8* pixels = (uint8*)lpBits;

    int const total_pixels = width*height;
    uint8 const* src = pixels;
    uint8* dst = pixels;
    uint8 r, g, b;
    for (int i=0; i<total_pixels; ++i,src+=4) {
        r = src[2];
        g = src[1];
        b = src[0];
        *dst++ = r;
        *dst++ = g;
        *dst++ = b;
    }

    free(lpBits);

    DeleteDC(hMemDC);
}
#endif

#if 1
class WindowsVRWidget
{
public:
    WindowsVRWidget() {}
    bool Initialize(HWND) { return true; }
    mlabs::balai::graphics::ITexture* GetTexture() const {
        return NULL;
    }
    void Finalize() {}
};
#else
class WindowsVRWidget
{
    HWND hWnd_;
    mlabs::balai::graphics::Texture2D* texture_;

public:
    WindowsVRWidget():hWnd_(NULL),texture_(NULL) {}
    ~WindowsVRWidget() {
        assert(NULL==texture_);
    }

    // hwnd
    bool Initialize(HWND hwnd) {
        //if (NULL==hwnd)
        //   hwnd = GetDesktopWindow();
        CaptureScreen();

        //CaptureAnImage();

        int width, height;
        if (NULL==hwnd) {
            width = GetSystemMetrics(SM_CXSCREEN);
            height = GetSystemMetrics(SM_CYSCREEN);
        }
        else {
            RECT rect;
            GetWindowRect(hwnd, &rect);
            width = rect.right - rect.left;
            height = rect.bottom - rect.top;
        }

        HDC hScrDC =  CreateDC(L"DISPLAY",0,0,0); // GetDC(hwnd);
        HDC hMemDC = CreateCompatibleDC(NULL);

        BITMAPINFO bmi = { 0 };
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = width;
        bmi.bmiHeader.biHeight      = -height;
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biCompression = BI_RGB;
        bmi.bmiHeader.biBitCount    = 32;
        DWORD* bitmapPixels         = NULL;
        HBITMAP hBitmap = CreateDIBSection(hMemDC, &bmi, DIB_RGB_COLORS, (void**)&bitmapPixels, NULL, 0);
        HGDIOBJ hBitmapPrev = SelectObject(hMemDC, hBitmap);
        DWORD res = BitBlt(hMemDC, 0,0, width, height, hScrDC, 0,0, SRCCOPY);
        if (!res) {
            MessageBox(NULL, L"BitBlt has failed", L"Failed", MB_OK);
        }

        uint8* pixels = (uint8*)bitmapPixels;

        int const total_pixels = width*height;
        uint8 const* src = pixels;
        uint8* dst = pixels;
        uint8 r, g, b;
        for (int i=0; i<total_pixels; ++i,src+=4) {
            r = src[2];
            g = src[1];
            b = src[0];
            *dst++ = r;
            *dst++ = g;
            *dst++ = b;
        }

        if (NULL==texture_) {
            texture_ = mlabs::balai::graphics::Texture2D::New(0, true);
            texture_->SetAddressMode(mlabs::balai::graphics::ADDRESS_CLAMP,
                                        mlabs::balai::graphics::ADDRESS_CLAMP);
            texture_->SetFilterMode(mlabs::balai::graphics::FILTER_BILINEAR);
        }

        texture_->UpdateImage((uint16) width, (uint16) height,
            mlabs::balai::graphics::FORMAT_RGBA8, pixels, false);

        SelectObject(hMemDC, hBitmapPrev);
        if (hBitmap) DeleteObject(hBitmap);
        DeleteDC(hMemDC);

        return true;
    }

    mlabs::balai::graphics::ITexture* GetTexture() const {
        if (NULL!=texture_) {
            texture_->AddRef();
            return texture_;
        }
        return NULL;
    }

    void Finalize() {
        BL_SAFE_RELEASE(texture_);
        hWnd_ = NULL;
    }
};
#endif

//
// just a copycat from codeproject -
// http://www.codeproject.com/Tips/233484/Change-Master-Volume-in-Visual-Cplusplus
//
class MasterVolumeKeeper
{
    IAudioEndpointVolume* endpointVolume_;
    float master_volume_inf_;
    float master_volume_sup_;
    float master_volume_inc_;
    float master_volume_;
    mutable float master_volume_scalar_; // [0.0f, 1.0f]
    float master_volume_default_;

public:
    MasterVolumeKeeper():endpointVolume_(NULL),
        master_volume_inf_(0.0f),master_volume_sup_(0.0f),master_volume_inc_(0.0f),
        master_volume_(-1.0),master_volume_scalar_(-1.0f),master_volume_default_(-1.0f) {
        CoInitialize(NULL);

        IMMDeviceEnumerator* deviceEnumerator = NULL;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER,
                                       __uuidof(IMMDeviceEnumerator), (LPVOID*) &deviceEnumerator))) {
            IMMDevice* defaultDevice = NULL;
            if (SUCCEEDED(deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice))) {
                if (SUCCEEDED(defaultDevice->Activate(__uuidof(IAudioEndpointVolume), 
                                                      CLSCTX_INPROC_SERVER, NULL, (LPVOID*) &endpointVolume_))) {
                    if (FAILED(endpointVolume_->GetMasterVolumeLevelScalar(&master_volume_scalar_))) {
                        master_volume_scalar_ = -1.0f;
                        endpointVolume_->Release();
                        endpointVolume_ = NULL;
                        BL_ERR(">>>>> master volume keeper failed!!!\n");
                    }
                    else {
                        master_volume_default_ = master_volume_scalar_;
                        HRESULT hr1 = endpointVolume_->GetVolumeRange(&master_volume_inf_,
                                                                  &master_volume_sup_,
                                                                  &master_volume_inc_);
                        HRESULT hr2 = endpointVolume_->GetMasterVolumeLevel(&master_volume_);
                        if (FAILED(hr1) || FAILED(hr2)) {
                            BL_ERR(">>>>> master volume keeper potential failed!?\n");
                        }
                    }
                }
                defaultDevice->Release();
                defaultDevice = NULL;
            }
            deviceEnumerator->Release();
            deviceEnumerator = NULL;
        }
    }
    ~MasterVolumeKeeper() {
        if (NULL!=endpointVolume_) {
            endpointVolume_->Release();
            endpointVolume_ = NULL;
        }
        CoUninitialize();
    }

    // are you OK? master volume keeper?
    operator bool() const { return (NULL!=endpointVolume_); }

    // reset volume from start
    void ResetVolume() { SetVolume(master_volume_default_); }

    // call if master volume change from other apps
    float SyncVolume() const {
        if (NULL!=endpointVolume_ &&
            SUCCEEDED(endpointVolume_->GetMasterVolumeLevelScalar(&master_volume_scalar_))) {
            return master_volume_scalar_;
        }
        return -1.0f;
    }

    // get current volume
    float GetVolume(bool sync=true) const {
        return sync ? SyncVolume():master_volume_scalar_;
    }

    // set volume
    float SetVolume(float vol) {
        if (NULL!=endpointVolume_) {
            if (vol<0.005f) vol = 0.0f; // < 1%
            float const old_volume = master_volume_scalar_;
            if (SUCCEEDED(endpointVolume_->SetMasterVolumeLevelScalar(vol, NULL))) {
                //BL_LOG("old volume:%.2f new volume:%.2f\n", old_volume, vol);
                master_volume_scalar_ = vol;
            }
            return old_volume;
        }
        return -1.0f;
    }
};

} // namespace htc

#endif