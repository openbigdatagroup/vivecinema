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
 * @file	BLJPEG.h
 * @author	andre chen
 * @history	2014/06/16 created
 *
 */
#ifndef BL_JPEG_H
#define BL_JPEG_H

namespace mlabs { namespace balai { namespace image {

//
// forget to free pointer return will leak.
// since return pointer has type of void*, you must free it after used,.
// (array) deleting void* is undefined.
//

//
// decompress(from byte memory)
// components = 1(grayscale) or 3(color, pixels in R, G, B order)
void* decompress_JPEG(void const* jpg, int size, int& width, int& height, int& components, int max_size=-1);

//
// compress(to byte memory)
// components = 1(grayscale) or 3(color, pixels in R, G, B order)
void* compress_JPEG(int& size, void const* pixels, int width, int height, int components=3, int quality=90);

//
// handy function to get JPEG image size & components.
bool get_JPEG_Info(void const* jpg, int size, int& width, int& height, int& components);
bool get_JPEG_Info(char const* filename, int& width, int& height, int& components);

//
// compress jpg with mipmap level=0, 1, 2, 3, 4.  each level halfs the (level-1) in both widht and height
// level 0 : width x height
// level 1 : width/2 x height/2
// level 2 : width/4 x height/4
// level 3 : width/8 x height/8
// level 4 : width/16 x height/16
// too bad that this version of libjpeg does not support this yet
//void* compress_JPEG_level(int& size, void const* pixels, int width, int height, int level, int components=3, int quality=90);
//

//
// read JPEG from file (use decompress_JPEG() above)
void* read_JPEG(char const* filename, int& width, int& height, int& components, int max_size=-1);

//
// write JPEG file (use compress_JPEG() above)
bool write_JPEG(char const* filename, void const* pixels, int width, int height, int components=3, int quality=90);

}}}

#endif