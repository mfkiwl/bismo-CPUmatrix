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

package bismo

import Chisel._

object TargetStages {
  val stgFetch :: stgExec :: stgResult :: Nil = Enum(UInt(), 3)
}

class BISMOInstruction extends Bundle {
  // which stage this instruction is targeting (TargetStages)
  val targetStage = UInt(width = 2)
  // run stage if true, sync otherwise
  val isRunCfg = Bool()
  // rest of instruction data, fill up 128 bits
  val instrData = UInt(width = 128-3)

  override def cloneType: this.type =
    new BISMOInstruction().asInstanceOf[this.type]
}

class BISMOSyncInstruction extends Bundle {
  // which stage this instruction is targeting (TargetStages)
  val targetStage = UInt(width = 2)
  // always false since this is a sync instruction
  val isRunCfg = Bool()
  // rest of instruction data
  val instrData = new Bundle {
    // send token if true, receive if false
    val iSendToken = Bool()
    // channel number for token sync
    val chanID = UInt(width = 2)
    // rest of instruction data, fill up 128 bits
    val unused = UInt(width = 125 - 3)
  }

  override def cloneType: this.type =
    new BISMOSyncInstruction().asInstanceOf[this.type]
}

class BISMOFetchRunInstruction extends Bundle {
  // always stgFetch
  val targetStage = UInt(width = 2)
  // always true
  val isRunCfg = Bool()
  // rest of instruction data
  val instrData = new Bundle {
    val runcfg = new FetchStageCtrlIO()
    // rest of instruction data, fill up 128 bits
    val unused = UInt(width = 125 - runcfg.getWidth())
  }

  override def cloneType: this.type =
    new BISMOFetchRunInstruction().asInstanceOf[this.type]
}

class BISMOExecRunInstruction extends Bundle {
  // always stgFetch
  val targetStage = UInt(width = 2)
  // always true
  val isRunCfg = Bool()
  // rest of instruction data
  val instrData = new Bundle {
    val runcfg = new ExecStageCtrlIO()
    // rest of instruction data, fill up 128 bits
    val unused = UInt(width = 125 - runcfg.getWidth())
  }

  override def cloneType: this.type =
    new BISMOExecRunInstruction().asInstanceOf[this.type]
}

class BISMOResultRunInstruction extends Bundle {
  // always stgFetch
  val targetStage = UInt(width = 2)
  // always true
  val isRunCfg = Bool()
  // rest of instruction data
  val instrData = new Bundle {
    val runcfg = new ResultStageCtrlIO()
    // rest of instruction data, fill up 128 bits
    val unused = UInt(width = 125 - runcfg.getWidth())
  }

  override def cloneType: this.type =
    new BISMOResultRunInstruction().asInstanceOf[this.type]
}