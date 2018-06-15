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
 * @file    BLPNG.cpp
 * @author  andre chen
 * @history 2014/08/20 created
 *
 */
#include "BLPNG.h"
#include "BLCore.h"

#include "png.h"
//#include "pngstruct.h" // even though there is jmp_buf inside PNG structures,
                         // but it's evils to access PNG structures directly.
//#include <setjmp.h> // automatically included in pngconf.h, itself included in png.h

int const MIN_PNG_SIZE = 48; // PNG signature(8) + IHDR(12+n) + IDAT(12+n)xMany + IEND(12),
                             // should be way bigger!
namespace mlabs { namespace balai { namespace image {

struct DataStream {
    jmp_buf	env_;
    uint8 const* begin;
    uint8 const* end;
    uint8* ptr;	   // read/write ptr
    uint8* output; // decode pixels(read mode) or encoded png(write mode)
    png_bytep* scanlines;

    DataStream():begin(NULL),end(NULL),ptr(NULL),output(NULL),scanlines(NULL) {}

    bool SetDataReadSource(uint8 const* s, uint8 const* e) {
        if (NULL!=s || s<e) {
            begin = s; 
            end   = e;
            ptr = (uint8*) begin;
            return true;
        }
        return false;
    }

    bool Read(void* out, int size) {
        if (NULL!=out && (end-ptr)>=size) {
            memcpy(out, ptr, size);
            ptr += size;
            return true;
        }
        return false;
    }

    bool Write(void const* src, int size) {
        if (NULL!=src && size>0) {
            // write pattern (size)
            // signature : 8 bytes
            // IHDR      : [8+13+4]
            // IDAT      : [8+n+4], repeat several times, n may as large as 8192
            // IEND      : [8+4]
            if ((end-ptr)<size || NULL==ptr) {
                int const prev_bytes = int(ptr - begin);
                int const min_extend_size = prev_bytes + size + 16;
                int malloc_size = min_extend_size + (512<<10); // +512K
                output = (uint8*) malloc(malloc_size);
                if (NULL==output) {
                    output = (uint8*) malloc(min_extend_size);
                    if (NULL!=output) {
                        malloc_size = min_extend_size;
                    }
                    else {
                        return false;
                    }
                }

                end = output + malloc_size;
                if (NULL!=begin) {
                    if (prev_bytes>0)
                        memcpy(output, begin, prev_bytes);

                    free((void*)begin); // ???
                }
                begin = output;
                ptr = output + prev_bytes;
            }

            memcpy(ptr, src, size);
            ptr += size;

            return true;
        }
        return false;
    }

    static void png_read(png_structp png_ptr, png_bytep dst, png_size_t size) {
        if (NULL!=png_ptr) {
            DataStream* in = reinterpret_cast<DataStream*>(png_get_io_ptr(png_ptr));
            if (NULL!=in) {
                if (in->Read(dst, (int)size))
                    return;

                BL_ERR("png_read - failed to read memory(dst:%p size:%d)!\n", dst, size);
                longjmp(in->env_, 1);
            }
            else {
                BL_ERR("png_read - png_ptr:%p with null io_ptr\n", png_ptr);
            }
        }
        else {
            BL_ERR("png_read - null png_ptr!!!\n");
        }
    }

    static void png_write(png_structp png_ptr, png_bytep src, png_size_t size) {
        if (NULL!=png_ptr) {
            DataStream* out = reinterpret_cast<DataStream*>(png_get_io_ptr(png_ptr));
            if (NULL!=out) {
                if (out->Write(src, (int)size))
                    return;

                BL_ERR("png_write - failed to write memory(src:%p size:%d)!\n", src, size);
                longjmp(out->env_, 1);
            }
            else {
                BL_ERR("png_write - png_ptr:%p with null io_ptr\n", png_ptr);
            }
        }
        else {
            BL_ERR("png_write - null png_ptr!!!\n");
        }
    }
    static void png_write_flush(png_structp /*png_ptr*/) {}

