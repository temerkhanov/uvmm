/*
 * Copyright (C) 2018 Kernkonzept GmbH.
 * Author(s): Sarah Hoffmann <sarah.hoffmann@kernkonzept.com>
 *            Alexander Warg <alexander.warg@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */

#include "debug.h"
#include "vm_memmap.h"

static void
throw_error(char const *msg,
            cxx::Ref_ptr<Vmm::Mmio_device> const &dev, Region const &region,
            cxx::Ref_ptr<Vmm::Mmio_device> const &new_dev, Region const &new_region)
{
  char buf[80], buf_new[80];
  Err().printf("%s: [%lx:%lx] (%s) <-> [%lx:%lx] (%s)\n", msg,
               region.start, region.end, dev->dev_info(buf, sizeof(buf)),
               new_region.start, new_region.end,
               new_dev->dev_info(buf_new, sizeof(buf_new)));
  L4Re::chksys(-L4_EINVAL, msg);
}

void
Vm_mem::add_mmio_device(Region const &region,
                        cxx::Ref_ptr<Vmm::Mmio_device> const &dev)
{
  if (count(region) == 0)
    {
      insert(std::make_pair(region, dev));
      return;
    }

  auto lower = lower_bound(region);
  auto const &current_region = lower->first;
  if (current_region.contains(region))
    {
      // Region is a subset of an already existing one, there can be
      // at most one such region
      if (!lower->second->mergable(dev, region.start, current_region.start))
        throw_error("Unmergable mmio regions: region is subset",
                    lower->second, current_region, dev, region);
      return;
    }

  auto upper = upper_bound(region);
  for(auto it = lower; it != upper; ++it)
    {
      auto const &tmp_region = it->first;
      // We already handled smaller regions above, so the region is
      // either a superset or not a candidate for a merge operation
      if (region.contains(tmp_region))
        {
          if (!it->second->mergable(dev, region.start, tmp_region.start))
            throw_error("Unmergable mmio regions: region is superset",
                        lower->second, tmp_region, dev, region);
        }
      else
        throw_error("Unmergable mmio regions",
                    lower->second, tmp_region, dev, region);
    }

  // [lower, upper) is a subset of region - erase it
  erase(lower, upper);
  insert(std::make_pair(region, dev));
}

