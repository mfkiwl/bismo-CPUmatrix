// Copyright (c) 2018 Xilinx
//
// BSD v3 License
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of [project] nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <iostream>
#include <vector>
#include <cassert>
#include <time.h>
#include "platform.h"
#include "EmuTestExecInstrGenSingleMM.hpp"
#include "BISMOInstruction.hpp"
#include "InstrGen.hpp"
#include "StageModels.hpp"
#include "gemmbitserial/test/testhelpers.hpp"
#include "gemmbitserial/gemmbitserial.hpp"

using namespace std;

WrapperRegDriver * p;
EmuTestExecInstrGenSingleMM * t;


// generate BISMO matrix memory buffers from a bit serial matrix
// note that this is dependent on the size of the instantiated hardware
void matrix2mem(
  gemmbitserial::BitSerialMatrix & matrix, // bit serial matrix to pack
  size_t peCount,                          // number of PEs
  size_t peMemBasePtr,                     // PE memory packing start address
  size_t peMemSize,                        // number of entries in each PE mem
  StageModels::BitVector * mem             // pointer to PE memories: mem[peCount][peMemSize]
) {
  // ensure workload is aligned
  assert(matrix.nrows_a % peCount == 0);
  // ensure workload and model Dk match
  assert(sizeof(matrix.data[0]) == sizeof(StageModels::BitVector));
  // keep track of which position we are writing to for each PE memory
  vector<size_t> pe_mem_ptr(peCount, peMemBasePtr);
  // place each vector of bits into appropriate PE, interleaving rows between
  // PEs
  for(size_t b = 0; b < matrix.nbits; b++) {
    for(size_t r = 0; r < matrix.nrows_a; r++) {
      for(size_t c = 0; c < matrix.wordsPerRow(); c++) {
        size_t targetPE = r % peCount;  // interleave rows between PEs
        size_t flat_ind = targetPE * peMemSize + pe_mem_ptr[targetPE];
        mem[flat_ind] = matrix.word(b, r, c);
        // make sure we are still within bounds of the PE memory
        assert(pe_mem_ptr[targetPE] < peMemSize);
        pe_mem_ptr[targetPE]++;
      }
    }
  }
}

// copy a matrix tile into the given coordinate of a larger matrix buffer,
// optionally transposing it in the process
template <typename Dtype>
void resmemcpy2d(
  // source and destination buffers
  const Dtype * src, Dtype * dst,
  // size of source buffer, layout src[m][n]
  size_t src_m, size_t src_n,
  // size of destination buffer and start coords
  size_t dst_cols, size_t dst_row_start, size_t dst_col_start,
  bool do_transpose
) {
  size_t src_rows = do_transpose ? src_n : src_m;
  size_t src_cols = do_transpose ?  src_m : src_n;
  for(size_t src_row = 0; src_row < src_rows; src_row++) {
    for(size_t src_col = 0; src_col < src_cols; src_col++) {
      const size_t src_ind_rm = src_row * src_n + src_col;
      const size_t src_ind_cm = src_col * src_n + src_row;
      const size_t src_ind = do_transpose ? src_ind_cm : src_ind_rm;
      const size_t dst_row = dst_row_start + src_row;
      const size_t dst_col = dst_col_start + src_col;
      const size_t dst_ind = dst_row * dst_cols + dst_col;
      dst[dst_ind] = src[src_ind];
    }
  }
}