    static void png_error_handler(png_structp png_ptr, png_const_charp msg) {
        if (NULL!=msg) {
            BL_ERR("png_error_handler - error msg:%s\n", msg);
        }

        if (NULL!=png_ptr) {
            DataStream* in = reinterpret_cast<DataStream*>(png_get_error_ptr(png_ptr));
            if (NULL!=in) {
                longjmp(in->env_, 1);
            }
            else {
                BL_ERR("png_error_handler - png_ptr:%p with null error_ptr\n", png_ptr);
            }
        }
        else {
            BL_ERR("png_error_handler - null png_ptr!!!\n");
        }
    }
};

static void* read_PNG_internal(uint8 const* begin, uint8 const* end, int& width, int& height, int& channels, int& bits)
{
    if (NULL==begin || ((begin+MIN_PNG_SIZE)>=end) || !png_check_sig(begin, 8))
        return NULL;

    // input stream
    DataStream is;
    if (is.SetDataReadSource(begin+8, end)) {
        png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                        &is, DataStream::png_error_handler, NULL);
        if (NULL==png_ptr) {
            return NULL;
        }

        png_infop info_ptr = png_create_info_struct(png_ptr);
        if (NULL==info_ptr) {
            png_destroy_read_struct(&png_ptr, NULL, NULL);
            return NULL;
        }

        if (setjmp(is.env_)) {
            // if we get here, DataStream::png_read() had encountered some troubles
            // it needs to clean up the both PNG objects, and return NULL.
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            if (NULL!=is.output) {
                free(is.output);
                is.output = NULL;
            }
            if (NULL!=is.scanlines) {
                free(is.scanlines);
                is.scanlines = NULL;
            }
            return NULL;
        }

        // set stream in
        png_set_read_fn(png_ptr, &is, DataStream::png_read);

        // tell libpng we already read the 8-bytes signature
        png_set_sig_bytes(png_ptr, 8);

        // read all PNG info up to image data
        png_read_info(png_ptr, info_ptr);

        // get width, height, bit-depth and color-type
        // alternatively, could make separate calls to png_get_image_width(),
        // etc., but want bit_depth and color_type for later [don't care about
        // compression_type and filter_type => NULLs]
        png_uint_32 w(0), h(0);
        int color_type(0), bit_depth(0), interlace_type(PNG_INTERLACE_NONE);
        png_get_IHDR(png_ptr, info_ptr, &w, &h, &bit_depth, &color_type, &interlace_type, NULL, NULL);

        // expand palette images to RGB, low-bit-depth grayscale images to 8 bits,
        // transparency chunks to full alpha channel; strip 16-bit-per-sample
        // images to 8 bits per sample?
        //
        // png_set_expand(png_ptr) -
        //  Expand paletted images to RGB, expand grayscale images of
        //  less than 8-bit depth to 8-bit depth, and expand tRNS chunks
        //  to alpha channels.
        //
#if 1
        if (color_type==PNG_COLOR_TYPE_PALETTE) {
            png_set_palette_to_rgb(png_ptr);
        }
        else if (color_type==PNG_COLOR_TYPE_GRAY && bit_depth<8) {
            png_set_expand_gray_1_2_4_to_8(png_ptr);
        }
        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
            png_set_tRNS_to_alpha(png_ptr);
        }
#else
        if (color_type==PNG_COLOR_TYPE_PALETTE ||
            /*color_type==PNG_COLOR_TYPE_GRAY ||*/ bit_depth<8 ||
            png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
            png_set_expand(png_ptr);
        }
#endif

        if (PNG_INTERLACE_NONE!=interlace_type) {
#ifdef PNG_DEBUG_VERBOSE
            BL_LOG("ReadPNG - interlace image type:%d\n", interlace_type);
#endif
            // avoid warning -
            // "libpng warning: Interlace handling should be turned on when using png_read_image"
            png_set_interlace_handling(png_ptr);
        }

        // png_set_strip_16(png_ptr) chops 16-bit depth files to 8-bit depth
        if (bit_depth==16) {
            if (8==bits) {
                png_set_strip_16(png_ptr);
            }
            else {
                bits = 16;
            }
        }
        else {
            bits = 8;
        }

        // all transformations have been registered; now update info_ptr data
        png_read_update_info(png_ptr, info_ptr);

        // get rowbytes and channels, and allocate image memory
        int const ch = (int) png_get_channels(png_ptr, info_ptr);
        int const strides = ch * w * (bits/8);
        int const rowbytes = (int) png_get_rowbytes(png_ptr, info_ptr);
        bit_depth = (int) png_get_bit_depth(png_ptr, info_ptr);

