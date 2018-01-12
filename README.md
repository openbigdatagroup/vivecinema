# Vive Cinema

Vive Cinema is a light-weight, high performance VR video player for HTC VIVE.

### Features
* Video
	* Plane, 180 or 360
	* Mono, Side-By-Side or Over-Under Stereoscopic 3D
	* Hardware Accelerated Decoder Integrated
		* NVIDIA Video Codec SDK
		* AMD AMF
		* FFMpeg HWAccel
* Audio
	* Spatial Audio(with or without headlock stereo track)
		* Ambix/FuMa : 1st, 2nd and 3rd order ambisonics
		* Facebook TBE
	* Mono, Stereo, ITU 5.1 or ITU 7.1 (multilingual selectable)
* Subtitle (multilingual selectable)
	* Embedded Hardsub/Softsub
	* .SRT
	* .ASS
    
### Prerequisites
* Windows&reg; 7, Windows&reg; 8 or Windows&reg; 10
* Visual Studio&reg; 2012 or Visual Studio&reg; 2017
* HTC Vive&reg; and SteamVR&reg;

### Third-Party Softwares
* [AMD AMF](https://github.com/GPUOpen-LibrariesAndSDKs/AMF)
* [FFmpeg](https://github.com/FFmpeg/FFmpeg)
* [glew](http://glew.sourceforge.net/)
* [kiss_fft130](https://sourceforge.net/projects/kissfft/)
* [NVIDIA Video Codec SDK](https://developer.nvidia.com/nvidia-video-codec-sdk)
* [OpenVR](https://github.com/ValveSoftware/openvr/releases/tag/v1.0.2)
* [SDL2](https://www.libsdl.org/) 
* [uchardet](https://github.com/BYVoid/uchardet)

### Getting Started
* Visual Studio&reg; solutions can be found in the `\vivecinema` directory.

### Run the Program
* Put videos in `\vivecinema\bin\videos` directory or edit `\vivecinema\bin\assets\vivecinema.xml` to specify video directories.
* Refer [Viveport Vive Cinema](https://www.viveport.com/apps/ed3adb70-9390-4ca3-863a-26b5fd08b8d7) for more details.

### Balai 3D/VR
Balai 3D/VR is a rendering framework we created for easily building simple 3D/VR applications. You may find some samples in `\samples` directory.

### Known Issues
* Few memleak reports when app closing ([VS2012](https://connect.microsoft.com/VisualStudio/feedback/details/757212)).
* Leveraging VS2012 non C++ standard std::async() for asynchronization.
* Potential crash on 32-bit build for huge videos(e.g. 4096x4096, 8192x8192). Always use 64-bit build if possible.
* AMD AMF GPU accelerated video decoder is still testing. Switch back to CPU decoder (Press F4) if any problem occurs.

### License
Vive Cinema is distributed under the terms of GPLv3 with respect to FFmpeg.

