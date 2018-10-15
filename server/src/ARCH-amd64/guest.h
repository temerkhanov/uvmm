/*
 * Copyright (C) 2018 Kernkonzept GmbH.
 * Author(s): Sarah Hoffmann <sarah.hoffmann@kernkonzept.com>
 *            Philipp Eppelt <philipp.eppelt@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/util/cpu.h>
#include <l4/vbus/vbus>
#include <l4/l4virtio/l4virtio>

#include <map>
#include <vector>

#include "cpu_dev_array.h"
#include "generic_guest.h"
#include "io_device.h"
#include "msr_device.h"
#include "mem_access.h"
#include "timer.h"
#include "vcpu_ptr.h"
#include "virt_lapic.h"
#include "vmprint.h"
#include "zeropage.h"
#include "pt_walker.h"
#include "vm_ram.h"

namespace Vmm {

class Guest : public Generic_guest
{
public:
  enum { Default_rambase = 0, Boot_offset = 0 };

  Guest()
  : _ptw(&_memmap, get_max_physical_address_bit()),
    _apics(Vdev::make_device<Gic::Lapic_array>(get_max_physical_address_bit()))
  {
    add_mmio_device(_apics->mmio_region(), _apics);

    register_msr_device(_apics);
  }

  static Guest *create_instance();

  void setup_device_tree(Vdev::Device_tree) {}

  void show_state_interrupts(FILE *, Vcpu_ptr) {}

  void register_io_device(Io_region const &region,
                          cxx::Ref_ptr<Io_device> const &dev);

  void register_msr_device(cxx::Ref_ptr<Msr_device> const &dev);

  void register_timer_device(cxx::Ref_ptr<Vdev::Timer> const &dev)
  {
    _clock.add_timer(dev);
  }

  l4_addr_t load_linux_kernel(Vm_ram *ram, char const *kernel,
                              Ram_free_list *free_list);

  void prepare_linux_run(Vcpu_ptr vcpu, l4_addr_t entry, Vm_ram *ram,
                         char const *kernel, char const *cmd_line,
                         l4_addr_t dt_boot_addr);

  void run(cxx::Ref_ptr<Cpu_dev_array> const &cpus) L4_NORETURN;

  void handle_entry(Vcpu_ptr vcpu);

  Gic::Virt_lapic *lapic(Vcpu_ptr vcpu)
  { return _apics->get(vcpu.get_vcpu_id()).get(); }

  cxx::Ref_ptr<Gic::Lapic_array> apic_array() { return _apics; }

  int handle_cpuid(l4_vcpu_regs_t *regs);
  int handle_vm_call(l4_vcpu_regs_t *regs);
  int handle_io_access(unsigned port, bool is_in, Mem_access::Width op_width,
                       l4_vcpu_regs_t *regs);

private:
  // guest physical address
  enum : unsigned
  {
    Linux_kernel_start_addr = 0x100000,
    Max_phys_addr_bits_mask = 0xff,
  };

  void run_vmx(cxx::Ref_ptr<Cpu_dev> const &cpu_dev) L4_NORETURN;
  int handle_exit_vmx(Vcpu_ptr vcpu);

  unsigned get_max_physical_address_bit() const
  {
    l4_umword_t ax, bx, cx, dx;
    // check for highest CPUID leaf:
    l4util_cpu_cpuid(0, &ax, &bx, &cx, &dx);

    if (ax == 0x80000008)
      l4util_cpu_cpuid(0x80000008, &ax, &bx, &cx, &dx);
    else
      {
        l4util_cpu_cpuid(0x1, &ax, &bx, &cx, &dx);
        if (dx & (1UL << 6)) // PAE
          ax = 36;           // minimum if leaf not supported
        else
          ax = 32;
      }

    return ax & Max_phys_addr_bits_mask;
  }

  bool msr_devices_rwmsr(l4_vcpu_regs_t *regs, bool write, unsigned vcpu_no);

  typedef std::map<Io_region, cxx::Ref_ptr<Io_device>> Io_mem;
  Io_mem _iomap;

  std::vector<cxx::Ref_ptr<Msr_device>> _msr_devices;

  // devices
  Vdev::Clock_source _clock;
  Guest_print_buffer _hypcall_print;
  Pt_walker _ptw;
  cxx::Ref_ptr<Gic::Lapic_array> _apics;
  Binary_type _guest_t;
};

/**
 * Handler for MSR read/write to a specific vCPU with its corresponding
 * VM state.
 */
class Vcpu_msr_handler : public Msr_device
{
public:
  Vcpu_msr_handler(Cpu_dev_array *cpus) : _cpus(cpus) {};

  bool read_msr(unsigned msr, l4_uint64_t *value, unsigned vcpu_no) override
  {
    return _cpus->vcpu(vcpu_no).vm_state()->read_msr(msr, value);
  }

  bool write_msr(unsigned msr, l4_uint64_t value, unsigned vcpu_no) override
  {
    return _cpus->vcpu(vcpu_no).vm_state()->write_msr(msr, value);
  }

private:
  Cpu_dev_array *_cpus;
};

} // namespace Vmm