        if (bits!=bit_depth) {
            BL_ERR("PNG bit_depth%d not coincident!!!\n", bit_depth);
        }

        if (rowbytes>=strides) {
#ifdef PNG_DEBUG_VERBOSE
            if (rowbytes>strides) {
                BL_LOG("ReadPNG - png padding!? row bytes:%d strides:%d bit_depth:%d width:%d type:%d\n",
                    rowbytes, strides, bit_depth, w, color_type);
            }
#endif
            is.output = (uint8*) malloc(h*strides + (rowbytes-strides));
            if (NULL!=is.output) {
                if (rowbytes==strides || PNG_INTERLACE_NONE!=interlace_type) {
                    is.scanlines = (png_bytep*) malloc(h*sizeof(png_bytep));
                    if (NULL!=is.scanlines) {
                        for (png_uint_32 i=0; i<h; ++i) {
                            is.scanlines[i] = is.output + i*rowbytes;
                        }
                    }
                    // if NULL==in.scanlines, read rows one after another...
                }

                if (NULL!=is.scanlines) {
                    png_read_image(png_ptr, is.scanlines);
                    free(is.scanlines);
                    is.scanlines = NULL;

                    // fix padding!? (interlace mode)
                    if (rowbytes>strides) {
                        for (png_uint_32 i=1; i<h; ++i) {
                            memmove(is.output+i*strides, is.output+i*rowbytes, strides);
                        }
                    }
                }
                else if (PNG_INTERLACE_NONE!=interlace_type) {
                    for (png_uint_32 i=0; i<h; ++i) {
                        png_read_row(png_ptr, (png_bytep)(is.output + i*strides), NULL);
                    }
                }
                else {
                    // failed(interlace mode)...
                    free(is.output);
                    is.output = NULL;
                }
            }
            else {
                BL_ERR("ReadPNG - out of memory, failed to malloc(%d)\n", h*strides + (rowbytes-strides));
            }
        }
        else {
            BL_LOG("ReadPNG - invalid row bytes size!!! row bytes:%d strides:%d bit_depth:%d width:%d type:%d\n",
                    rowbytes, strides, bit_depth, w, color_type);
        }

        // read the additional chunks in the PNG file (not really needed)
        png_read_end(png_ptr, NULL);

        // and we're done
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

#ifdef PNG_DEBUG_VERBOSE
        if (is.end!=is.ptr) {
            BL_LOG("Warning!!! ReadPNG - %d bytes remaining!\n", (is.end-is.ptr));
        }
#endif
        if (NULL!=is.output) {
            width  = w;
            height = h;
            channels = ch;
            bits = bit_depth;
        }
        return is.output;
    }
    return NULL;
}

//---------------------------------------------------------------------------------------
void* decompress_PNG(void const* png, int size, int& width, int& height, int& channels, int& bits)
{
    bits = 0;
    return read_PNG_internal((uint8*)png, ((uint8*)png)+size, width, height, channels, bits);
}
//---------------------------------------------------------------------------------------
void* decompress_PNG(void const* png, int size, int& width, int& height, int& channels)
{
    int bits = 8;
    return read_PNG_internal((uint8*)png, ((uint8*)png)+size, width, height, channels, bits);
}

//---------------------------------------------------------------------------------------
void* compress_PNG(int& size, void const* pixels, int width, int height, int channels, int bit_depth)
{
    if (NULL!=pixels && width>0 && height>0 && 0<channels && (channels<=4) && (8==bit_depth||16==bit_depth)) {
        // output stream
        DataStream os;

        // could also replace libpng warning-handler (final NULL), but no need
        png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                &os, DataStream::png_error_handler, NULL);
        if (!png_ptr)
            return NULL;

        png_infop info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) {
            png_destroy_write_struct(&png_ptr, NULL);
            return NULL;
        }

        if (setjmp(os.env_)) {
            png_destroy_write_struct(&png_ptr, &info_ptr);
            if (NULL!=os.output) {
                free(os.output);
                os.output = NULL;
            }
            if (NULL!=os.scanlines) {
                free(os.scanlines);
                os.scanlines = NULL;
            }
            return NULL;
        }

