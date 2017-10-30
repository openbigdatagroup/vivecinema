# Vive Cinema

Vive Cinema is an open source VR media player application depended on HTC Vive. This application needs be executed on Windows 7 platform or later, with the software steamVR running. The basic functions are implemented in C++ language and OpenGL 4 API. Furthermore, there are advanced features such as spatial audio decoding and decoding with GPU acceleration. These features are depended on other related libraries which are listed in "Third Parties" section.

### Feature Highlights

* Video Types: 2D/stereo videos, 180/360 degree videos, stereo 180/360 degree videos
     * Spatial Audio with HRTF (SADIE Binaural Measurement KU100)
     * FOA ([youtube spatial audio spec](https://github.com/google/spatial-media/blob/master/docs/spatial-audio-rfc.md))
     * Fuma
     * TBE ([Facebook 360 Spatial WorkStation](https://facebook360.fb.com/spatial-workstation/))
     * TOA
     * ITU 5.1, 7.1
* Hardware Acceleration for decoding. ( NVIDIA and AMD )
* Subtitle for 180/360 videos
* Virtual screen for 2D Videos with control gestures
    
## Prerequisites

* Windows 7, Windows 8, or Windows 10
* Microsoft Visual Studio 2012
* SteamVR
* HTC Vive Hardware

## Third Parties (in alphabetical order)

* [Advanced Media Framework (AMF) SDK 1.4.4](https://github.com/GPUOpen-LibrariesAndSDKs/AMF)
* [FFmpeg](https://github.com/FFmpeg/FFmpeg)
* [glew 1.13](http://glew.sourceforge.net/)
* [kiss_fft130](https://sourceforge.net/projects/kissfft/)
* [NVIDIA Video Codec SDK 8.0](https://developer.nvidia.com/nvidia-video-codec-sdk)
* [openvr 1.0.2](https://github.com/ValveSoftware/openvr/releases/tag/v1.0.2)
* [SDL2 2.0.3](https://www.libsdl.org/) 
* [uchardet 0.0.6](https://github.com/BYVoid/uchardet)

## Getting Started

You can find Visual Studio 2012 solution file ViveCinema.sln in folder vivecinema. Open it and build.
        
## Run

### Setup HTC Vive  
        
You need connect the HTC Vive equipment in advanced. The minimum equipments that Vive Cinema needs are the HMD and one base station. A controller is optional but recommanded.

### First Run the program

You can run the program if the solution is built successfully. The default video folder path is the folder `Vive Cinema` in Windows Video Library. If Vive Cinema can’t find the folder, it will create one automatically. And there is two default demonstration videos included in the folder: Watch Me Please.mp4 and The Deserted.mp4. 
* Watch Me Please.mp4
     * 2D videos to introduce the features of Vive Cinema and supportive gestures.
     * Subtitle feature show case
* The Deserted.mp4 (2017 VR Movie Trailer by Director Ming-Liang Tsai)
     * 360 stereo videos with 4Kx4K resolution
     * TBE spatial audio

### Put Your Videos Into `Vive Cinema` Folder or Set Your Video Path and Run
 
There is another way to set your video path: modify the xml file `vivecinema.xml` in the `Vive Cinema` folder created before.
* Open `vivecinema.xml` by Notepad or Notepad++
* Write down your video directories absolute path between the tag `<videopaths><\videopaths>`
* Write down your videos absolute path between the tag `<videos><\videos>`, and it allows to manual set sv3d, sa3d preferences for each video.
* The xml file should be saved as UTF-8 format. 


### The Whole Features of Vive Cinema

* Video Types: 2D/Stereo Videos, 180/360 Degree Videos, Stereo 180/360 Degree Videos
     * File Format: .mp4, .mov, .mkv, .divx
* Spatial Audio 
     * FOA ([Youtube Spatial Audio Spec](https://github.com/google/spatial-media/blob/master/docs/spatial-audio-rfc.md))
     * Fuma
     * TBE ([Facebook 360 Spatial WorkStation](https://facebook360.fb.com/spatial-workstation/))
     * ITU 5.1, 7.1
* Subtitle for All Types of Video (Espatially 180/360 Videos)
     * External File Formats: .ass, .srt
* Control Gestures
     * [Select Video] 
          1. Point to the video thumbnail, pull Trigger.
          2. Swipe left and right on Touchpad of controller to the next page if more than 12 videos.
     * [Manual change the playing mode to stereo or 360 video]
          1. In the Video Selection page, the right-top corner of thumbnail will show a setting icon. Point to this icon then it will show a video type selection panel. 
          2. Pull Trigger to switch the mode. You can select the combination of 3D SBS, 3D TB, 360, 180, Noticed 360 SBS supports to some video claimed stereo 180 degree.
     * [Adjust the screen position]
          1. For flat, stereo(3D) videos
               - Point to the screen, pull and hold Trigger. then point to any position to put down the screen.
               - Point to the screen, pull and hold Trigger, swipe top/down on Touchpad to adjust the distance of the screen.
               - Point to the screen, pull and hold Trigger, press Touchpad to rotate the screen.
          2. For 360, stereo 360 videos
               - Press and hold Trigger, swipe left/right on Touchpad to adjust the view direction on the center.
     * [Play, Pause, Next Video, and Volume control]
          1. Press Menu to show or hide the widget panel.
          2. Point to the function icon on the widget panel, press Trigger.
          3. Press Grip to Pause or Start the video.

## Advanced

Vive Cinema shows how to decode videos using GPU acceleration. We studied two kinds of GPU acceleration framework: NVIDIA Video Codec SDK and Advanced Media Framework (AMF) SDK.
The demonstration codes are in [HWAccelDecoder.h](vivecinema/HWAccelDecoder.h) and [HWAccelDecoder.cpp](vivecinema/HWAccelDecoder.cpp).

## Appendix: Balai Example

Balai is a OpenGL-based rendering framework which Vive Cinema uses. We provide two examples to show how to manipulate it.

* HelloBalai

    It is the simplest example to show the balai rendering framework.

* HelloBalaiVR

    It is an example to demonstrate how to manipulate VR features with balai rendering framework. The default (and the only) component is depended on [openvr](https://github.com/ValveSoftware/openvr).

## License

Vive Cinema is distributed under the terms of GPLv3.

