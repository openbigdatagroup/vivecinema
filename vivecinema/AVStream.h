/*
 * Copyright (C) 2018 HTC Corporation
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
 * @file    AVStream.h
 * @author  andre chen, andre.HL.chen@gmail.com
 * @history 2018/03/22 created, take out from AVDecoder.h
 *
 */
#ifndef AV_CUSTOMIZED_STREAM_H
#define AV_CUSTOMIZED_STREAM_H

#include "BLCore.h"
#include <mutex>

namespace mlabs { namespace balai { namespace video {

// customized input stream (via AVIOContext)
class IInputStream
{
    mutable std::mutex mutex_;
    volatile int refCount_;

protected:
    IInputStream():mutex_(),refCount_(1) {}
    virtual ~IInputStream() { assert(refCount_<=0); }

public:
    int RefCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return refCount_;
    }
    int AddRef() {
        std::lock_guard<std::mutex> lock(mutex_);
        BL_ASSERT(refCount_>0); // you see dead people?
        return ++refCount_; 
    }
    int Release() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            BL_ASSERT(refCount_>0); // you really did?
            if (--refCount_>0)
                return refCount_;
        } // must unlock before kill this->mutex_
        delete this; // kill self
        return 0;
    }

    // name
    virtual char const* Name() const { return "InputStream"; }

    // url
    virtual char const* URL() const = 0;

    // ready to read from start, will be called when stream is opened.
    virtual bool Rewind() = 0; 

    // read data and return number of bytes read.
    virtual int Read(uint8_t* buf, int buf_size) = 0;

    // like fseek()/_fseeki64(), whence = SEEK_SET(mostly), SEEK_CUR or SEEK_END
    // but unlike fseek()/_fseeki64(), it return stream position.
    virtual int64_t Seek(int64_t offset, int whence) = 0;

    // return stream length.
    virtual int64_t Size() = 0;
};


}}}
#endif