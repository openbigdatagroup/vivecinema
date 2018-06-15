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
 * @file	BLJPEG.cpp
 * @author	andre chen
 * @history	2014/06/13 created
 *
 */
#include "BLJPEG.h"
#include "BLCore.h"
#include "jpeglib.h"

#include <setjmp.h>
//#include <math.h> // floor

struct my_error_mgr : public jpeg_error_mgr {
	jmp_buf env;
};

static void my_output_message(j_common_ptr cinfo) {
	char buffer[JMSG_LENGTH_MAX];
	(*cinfo->err->format_message) (cinfo, buffer);
	BL_ERR("    %s\n", buffer);
}

static void my_error_exit(j_common_ptr cinfo) {
	/* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
	my_error_mgr* myerr = (my_error_mgr*) cinfo->err;

	/* Always display the message. */
	/* We could postpone this until after returning, if we chose. */
	(*cinfo->err->output_message) (cinfo);

	// return control to the setjmp point
	/* In C++, the implementation may perform stack unwinding that destroys objects
	 * with automatic duration. If this invokes any non-trivial destructors,
	 * it causes undefined behavior */
	longjmp(myerr->env, 1);
}

namespace mlabs { namespace balai { namespace image {

//---------------------------------------------------------------------------------------
void* decompress_JPEG(void const* jpg, int size, int& width, int& height, int& components, int max_size)
{
	if (NULL!=jpg && size>16 && (max_size<=0||max_size>16)) {
		// Step 1: allocate and initialize JPEG decompression object
		// set up the normal JPEG error routines, override error_exit and output_message.
		jpeg_decompress_struct cinfo;
		my_error_mgr		   err_mgr;
		cinfo.err              = jpeg_std_error(&err_mgr);
		err_mgr.error_exit     = my_error_exit;
		err_mgr.output_message = my_output_message;

		// establish the setjmp return context for my_error_exit to use.
		if (setjmp(err_mgr.env)) {
			// if we get here, the JPEG code has signaled an error.
			// it needs to clean up the JPEG object, and return NULL.
			jpeg_destroy_decompress(&cinfo);
			return NULL;
		}

		// now it can initialize the JPEG decompression object
		jpeg_create_decompress(&cinfo);

		// step 2: specify data source:
		//         (jpeg_stdio_src(&cinfo, file) if from FILE*)
		jpeg_mem_src(&cinfo, (uint8*)jpg, size);

		// step 3: read file parameters with jpeg_read_header()
		(void) jpeg_read_header(&cinfo, TRUE);
		/* we can ignore the return value from jpeg_read_header since
         *   (a) suspension is not possible with the stdio data source, and
         *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
         * see libjpeg.txt for more info.
         */

		// step 4(optional): set parameters for decompression
		width  = cinfo.image_width;
		height = cinfo.image_height;
		//BL_ERR("decode jpg file size:%d, width:%d height:%d", size, width, height);
		if (max_size>4 && (width>max_size||height>max_size)) {
			//
			// (unsigned int) scale_num, scale_denom
			// Scale the image by the fraction scale_num/scale_denom.  Currently,
			// the supported scaling ratios are M/N with all M from 1 to 16, where
			// N is the source DCT size, which is 8 for baseline JPEG.  (The library
			// design allows for arbitrary scaling ratios but this is not likely
			// to be implemented any time soon.)  The values are initialized by
			// jpeg_read_header() with the source DCT size.  For baseline JPEG
			// this is 8/8.  If you change only the scale_num value while leaving
			// the other unchanged, then this specifies the DCT scaled size to be
			// applied on the given input.  For baseline JPEG this is equivalent
			// to M/8 scaling, since the source DCT size for baseline JPEG is 8.
			// Smaller scaling ratios permit significantly faster decoding since
			// fewer pixels need be processed and a simpler IDCT method can be used.
			//

			// default
			cinfo.scale_num = cinfo.scale_denom = 1;

			int w = width;
			int h = height;
			while ((w>max_size || h>max_size) && (cinfo.scale_denom<16)) {
				cinfo.scale_denom <<= 1;
				w >>= 1; h >>= 1;
			}

			// say if you want to set max_size of 4096, but it can have 4000 (no sweat!).
			// i'd say let's accept it. even it can not achieve 4096.
			float const scale_error_threshold = 0.95f; // 4000.0f/4096.0f = 0.9765625

			// but if it downscaled too much, roll back 1 level
			if ((cinfo.scale_denom>1) &&
				(float(w)/float(max_size))<scale_error_threshold &&
				(float(h)/float(max_size))<scale_error_threshold) {
				cinfo.scale_denom >>= 1;
			}
			// also see 2nd stage below...
			//BL_ERR("scale down:%d%%", 100*cinfo.scale_num/cinfo.scale_denom);
		}
//
//		cinfo.two_pass_quantize = FALSE;
//		cinfo.dither_mode = JDITHER_ORDERED;
//		cinfo.dct_method = JDCT_FASTEST;
//		cinfo.do_fancy_upsampling = FALSE;
//		cinfo.do_block_smoothing = FALSE;
//
		// step 5: Start decompressor
		(void) jpeg_start_decompress(&cinfo);
		/* we can ignore the return value since suspension is not possible
         * with the stdio data source. it erroe occur, longjmp will be fired
		 * and this function will return NULL above(ln.70)
         */

		// decompression complete, so now begin extraction of data
		// row_stride = output pixels per row * number of colours per row
		width      = cinfo.output_width;
		height	   = cinfo.output_height;
		components = cinfo.output_components;
		int const row_stride = width * components;
		uint8* buffer = (uint8*) malloc(height*row_stride);

		// step 6: while (scan lines remain to be read)
		//           jpeg_read_scanlines(...);
#ifdef JPEG_MULTIPLE_SCANLINES
		JSAMPROW* scanlines = new JSAMPROW[cinfo.output_height];
		for (int h=0; h<height; ++h) {
			scanlines[h] = (JSAMPROW) (buffer + h*row_stride);
		}
		int row = 0;
		while (cinfo.output_scanline<cinfo.output_height) {
			row += jpeg_read_scanlines(&cinfo, scanlines+row, height-row);
		}
		delete[] scanlines;
#else
		int row = 0;
		while (cinfo.output_scanline<cinfo.output_height) {
			JSAMPROW pRow = buffer + (row*row_stride);
			row += jpeg_read_scanlines(&cinfo, &pRow, 1);
		}
#endif
		// step 7: Finish decompression
		(void) jpeg_finish_decompress(&cinfo);
		/* We can ignore the return value since suspension is not possible
         * with the stdio data source.
         */

		// step 8: Release JPEG decompression object
		// this is an important step since it will release a good deal of memory.
		jpeg_destroy_decompress(&cinfo);

		// perform 2nd stage downsize...
		if (max_size>4 && (width>max_size || height>max_size)) {
			int new_width  = max_size;
			int new_height = max_size;
			float const aspect = float(width)/float(height);
			if (aspect>1.0f) {
				//new_width = max_size;
				new_height = (int)(max_size/aspect);
			}
			else {
				new_width = (int) (max_size*aspect);
				//new_height = max_size;
			}

			//BL_ERR("resize:%dx%d -> %dx%d", width, height, new_width, new_height);
			uint8* const new_buffer = (uint8*) malloc(new_height*new_width*components);
			uint8* dst = new_buffer; // write pointer
			uint8 const* r0; // src row0
			uint8 const* r1; // src row1
			uint8 const* s0;
			uint8 const* s1;

			// scale factor (12-bits fixed point)
			int const scale_h = 4096*(height-1)/(new_height-1);
			int const scale_w = 4096*(width-1)/(new_width-1);

			int a, b, c;
			int w00, w10, w01, w11; // weights of 4-pixels box
			for (int i=0; i<new_height; ++i) {
				c = i*scale_h;
				a = (c & 0xfff)>>4;
				b = 255 - a;

				c >>= 12; // row
				r1 = r0 = buffer + (c*row_stride); // upper row
				if (c<(height-1)) {
					r1 += row_stride; // bottom row
				}

				for (int j=0; j<new_width; ++j) {
					c = j*scale_w;
					w00 = (c & 0xfff)>>4;
					w01 = 255 - w00;

					c >>= 12; // column
					s0 = r0 + (c*components); // upper-left
					s1 = r1 + (c*components); // bottom-left
					if (c<(width-1)) {
						w11 = a*w00;
						w10 = b*w00;
						w00 = b*w01;
						w01 *= a;
						for (int k=0; k<components; ++k) {
							c = (w00*s0[k] + w10*s0[k+components] + w01*s1[k] + w11*s1[k+components] + 32768)>>16;
							*dst++ = c<256 ? ((uint8)c):255;
						}
					}
					else {
						for (int k=0; k<components; ++k) {
							c = (b*s0[k] + a*s1[k] + 128)>>8;
							*dst++ = c<256 ? ((uint8)c):255;
						}
					}
				}
			}
			//BL_ERR("resize finish");

			free(buffer);
			width  = new_width;
			height = new_height;
			buffer = new_buffer;
		}
		return buffer;
	}
	return NULL;
}

//---------------------------------------------------------------------------------------
bool get_JPEG_Info(void const* jpg, int size, int& width, int& height, int& components)
{
	if (NULL!=jpg && size>16) {
		// Step 1: allocate and initialize JPEG decompression object
		// set up the normal JPEG error routines, override error_exit and output_message.
		jpeg_decompress_struct cinfo;
		my_error_mgr		   err_mgr;
		cinfo.err              = jpeg_std_error(&err_mgr);
		err_mgr.error_exit     = my_error_exit;
		err_mgr.output_message = my_output_message;

		// establish the setjmp return context for my_error_exit to use.
		if (setjmp(err_mgr.env)) {
			// if we get here, the JPEG code has signaled an error.
			// it needs to clean up the JPEG object, and return NULL.
			jpeg_destroy_decompress(&cinfo);
			return false;
		}

		// now it can initialize the JPEG decompression object
		jpeg_create_decompress(&cinfo);

		// step 2: specify data source:
		//         (jpeg_stdio_src(&cinfo, file) if from FILE*)
		jpeg_mem_src(&cinfo, (uint8*)jpg, size);

		// step 3: read file parameters with jpeg_read_header()
		(void) jpeg_read_header(&cinfo, TRUE);
		/* we can ignore the return value from jpeg_read_header since
         *   (a) suspension is not possible with the stdio data source, and
         *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
         * see libjpeg.txt for more info.
         */
		width  = cinfo.image_width;
		height = cinfo.image_height;
		components = cinfo.num_components;

		jpeg_destroy_decompress(&cinfo);

		return true;
	}
	return false;
}
//---------------------------------------------------------------------------------------
bool get_JPEG_Info(char const* filename, int& width, int& height, int& components)
{
	if (NULL!=filename) {
		FILE* file = fopen(filename, "rb");
		if (NULL!=file) {
			bool result = false;
			fseek(file, 0, SEEK_END);
			int const jpg_size = ftell(file);
			uint8* jpg = (uint8*) malloc(jpg_size);
			if (NULL!=jpg) {
				rewind(file);
				if (jpg_size==(int)fread(jpg, 1, jpg_size, file)) {
					result = get_JPEG_Info(jpg, jpg_size, width, height, components);
				}	
				free(jpg);
			}
			fclose(file);
			return result;
		}
	}
	return false;
}
//---------------------------------------------------------------------------------------
void* compress_JPEG_level(int& size, void const* data, int width, int height, int level, int components, int quality)
{
	if (NULL!=data && width>0 && width<65536 && 0<height && height<65536 &&
		(0<=level && level<=4) && (1==components || 3==components)) {
		jpeg_compress_struct cinfo;
		jpeg_error_mgr		 jerr;

		cinfo.err = jpeg_std_error(&jerr);
		jerr.output_message = my_output_message;

		jpeg_create_compress(&cinfo);

		uint8* dst_buffer = NULL;
		unsigned long dst_buffer_size = 0;
		jpeg_mem_dest(&cinfo, &dst_buffer, &dst_buffer_size);

		cinfo.image_width      = width; 	/* image width and height, in pixels */
		cinfo.image_height     = height;
		cinfo.input_components = components;		/* # of color components per pixel */
		cinfo.in_color_space   = (3==components) ? JCS_RGB:JCS_GRAYSCALE; 	/* colorspace of input image */

		jpeg_set_defaults(&cinfo);
		jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

		if (level>0) {
			// unsigned int scale_num, scale_denom
			// Scale the image by the fraction scale_num/scale_denom.  Default is
			// 1/1, or no scaling.  Currently, the supported scaling ratios are
			// M/N with all N from 1 to 16, where M is the destination DCT size,
			// which is 8 by default (see block_size parameter above).
			// (The library design allows for arbitrary scaling ratios but this
			// is not likely to be implemented any time soon.)
			cinfo.scale_num   = 1;
			cinfo.scale_denom = 1<<level;

			jpeg_calc_jpeg_dimensions(&cinfo);
			
			int block_size = cinfo.block_size;
			cinfo.block_size = block_size;
		}

		jpeg_start_compress(&cinfo, TRUE);

		//
		// you may check
		// cinfo.comp_info.downsampled_width and cinfo.comp_info.downsampled_height.
		//

		int const row_stride = width * components;	/* JSAMPLEs per row in image_buffer */
		uint8 const* pixels = (uint8 const*) data;
#ifdef JPEG_MULTIPLE_SCANLINES
		JSAMPROW* scanlines = new JSAMPROW[height];
		for (int i=0; i<height; ++i) {
			scanlines[i] = (JSAMPROW) (pixels + i*row_stride);
		}
		while (cinfo.next_scanline<cinfo.image_height) {
			jpeg_write_scanlines(&cinfo, scanlines+cinfo.next_scanline, cinfo.image_height-cinfo.next_scanline);
		}
		delete[] scanlines;
#else
		while (cinfo.next_scanline<cinfo.image_height) {
			JSAMPROW pRow = (uint8*) pixels + (cinfo.next_scanline*row_stride);
			jpeg_write_scanlines(&cinfo, &pRow, 1);
		}
#endif

		jpeg_finish_compress(&cinfo);
		jpeg_destroy_compress(&cinfo);

		size = (int) dst_buffer_size;

		// head must start from 0xFFD8 (SOI)
		if (size<16 || 0xFF!=dst_buffer[0] || 0xD8!=dst_buffer[1]) {
			size = 0;
			free(dst_buffer);
			dst_buffer = NULL;
		}

		// caller should free the pointer
		return dst_buffer;
	}
	return NULL;
}
//---------------------------------------------------------------------------------------
void* compress_JPEG(int& size, void const* data, int width, int height, int components, int quality) {
	return compress_JPEG_level(size, data, width, height, 0, components, quality);
}
//---------------------------------------------------------------------------------------
void* read_JPEG(char const* name, int& w, int& h, int& c, int max_size)
{
	if (NULL!=name) {
		FILE* file = fopen(name, "rb");
		if (file) {
			fseek(file, 0, SEEK_END);
			int size = ftell(file);
			if (size>16) {
				uint8* data = (uint8*) malloc(size);
				rewind(file);
				fread(data, 1, size, file);
				fclose(file);
				void* pixels = decompress_JPEG(data, size, w, h, c, max_size);
				free(data);
				return pixels;
			}
			fclose(file);
		}
	}
	return NULL;
}
//---------------------------------------------------------------------------------------
bool write_JPEG(char const* name, void const* pixels, int w, int h, int c, int quality)
{
	//
	// the better approach is to use jpeg_stdio_dest(jpeg_compress_struct*, FILE*),
	// so it won't have to decompress in memory!
	//
	bool result = false;
	if (NULL!=name && NULL!=pixels && 0<w && 0<h) {	
		int size = 0;
		void* jpg = compress_JPEG(size, pixels, w, h, c, quality);
		if (NULL!=jpg) {
			if (size>0) {
				FILE* file = fopen(name, "wb");
				if (NULL!=file) {
					result = (size==(int)fwrite(jpg, 1, size, file));
					fclose(file);
				}
			}
			free(jpg);
		}
	}
	return result;
}

}}} // mlabs::balai::image