        // set stream out
        png_set_write_fn(png_ptr, &os, DataStream::png_write, DataStream::png_write_flush);
/*
        // set the compression levels--in general, always want to leave filtering
        // turned on (except for palette images) and allow all of the filters,
        // which is the default; want 32K zlib window, unless entire image buffer
        // is 16K or smaller (unknown here)--also the default; usually want max
        // compression (NOT the default); and remaining compression flags should
        // be left alone
        png_set_compression_level(png_ptr, Z_BEST_COMPRESSION); // zlib.h compression level

        // this is default for no filtering; Z_FILTERED is default otherwise:
        png_set_compression_strategy(png_ptr, Z_DEFAULT_STRATEGY);

        // these are all defaults:
        png_set_compression_mem_level(png_ptr, 8);
        png_set_compression_window_bits(png_ptr, 15);
        png_set_compression_method(png_ptr, 8);
 */
        // set the image parameters appropriately
        int color_type = PNG_COLOR_TYPE_RGB;
        if (1==channels)
            color_type = PNG_COLOR_TYPE_GRAY;
        else if (2==channels)
            color_type = PNG_COLOR_TYPE_GRAY_ALPHA;
        else if (4==channels)
            color_type = PNG_COLOR_TYPE_RGB_ALPHA;

        // interlace? keep it simple, plz!
        int const interlace_type = PNG_INTERLACE_NONE;
//      int const interlace_type = PNG_INTERLACE_ADAM7;

        png_set_IHDR(png_ptr, info_ptr, width, height,
                    bit_depth, color_type, interlace_type,
                    PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

        // first write, write all chunks up to (but not including) first IDAT
        png_write_info(png_ptr, info_ptr);

        // set up the transformations(not necessacy since it's for bit_depth<8)
        //png_set_packing(png_ptr);

        // encode image
        int const row_bytes = width*channels*bit_depth/8;
        os.scanlines = (png_bytep*) malloc(height*sizeof(png_bytep));
        if (NULL!=os.scanlines) {
            for (int i=0; i<height; ++i) {
                os.scanlines[i] = ((uint8*) pixels) + i*row_bytes;
            }
        }
        // if NULL==out.scanlines, it still may write rows one after another...

        if (NULL!=os.scanlines) {
            png_write_image(png_ptr, os.scanlines);
            free(os.scanlines);
            os.scanlines = NULL;

            // close out PNG file
            png_write_end(png_ptr, NULL);
        }
        else if (interlace_type==PNG_INTERLACE_NONE) {
            for (int i=0; i<height; ++i) {
                png_write_row(png_ptr, ((uint8*) pixels) + i*row_bytes);
            }

            // close out PNG file
            png_write_end(png_ptr, NULL);
        }
        else {
            // error!!!
            if (NULL!=os.output) {
                free(os.output);
                os.output = NULL;
            }
        }

        // and we are done!
        png_destroy_write_struct(&png_ptr, &info_ptr);

        // size
        size = (int)(os.ptr - os.output);

        return os.output;
    }
    return NULL;
}

