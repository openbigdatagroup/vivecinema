/* 
 * MIT license
 *
 * Copyright (c) 2017 SFG(Star-Formation Group), Physics Dept., CUHK.
 * All rights reserved.
 *
 * file    SFGDataset.cpp
 * author  andre chen, andre.hl.chen@gmail.com
 * history 2017/10/03 created
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
#ifndef SFG_DATASET_H
#define SFG_DATASET_H

#include "BLCore.h"

namespace sfg {

enum DATATYPE {
    DATATYPE_UNKNOWN = 0,
    DATATYPE_INT8,
    DATATYPE_UINT8,
    DATATYPE_INT16,
    DATATYPE_UINT16,
    DATATYPE_INT32,
    DATATYPE_UINT32,
    DATATYPE_INT64,
    DATATYPE_UINT64,
    DATATYPE_FLOAT,
    DATATYPE_DOUBLE,
    DATATYPE_QUAD,   // 16-bytes floating point

    DATATYPE_TOTALS  // the last one
};

inline int GetDataSize(DATATYPE type) {
    int const tbl[DATATYPE_TOTALS] = {
        -1, 1, 1, 2, 2, 4, 4, 8, 8, 4, 8, 16
    };
    return (type<DATATYPE_TOTALS) ? tbl[type] : -1;
}

inline char const* GetTypeString(DATATYPE type) {
    char const* tbl[DATATYPE_TOTALS] = {
        "UNKNOWN",
        "INT8",
        "UNIT8",
        "INT16",
        "UINT16",
        "INT32",
        "UINT32",
        "INT64",
        "UINT64",
        "FLOAT",
        "DOUBLE",
        "QUAD"
    };
    return (type<DATATYPE_TOTALS) ? tbl[type] : "Error!!!";
}

// Serialization of HD5F dataset, use as has-a or has-many. not intended to be inherited.
class Dataset
{
    char     name_[32];
    int      dimensions_[32];
    void*    data_;
    size_t   size_; // data alloc size
    size_t   elements_;
    int      rank_;
    DATATYPE type_;

public:
    Dataset():data_(NULL),size_(0),elements_(0),
        rank_(0),type_(DATATYPE_UNKNOWN) {
        memset(name_, 0, sizeof(name_));
        memset(dimensions_, 0, sizeof(dimensions_));
    }
    ~Dataset() { Free(); }

    // accessors
    char const* Name() const { return name_; }
    int const* Dimensions() const { return dimensions_; }
    void const* DataPtr() const { return data_; }
    void* DataPtr() { return data_; }
    size_t DataSize() const { return size_; }
    size_t NumElements() const { return elements_; }
    int Rank() const { return rank_; }
    DATATYPE Type() const { return type_; }

    bool SetProperties(char const* name, DATATYPE type, int const* dimensions, int rank) {
        if (DATATYPE_UNKNOWN<type && type<DATATYPE_TOTALS && dimensions && 0<rank && (rank<sizeof(dimensions_)/sizeof(dimensions_[0]))) {
            if (name) {
                strncpy(name_, name, 31);
            }
            else {
                name_[0] = '\0';
            }

            elements_ = 1;
            for (int i=0; i<rank; ++i) {
                if (0<dimensions[i]) {
                    dimensions_[i] = dimensions[i];
                    elements_ *= dimensions[i];
                }
                else {
                    elements_ = 0;
                    return false;
                }
            }

            rank_ = rank;
            type_ = type;

            return true;
        }
        return false;
    }

    //
    // data pointer casting, code like:
    //  int const* pi = dset.CastPtr<int>();
    //  float const* pf = dset.CastPtr<float>();
    //
    template<typename T>
    T const* CastPtr() const { return NULL; }
    #define CAST_PTR(t, T) template <> t const* CastPtr() const { return (T==type_) ? ((t const*)data_):NULL; }
    CAST_PTR(int8, DATATYPE_INT8);
    CAST_PTR(uint8, DATATYPE_UINT8);
    CAST_PTR(int16, DATATYPE_INT16);
    CAST_PTR(uint16, DATATYPE_UINT16);
    CAST_PTR(int32, DATATYPE_INT32);
    CAST_PTR(uint32, DATATYPE_UINT32);
    CAST_PTR(int64, DATATYPE_INT64);
    CAST_PTR(uint64, DATATYPE_UINT64);
    CAST_PTR(float, DATATYPE_FLOAT);
    CAST_PTR(double, DATATYPE_DOUBLE);
    CAST_PTR(long double, DATATYPE_QUAD);

    // malloc and free
    void* Malloc(size_t sz, bool realloc=true) {
        if (realloc && data_ && size_>=sz)
            return data_;

        Free();

        data_ = malloc(sz);
        if (NULL!=data_) {
            size_ = sz;
            return data_;
        }
        return NULL;
    }
    void Free() {
        memset(name_, 0, sizeof(name_));
        memset(dimensions_, 0, sizeof(dimensions_));
        if (data_) {
            free(data_);
            data_ = NULL;
        }
        size_ = elements_ = rank_ = 0;
        type_ = DATATYPE_UNKNOWN;
    }
    void* Free(size_t& size) {
        void* data = NULL;
        if (data_) {
            data = data_;
            size = size_;
            data_ = NULL;
        }
        else {
            size = 0;
        }

        memset(name_, 0, sizeof(name_));
        memset(dimensions_, 0, sizeof(dimensions_));
        size_ = elements_ = rank_ = 0;
        type_ = DATATYPE_UNKNOWN;

        return data;
    }

    // debug string
    size_t DimensionString(char* str) const { // caller must be sure str[] array is bit enough.
        if (str) {
            if (0<rank_) {
                sprintf(str, "%ld", dimensions_[0]);
                for (int i=1; i<rank_; ++i) {
                    sprintf(str, "%sx%ld", str, dimensions_[i]);
                }
            }
            else {
                str[0] = '\0';
            }
        }
        return rank_;
    }

    char const* TypeString() const { return GetTypeString(type_); }
};

class ReadBuffer
{
    void*  buffer_;
    size_t buffer_size_;

protected:
    void* Malloc_(size_t& size) {
        if (size > 0) {
            if (size<buffer_size_||NULL==buffer_) {
                if (buffer_) {
                    free(buffer_);
                }
                buffer_ = malloc(size);
                if (buffer_) {
                    buffer_size_ = size;
                }
                else {
                    buffer_size_ = 0;
                }
            }
            else {
                size = buffer_size_;
            }
            return buffer_;
        }
        return NULL;
    }
    void Free_() {
        if (buffer_) {
            free(buffer_);
            buffer_ = NULL;
        }
        buffer_size_ = 0;
    }

public:
    ReadBuffer():buffer_(NULL),buffer_size_(0) {}
    virtual ~ReadBuffer() { Free_(); }
};

} // namespace sfg

#endif