int main(int argc, char const *argv[]) {
  bool t_okay = true;
  try {
    cout << "EmuTestExecInstrGenSingleMM running" << endl;

    srand(time(NULL));

    // hardware dims for test
    const size_t Dm = 2, Dk = 4, Dn = 2;
    // define dimensions for the workload
    const size_t tiles_m = 1;
    const size_t tiles_k = 1;
    const size_t tiles_n = 1;
    const size_t bits_l = 2;
    const size_t bits_r = 3;
    const size_t base_l = 0;
    const size_t base_r = 0;
    const size_t base_res = 0;
    const size_t nbufs_res = tiles_m * tiles_n;
    const size_t nrows_lhs = Dm * tiles_m;
    const size_t nrows_rhs = Dn * tiles_n;
    const size_t ncols = Dk * tiles_k;
    const size_t mem_m = tiles_m * tiles_k * bits_l;
    const size_t mem_n = tiles_n * tiles_k * bits_r;
    const bool sgn_lhs = true;
    const bool sgn_rhs = true;

    // create a small random workload
    int8_t * lhs = new int8_t[nrows_lhs * ncols];
    int8_t * rhs = new int8_t[nrows_rhs * ncols];
    gemmbitserial::generateRandomVector(bits_l, nrows_lhs*ncols, lhs, sgn_lhs);
    gemmbitserial::generateRandomVector(bits_r, nrows_rhs*ncols, rhs, sgn_rhs);
    gemmbitserial::GEMMContext ctx = gemmbitserial::allocGEMMContext_base(
      nrows_lhs, ncols, nrows_rhs, bits_l, bits_r, sgn_lhs, sgn_rhs,
      Dm, 1, Dn, 1
    );
    ctx.lhs.importRegular(lhs);
    ctx.rhs.importRegular(rhs);
    // compute the golden result
    gemmbitserial::gemmBitSerial_generic_naive(ctx);
    gemmbitserial::printmatrix(lhs, nrows_lhs, ncols);
    cout << endl;
    gemmbitserial::printmatrix(rhs, nrows_rhs, ncols);
    cout << endl;
    gemmbitserial::printmatrix(ctx.res, nrows_lhs, nrows_rhs);

    // create instruction sequence for bit serial MM
    hls::stream<BISMOInstruction> instrs;
    InstrGen::SingleMMDescriptor dsc;
    dsc.tiles_m = tiles_m;
    dsc.tiles_k = tiles_k;
    dsc.tiles_n = tiles_n;
    dsc.bits_l = bits_l;
    dsc.bits_r = bits_r;
    dsc.signed_l = sgn_lhs;
    dsc.signed_r = sgn_rhs;
    dsc.base_l = base_l;
    dsc.base_r = base_r;
    dsc.base_res = base_res;
    dsc.nbufs_res = nbufs_res;
    InstrGen::ExecInstrGenSingleMM(dsc, instrs);

    // test generated instructions in software model
    StageModels::Accumulator * hw_acc = new StageModels::Accumulator[tiles_m*tiles_n];
    StageModels::Accumulator * hw_res = new StageModels::Accumulator[tiles_m*tiles_n*nbufs_res];
    StageModels::BitVector hw_lhs[Dm][mem_m] = {0};
    StageModels::BitVector hw_rhs[Dn][mem_n] = {0};
    const size_t hw_baseptr = 0;

    // fill memories and call the functional model with generated instructions
    matrix2mem(ctx.lhs, Dm, hw_baseptr, mem_m, (StageModels::BitVector *)hw_lhs);
    matrix2mem(ctx.rhs, Dn, hw_baseptr, mem_n, (StageModels::BitVector *)hw_rhs);
    StageModels::ExecMultiInstr<Dm, Dn, mem_m, mem_n, nbufs_res>(
      instrs, hw_lhs, hw_rhs, hw_acc, hw_res
    );

    StageModels::Accumulator * hw_res_full = new StageModels::Accumulator[nrows_rhs * ncols];
    // copy from accelerator result memory into full result matrix
    // TODO loop over multiple result tiles here
    resmemcpy2d(hw_res, hw_res_full, Dm, Dn, nrows_lhs, 0, 0, true);
    const size_t res_bytes = nrows_lhs * nrows_rhs * sizeof(StageModels::Accumulator);
    t_okay = (memcmp(hw_res_full, ctx.res, res_bytes) == 0);

    // TODO add comparison function to compare hw_res and ctx.res
    gemmbitserial::printmatrix(hw_res, nrows_lhs, nrows_rhs);
    gemmbitserial::printmatrix(hw_res_full, nrows_lhs, nrows_rhs);

    delete [] hw_acc;
    delete [] hw_res;
    delete [] hw_res_full;

    p = initPlatform();
    t = new EmuTestExecInstrGenSingleMM(p);

    for(unsigned int i = 0; i < 10; i++) {
      while(t->get_out_valid() != 1);
      t_okay &= t->get_out_bits_runcfg_lhsOffset() == i;
      t_okay &= t->get_out_bits_runcfg_rhsOffset() == 10 - i;
      t_okay &= t->get_out_bits_runcfg_numTiles() == 2 * i;
      t_okay &= t->get_out_bits_runcfg_shiftAmount() == i + 1;
      cout << "okay at " << i << " " << t_okay << endl;
      t->set_out_ready(0);
      t->set_out_ready(1);
    }

    delete t;
    deinitPlatform(p);
  } catch(const char * e) {
    cout << "Exception: " << e << endl;
  }

  cout << "Test passed: " << (t_okay ? "yes" : "no") << endl;

  return t_okay ? 0 : -1;
}