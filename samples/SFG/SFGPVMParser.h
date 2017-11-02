/* 
 * MIT license
 *
 * Copyright (c) 2017 SFG(Star-Formation Group), Physics Dept., CUHK.
 * All rights reserved.
 *
 * file    SFGPVMParser.h
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
#ifndef SFG_PVM_PARSER_H
#define SFG_PVM_PARSER_H

#include "SFGDataset.h"

namespace sfg {

class PVMParser
{
    virtual void ReadVoxels_(uint8 const* voxels,
                             int cx, int cy, int cz, int components,
                             float sx, float sy, float sz, // voxel size in x, y & z
                             char const* description,
                             char const* courtesy,
                             char const* parameters,
                             char const* comment) = 0;

protected:
    bool ReadVoxels_(float* dst, float& inf, float& sup,
                     uint8 const* src, int cx, int cy, int cz, int components);

public:
    PVMParser() {}
    virtual ~PVMParser() {}

    // ascii file only
    bool Load(char const* filename);

    //
    // return PVM file with type
    // 1: "PVM"
    // 2: "PVM2"
    // 3: "PVM3"
    // 4: "DDS v3d" - DDS no interleaved
    // 5: "DDS v3e" - DDS interleaved
    int IsPVMFile(char const* filename);
};

} // namespace sfg

#endif