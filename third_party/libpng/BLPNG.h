/**
 *
 * HTC Corporation Proprietary Rights Acknowledgment
 * Copyright (c) 2014 HTC Corporation
 * All Rights Reserved.
 *
 * The information contained in this work is the exclusive property of HTC Corporation
 * ("HTC").  Only the user who is legally authorized by HTC ("Authorized User") has
 * right to employ this work within the scope of this statement.  Nevertheless, the
 * Authorized User shall not use this work for any purpose other than the purpose
 * agreed by HTC.  Any and all addition or modification to this work shall be
 * unconditionally granted back to HTC and such addition or modification shall be
 * solely owned by HTC.  No right is granted under this statement, including but not
 * limited to, distribution, reproduction, and transmission, except as otherwise
 * provided in this statement.  Any other usage of this work shall be subject to the
 * further written consent of HTC.
 *
 * @file    BLPNG.h
 * @author  andre chen
 * @history 2014/08/20 created
 *
 */
#ifndef BL_PNG_H
#define BL_PNG_H

namespace mlabs { namespace balai { namespace image {

//
// forget to free pointer return will leak.
// since return pointer has type of void*, must be sure to free(void*) it after used,.
// (array) deleting void* is undefined.
//

//
// decompress(from byte memory)
// components = 1(grayscale) or 3(color, pixels in R, G, B order)
// bits : bit depth (for each channel), 8 or 16 will be returned if success
// for 16 bits image, it's big-endian. i.e. uint16 gray16 = (u8[0]<<8)|u8[1];
void* decompress_PNG(void const* png, int size, int& width, int& height, int& channels, int& bits);
void* decompress_PNG(void const* png, int size, int& width, int& height, int& channels); // load as 8 bits image

//
// compress(to byte memory): <<to be implemented soon!>>
// width/height>0, channels=1(grescale), 2(greyscale-alpha), 3(RGB) or 4(RGBA), bits must be 8 or 16
// e.g. 1) grayscale 8 bits : channels=1, bits=8
//      2) grayscale 16 bits : channels=1, bits=16
//      3) greyscale-alpha 8 bits : channels=2, bits=8
//      4) greyscale-alpha 16 bits : channels=2, bits=16
//      5) RGB 8 bits : channels=3, bits=8
//      6) RGB 16 bits : channels=3, bits=16
//      7) RGBA 8 bits : channels=4, bits=8
//      8) RGBA 16 bits : channels=4, bits=16
void* compress_PNG(int& size, void const* pixels, int width, int height, int channels, int bits);

//
// handy function to get JPEG image size & channels. bits_depth always return 8 or 16
bool get_PNG_Info(void const* png, int size, int& width, int& height, int& channels, int& bits_depth);
bool get_PNG_Info(char const* filename, int& width, int& height, int& channels, int& bits_depth);

//
// read PNG from file (use decompress_PNG() above)
void* read_PNG(char const* filename, int& width, int& height, int& channels, int& bits); // load 8 bits or 16 bits image
void* read_PNG(char const* filename, int& width, int& height, int& channels); // load as 8 bits image

//
// write PNG file (use compress_PNG() above)     
bool write_PNG(char const* filename, void const* pixels, int width, int height, int channels, int bits);

}}}

#endif