/*
 * Copyright (C) 2017 Kernkonzept GmbH.
 * Author(s): Philipp Eppelt <philipp.eppelt@kernekonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public License,
 * version 2. Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include "irq.h"
#include "virtio_dev.h"

namespace Virtio {

/**
 * This IRQ event connector supports a single IRQ for all events.
 *
 * In general, the event connector connects the events issued by a
 * virtio device with the interrupt configured in the transport layer.
 * The connector makes the virtio device independent from the transport
 * specific event notification facility, e.g. Interrupt or MSI.
 */
class Event_connector_irq
{
public:
  /**
   * Commit / send events marked in `ev` to the guest.
   *
   * \param ev  Set of pending events to be injected into the guest.
   */
  void send_events(Virtio::Event_set &&ev)
  {
    if (ev.e)
      _sink.inject();

    ev.reset();
  }

  /// Send a single event with index `idx` to the guest.
  void send_event(l4_uint16_t idx)
  {
    (void)idx;
    _sink.inject();
  }

  /**
   * Acknowledge the bits set in the bit mask.
   *
   * \param irq_ack_mask  Describes the config/queue events to acknowledge.
   */
  void clear_events(unsigned irq_ack_mask)
  {
    (void)irq_ack_mask;
    _sink.ack();
  }

  /**
   * Line-based IRQ setup routine for device-tree setup.
   */
  int init_irqs(Vdev::Device_lookup *devs, Vdev::Dt_node const &node)
  {
    cxx::Ref_ptr<Gic::Ic> ic = devs->get_or_create_ic_dev(node, false);
    if (!ic)
      return -1;

    int propsz;
    auto *irq_prop = node.get_prop<fdt32_t>("interrupts", &propsz);

    int irq = ic->dt_get_interrupt(irq_prop, propsz, nullptr);

    if (irq < 0)
      return -1;

    _sink.rebind(ic.get(), irq);
    return 0;
  }

private:
  Vmm::Irq_sink _sink;
};

} // namespace Virtio
