/* 
 * MIT license
 *
 * Copyright (c) 2017 SFG(Star-Formation Group), Physics Dept., CUHK.
 * All rights reserved.
 *
 * file    SFGPVMParser.cpp
 * author  andre chen, andre.hl.chen@gmail.com
 * history 2017/10/25 created
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "SFGPVMParser.h"

#define DDS_BLOCKSIZE (1<<20)
#define DDS_INTERLEAVE (1<<24)

#define DDS_RL (7)

unsigned short int DDS_INTEL = 1;
#define DDS_ISINTEL (*((uint8*)(&DDS_INTEL)+1)==0)

// helper functions for DDS:
inline uint32 DDS_shiftl(uint32 value, uint32 bits) { return (bits>=32) ? 0:(value<<bits); }
inline uint32 DDS_shiftr(uint32 value, uint32 bits) { return (bits>=32) ? 0:(value>>bits); }
inline uint32 DDS_swapuint(uint32 x) { return ((x&0xff)<<24)| ((x&0xff00)<<8)| ((x&0xff0000)>>8)| ((x&0xff000000)>>24); }

class DDS_BitReader {
    mutable uint32 const* begin_;
    uint32 const* end_;
    uint32 cache_;
    uint32 cache_bits_;

    virtual uint32 read_uint32_() const {
        uint32 v = 0;
        if (begin_<end_) {
            v = *begin_++;

            if (DDS_ISINTEL) {
                v = DDS_swapuint(v);
            }
        }
        return v;
    }

public:
    DDS_BitReader(uint32 const* begin, uint32 const* end):
        begin_(begin),end_(end),cache_(0),cache_bits_(0) {}

    uint32 readbits(uint32 bits) {
        uint32 value = 0;
        if (bits<cache_bits_) {
            cache_bits_ -= bits;
            value = DDS_shiftr(cache_, cache_bits_);
        }
        else {
            value = DDS_shiftl(cache_, bits-cache_bits_);

            cache_ = read_uint32_();

            cache_bits_ += 32 - bits;
            value |= DDS_shiftr(cache_, cache_bits_);
        }

        cache_ &= (DDS_shiftl(1, cache_bits_)-1);

        return value;
    }
};

// deinterleave a byte stream
bool DDS_deinterleave(uint8* data, uint32 bytes, uint32 skip, uint32 block, bool restore)
{
    if (skip>1) {
        uint32 i, j, k;

        uint8 *data2, *ptr;
        if (block==0) {
            if ((data2=(uint8*)malloc(bytes))==NULL) {
                return false;
            }

            if (!restore) {
                for (ptr=data2,i=0; i<skip; ++i) {
                    for (j=i; j<bytes; j+=skip) {
                        *ptr++ = data[j];
                    }
                }
            }
            else {
                for (ptr=data,i=0; i<skip; ++i) {
                    for (j=i; j<bytes; j+=skip) {
                        data2[j] = *ptr++;
                    }
                }
            }
            memcpy(data, data2, bytes);
        }
        else {
            if ((data2=(uint8*)malloc((bytes<skip*block) ? bytes:(skip*block)))==NULL) {
                return false;
            }

            if (!restore) {
                for (k=0; k<bytes/skip/block; ++k) {
                    for (ptr=data2,i=0; i<skip; ++i) {
                        for (j=i; j<skip*block; j+=skip) {
                            *ptr++ = data[k*skip*block+j];
                        }
                    }
                    memcpy(data+k*skip*block, data2, skip*block);
                }

                for (ptr=data2,i=0; i<skip; ++i) {
                    for (j=i; j<bytes-k*skip*block; j+=skip) {
                        *ptr++ = data[k*skip*block+j];
                    }
                }
                memcpy(data+k*skip*block, data2, bytes-k*skip*block);
            }
            else {
                for (k=0; k<bytes/skip/block; ++k) {
                    for (ptr=data+k*skip*block,i=0; i<skip; ++i) {
                        for (j=i; j<skip*block; j+=skip) {
                            data2[j]=*ptr++;
                        }
                    }

                    memcpy(data+k*skip*block, data2, skip*block);
                }

                for (ptr=data+k*skip*block,i=0; i<skip; ++i) {
                    for (j=i; j<bytes-k*skip*block; j+=skip) {
                        data2[j] = *ptr++;
                    }
                }

                memcpy(data+k*skip*block, data2, bytes-k*skip*block);
            }
        }

        free(data2);
    }
    return true;
}

namespace sfg {

//---------------------------------------------------------------------------------------
inline void* read_file(int64& size, char const* filename) {
    if (NULL!=filename) {
        FILE* file = fopen(filename, "rb");
        if (file) {
            _fseeki64(file, 0, SEEK_END);
            int64 const file_size = _ftelli64(file);
            rewind(file);
            void* buffer = NULL;
            if (file_size>0) {
                buffer = malloc(file_size);
                if (buffer) {
                    if (file_size==fread(buffer, 1, file_size, file)) {
                        size = file_size;
                    }
                    else {
                        free(buffer);
                        buffer = NULL;
                    }
                }
            }
            fclose(file);
            return buffer;
        }
    }

    return NULL;
}

bool PVMParser::Load(char const* filename)
{
    int64 pvm_size = 0;
    void* pvm = read_file(pvm_size, filename); // must be 16-bit aligned
    if (pvm) {
        char* header = (char*) pvm;
        int is_dds = 0;
        if (pvm_size>8 &&
            (0==memcmp("DDS v3d\n", header, 8) || 0==memcmp("DDS v3e\n", header, 8))) {
            unsigned int const block = ('d'==header[6]) ? 0:DDS_INTERLEAVE; 
            memmove(pvm, header+8, pvm_size-=8);
            memset(header+pvm_size, 0, 8);
            DDS_BitReader DDS((uint32*) header,
                              (uint32*) (header+((pvm_size+3)&~3))); // round up

            uint32 const skip = DDS.readbits(2) + 1;
            uint32 const strip = DDS.readbits(16) + 1;

            uint8* ptr1 = NULL;
            uint8* ptr2 = NULL;
            uint32 cnt(0), cnt1(0);
            int bits(0), act(0);
            while ((cnt1=DDS.readbits(DDS_RL))!=0) {
                bits = DDS.readbits(3);
                if (bits>0)
                    ++bits;

                for (uint32 cnt2=0; cnt2<cnt1; ++cnt2) {
                    if (strip==1 || cnt<=strip)
                        act += DDS.readbits(bits)-(1<<bits)/2;
                    else
                        act += *(ptr2-strip) - *(ptr2-strip-1) + DDS.readbits(bits)-(1<<bits)/2;

                    while (act<0)
                        act += 256;
                    while (act>255)
                        act -= 256;

                    if ((cnt&(DDS_BLOCKSIZE-1))==0) {
                        if (ptr1==NULL) {
                            if ((ptr1=(uint8*) malloc(DDS_BLOCKSIZE))==NULL) {
                                // error handling
                            }

                            ptr2 = ptr1;
                        }
                        else {
                            if ((ptr1=(uint8*) realloc(ptr1, cnt+DDS_BLOCKSIZE))==NULL) {
                                // error handling
                            }
                            ptr2 = ptr1 + cnt;
                        }
                    }

                    *ptr2++ = act;

                    ++cnt;
                }
            }

            // deinterleave a byte stream
            if (skip>1) {
                DDS_deinterleave(ptr1, cnt, skip, block, true);
            }

            free(pvm);
            if (ptr1) {
                pvm = ptr1;
                pvm_size = cnt;
                is_dds = 1;
                ptr1 = NULL;
            }
            else {
                pvm = NULL;
                pvm_size = 0;
            }

            // header
            header = (char*) pvm;
        }

        int version = 0;
        if (pvm_size>8 && 0==memcmp("PVM", header, 3)) {
            //
            // PVM : The header is 3 ascii text lines consisting of
            // 1) "PVM" on the first line,
            // 2) the width, height, and depth on the second line and,
            // 3) the number of components (bytes) per voxel on the third line.
            // The data then follows as binary data.
            //
            // Notes:
            //  1) The number of bytes in the binary data will be width*height*depth*components.
            //  2) The three lines of ascii header as a single string will be null '\0' terminated,
            //     so one extra byte after the last '\n'. <<--- may not be true.
            //  3) The voxels are assumed to be cubic.
            //
            // PVM2 is the same as PVM except there is an extra ascii line between the dimensions and the number of components.
            // This line has three numbers indicating the relative size of the voxels, that is, providing support for non-square voxels.
            //
            // PMV3 is the same as PVM2 except that after the 4th line of the PVM2 header and before the raw data there are 4 null terminated strings.
            // These can of course be used for anything but they are notionally intended for "description", "courtesy", "parameters", and "comment".
            // The maximum allowed length for each of these is 256 bytes.
            //

            char* ptr = header;
            char* const end = ptr + pvm_size;
            if ('\n'==header[3]) {
                version = 1; // "PVM\n"
                ptr = header + 4;
            }
            else if ('\n'==header[4] && ('2'==header[3] || '3'==header[3])) { // "PVM2\n" or "PVM3\n"
                version = header[3] - '0'; // note we didn't overwrite header[3]
                ptr = header + 5;
            }

            if (version) {
                if ('\0'==*ptr)
                    ++ptr;

                // width, height, and depth
                int cx(0), cy(0), cz(0);
                if (3==sscanf(ptr,"%d %d %d\n", &cx, &cy, &cz) && cx>0 && cy>0 && cz>0) {
                    while (ptr<end && *ptr!='\n')
                        ++ptr;

                    if ('\0'==*++ptr)
                        ++ptr;

                    // scaling
                    float sx(1.0f), sy(1.0f), sz(1.0f);
                    if (1<version) {
                        if (3!=sscanf(ptr,"%f %f %f\n", &sx, &sy, &sz) ||
                            sx<=0.0f && sy<=0.0f && sz<=0.0f) {
                            sx = sy = sz = 1.0f;
                        }

                        while (ptr<end && *ptr!='\n')
                            ++ptr;

                        if ('\0'==*++ptr)
                            ++ptr;
                    }
                    else {
                        // ???
                        while (*ptr=='#')
                            while (*ptr++!='\n');
                    }

                    // number of componenets
                    int components = 0;
                    if (1==sscanf(ptr, "%d\n", &components) && components>0) {
                        while (ptr<end && *ptr!='\n')
                            ++ptr;
#if 0
                        ///////////////////////////////////////////////////////////////////
                        // check this out.... it may not have one null character here!?
                        if ('\0'==*++ptr)
                            ++ptr;
                        ///////////////////////////////////////////////////////////////////
#else
                        ++ptr;
#endif
                        int const voxel_size = components*cx*cy*cz;
                        if ((ptr+voxel_size)<end) {
                            uint8* voxels = (uint8*) ptr;
                            ptr += voxel_size;
                            char const* description = NULL;
                            char const* courtesy = NULL;
                            char const* parameters = NULL;
                            char const* comment = NULL;
                            if (3==version && ptr<end && *ptr!='\0') {
                                //
                                // 4 null terminated strings in the end
                                //

                                // description
                                char const* s = ptr;
                                while (ptr<end && *ptr!='\0')
                                    ++ptr;

                                if (ptr<end) {
                                    description = s;

                                    // courtesy
                                    s = ++ptr;
                                    while (ptr<end && *ptr!='\0')
                                        ++ptr;

                                    if (ptr<end) {
                                        courtesy = s;

                                        // parameters
                                        s = ++ptr;
                                        while (ptr<end && *ptr!='\0')
                                            ++ptr;

                                        if (ptr<end) {
                                            parameters = s;

                                            // comment
                                            s = ++ptr;
                                            while (ptr<end && *ptr!='\0')
                                                ++ptr;

                                            if (ptr<end) {
                                                comment = s;
                                            }
                                        }
                                    }
                                }
                            }

                            ReadVoxels_(voxels, cx, cy, cz, components,
                                        sx, sy, sz,
                                        description, courtesy, parameters, comment);
                        }
                    }
                }
            }
        }

        free(pvm);
        return (0<version);
    }

    return false;
}

//---------------------------------------------------------------------------------------
bool PVMParser::ReadVoxels_(float* dst, float& inf, float& sup,
                            uint8 const* src, int cx, int cy, int cz, int components)
{
    if (dst && src && cx>0 && cy>0 && cz>0 && components>0) {
        int const pixels = cx*cy*cz;
        float t;
        if (1==components) {
            float const scale = 1.0f/255.0f;
            inf = sup = scale*(*src++);
            for (int i=1; i<pixels; ++i) {
                *dst++ = t = scale*(*src++);
                if (t<inf)
                    inf = t;
                else if (t>sup)
                    sup = t;
            }
        }
        else {
            if (2!=components) {
                // warning...
            }

            float const scale = 1.0f/65535.0f;
            src += (components - 2);
            inf = sup = scale*(256*src[1] + src[0]); src+=components;
            for (int i=1; i<pixels; ++i,src+=components) {
                *dst++ = t = scale*(256*src[1] + src[0]);
                if (t<inf)
                    inf = t;
                else if (t>sup)
                    sup = t;
            }
        }

        return true;
    }

    return false;
}
//---------------------------------------------------------------------------------------
int PVMParser::IsPVMFile(char const* filename)
{
    FILE* file = fopen(filename, "rb");
    if (file) {
        char buffer[8];
        size_t const read_bytes = fread(buffer, 1, 8, file);
        fclose(file);

        if (8==read_bytes) {
            if (0==memcmp("PVM\n", buffer, 4)) {
                return 1;
            }
            else if (0==memcmp("PVM2\n", buffer, 5)) {
                return 2;
            }
            else if (0==memcmp("PVM3\n", buffer, 5)) {
                return 3;
            }
            else if (0==memcmp("DDS v3d\n", buffer, 8)) {
                return 4;
            }
            else if (0==memcmp("DDS v3e\n", buffer, 8)) {
                return 5;
            }
        }
    }

    return 0;
}

}