//---------------------------------------------------------------------------------------
bool get_PNG_Info(void const* png, int size, int& width, int& height, int& channels, int& bits)
{
    uint8 const* begin = (uint8 const*) png;
    uint8 const* end = begin + size;
    if (NULL==begin || ((begin+MIN_PNG_SIZE)>=end) || !png_check_sig(begin, 8))
        return false;

    // input stream
    DataStream is;
    if (is.SetDataReadSource(begin+8, end)) {
        png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (NULL==png_ptr) {
            return false;
        }

        png_infop info_ptr = png_create_info_struct(png_ptr);
        if (NULL==info_ptr) {
            png_destroy_read_struct(&png_ptr, NULL, NULL);
            return false;
        }

        if (setjmp(is.env_)) {
            // if we get here, DataStream::png_read() had encountered some troubles
            // it needs to clean up the both PNG objects, and return NULL.
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            return false;
        }

        // set stream in
        png_set_read_fn(png_ptr, &is, DataStream::png_read);

        // tell libpng we already read the 8-bytes signature
        png_set_sig_bytes(png_ptr, 8);

        // read all PNG info up to image data
        png_read_info(png_ptr, info_ptr);

        // get width, height, bit-depth and color-type
        // alternatively, could make separate calls to png_get_image_width(),
        // etc., but want bit_depth and color_type for later [don't care about
        // compression_type and filter_type => NULLs]
        png_uint_32 w(0), h(0);
        int color_type(0), bit_depth(0);
        png_get_IHDR(png_ptr, info_ptr, &w, &h, &bit_depth, &color_type, NULL, NULL, NULL);

        // expand palette images to RGB, low-bit-depth grayscale images to 8 bits,
        // transparency chunks to full alpha channel; strip 16-bit-per-sample
        // images to 8 bits per sample?
        //
        // png_set_expand(png_ptr) -
        //  Expand paletted images to RGB, expand grayscale images of
        //  less than 8-bit depth to 8-bit depth, and expand tRNS chunks
        //  to alpha channels.
        //
        if (color_type==PNG_COLOR_TYPE_PALETTE ||
            /*color_type==PNG_COLOR_TYPE_GRAY ||*/ bit_depth<8 ||
            png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
            png_set_expand(png_ptr);
        }

        // all transformations have been registered; now update info_ptr data
        png_read_update_info(png_ptr, info_ptr);

        // get rowbytes and channels, and allocate image memory
        width    = w;
        height   = h;
        channels = (int) png_get_channels(png_ptr, info_ptr);
        bits     = (int) png_get_bit_depth(png_ptr, info_ptr);

        // and we're done
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

        return true;
    }
    return false;
}
//----------------------------------------------------------------------------------------
bool get_PNG_Info(char const* filename, int& width, int& height, int& channels, int& bits)
{
    bool result = false;
    if (NULL!=filename) {
        FILE* file = fopen(filename, "rb");
        if (NULL!=file) {
            fseek(file, 0, SEEK_END);
            int const file_size = ftell(file);
            if (file_size>MIN_PNG_SIZE) {
                uint8* file_data = (uint8*) malloc(file_size);
                if (NULL!=file_data) {
                    rewind(file);
                    fread(file_data, 1, file_size, file);
                    result = get_PNG_Info(file_data, file_size, width, height, channels, bits);
                    free(file_data);
                }
            }
            fclose(file);
        }
    }
    return result;
}
//----------------------------------------------------------------------------------------
void* read_PNG(char const* filename, int& width, int& height, int& channels, int& bits)
{
    void* pixels = NULL;
    if (NULL!=filename) {
        FILE* file = fopen(filename, "rb");
        if (NULL!=file) {
            fseek(file, 0, SEEK_END);
            int const file_size = ftell(file);
            if (file_size>MIN_PNG_SIZE) {
                uint8* file_data = (uint8*) malloc(file_size);
                if (NULL!=file_data) {
                    rewind(file);
                    fread(file_data, 1, file_size, file);

                    bits = 0;
                    pixels = read_PNG_internal(file_data, file_data+file_size,
                                               width, height, channels, bits);
                    free(file_data);
                }
            }
            fclose(file);
        }
    }
    return pixels;
}

//---------------------------------------------------------------------------------------
void* read_PNG(char const* filename, int& width, int& height, int& channels)
{
    void* pixels = NULL;
    if (NULL!=filename) {
        FILE* file = fopen(filename, "rb");
        if (NULL!=file) {
            fseek(file, 0, SEEK_END);
            int const file_size = ftell(file);
            if (file_size>MIN_PNG_SIZE) {
                uint8* file_data = (uint8*) malloc(file_size);
                if (NULL!=file_data) {
                    rewind(file);
                    fread(file_data, 1, file_size, file);

                    int bits = 8;
                    pixels = read_PNG_internal(file_data, file_data+file_size,
                                               width, height, channels, bits);
                    free(file_data);
                }
            }
            fclose(file);
        }
    }
    return pixels;
}

//---------------------------------------------------------------------------------------
bool write_PNG(char const* filename, void const* pixels, int width, int height, int channels, int bits)
{
    //
    // the better approach is to use png_init_io(png_ptr*, FILE*),
    // so it won't have to decompress in memory!
    //
    bool result = false;
    if (NULL!=filename && NULL!=pixels && width>0 && height>0 && 0<channels && channels<5 && (8==bits || 16==bits)) {
        int size = 0;
        void* png = compress_PNG(size, pixels, width, height, channels, bits);
        if (NULL!=png) {
            if (size>0) {
                FILE* file = fopen(filename, "wb");
                if (NULL!=file) {
                    result = (size==(int)fwrite(png, 1, size, file));
                    fclose(file);
                }
            }
            free(png);
        }
    }
    return result;
}

}}} // mlabs::balai::image