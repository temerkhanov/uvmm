/*
 * Copyright (C) 2019 Kernkonzept GmbH.
 * Author(s): Christian Pötzsch <christian.poetzsch@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include "smccc_device.h"
#include "vmprint.h"

namespace {

static Dbg warn(Dbg::Dev, Dbg::Warn, "vm_print");
static Dbg info(Dbg::Dev, Dbg::Info, "vm_print");

class Vm_print_device : public Vdev::Device, public Vmm::Smccc_device
{
  enum Vm_print_error_codes
  {
    Success = 0,
  };

public:
  bool vm_call(Vmm::Vcpu_ptr vcpu) override
  {
    l4_mword_t imm = vcpu->r.err & 0xffff;
    // Check this is imm 1
    if (imm != 1)
      return false;

    if (!is_valid_func_id(vcpu->r.r[0]))
      return false;

    _guest_print.print_char(vcpu->r.r[1]);
    vcpu->r.r[0] = Success;

    return true;
  }

private:
  bool is_valid_func_id(l4_umword_t reg) const
  {
    // Check for the correct SMC calling convention:
    // - this must be a fast call (bit 31)
    // - it is within the uvmm range (bits 29:24)
    // - the rest must be zero
    return (reg & 0xbfffffff) == 0x86000000;
  }

  Vmm::Guest_print_buffer _guest_print;
};

}