/*
 * Copyright (C) 2007 Ben Skeggs.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "drmP.h"
#include "drm.h"
#include "nouveau_drv.h"

int
nouveau_notifier_init_channel(drm_device_t *dev, int channel, DRMFILE filp)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_fifo *chan = &dev_priv->fifos[channel];
	int flags, ret;

	/*TODO: PCI notifier blocks */
	if (dev_priv->agp_heap)
		flags = NOUVEAU_MEM_AGP | NOUVEAU_MEM_FB_ACCEPTABLE;
	else
		flags = NOUVEAU_MEM_FB;

	chan->notifier_block = nouveau_mem_alloc(dev, 0, PAGE_SIZE, flags,filp);
	if (!chan->notifier_block)
		return DRM_ERR(ENOMEM);

	ret = nouveau_mem_init_heap(&chan->notifier_heap,
				    0, chan->notifier_block->size);
	if (ret)
		return ret;

	return 0;
}

void
nouveau_notifier_takedown_channel(drm_device_t *dev, int channel)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_fifo *chan = &dev_priv->fifos[channel];

	if (chan->notifier_block) {
		nouveau_mem_free(dev, chan->notifier_block);
		chan->notifier_block = NULL;
	}

	/*XXX: heap destroy */
}

int
nouveau_notifier_alloc(drm_device_t *dev, int channel, uint32_t handle,
		       int count, uint32_t *b_offset)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_fifo *chan = &dev_priv->fifos[channel];
	struct nouveau_object *obj;
	struct mem_block *mem;
	uint32_t offset;
	int target;

	if (!chan->notifier_heap) {
		DRM_ERROR("Channel %d doesn't have a notifier heap!\n",
			  channel);
		return DRM_ERR(EINVAL);
	}

	mem = nouveau_mem_alloc_block(chan->notifier_heap, 32, 0, chan->filp);
	if (!mem) {
		DRM_ERROR("Channel %d notifier block full\n", channel);
		return DRM_ERR(ENOMEM);
	}
	mem->flags = NOUVEAU_MEM_NOTIFIER;

	offset = chan->notifier_block->start + mem->start;
	if (chan->notifier_block->flags & NOUVEAU_MEM_FB) {
		offset -= drm_get_resource_start(dev, 1);
		target = NV_DMA_TARGET_VIDMEM;
	} else if (chan->notifier_block->flags & NOUVEAU_MEM_AGP) {
		offset -= dev_priv->agp_phys;
		target = NV_DMA_TARGET_AGP;
	} else {
		DRM_ERROR("Bad DMA target, flags 0x%08x!\n",
			  chan->notifier_block->flags);
		return DRM_ERR(EINVAL);
	}

	obj = nouveau_object_dma_create(dev, channel, NV_CLASS_DMA_IN_MEMORY,
					offset, mem->size, NV_DMA_ACCESS_RW,
					target);
	if (!obj) {
		nouveau_mem_free_block(mem);
		DRM_ERROR("Error creating notifier ctxdma\n");
		return DRM_ERR(ENOMEM);
	}

	obj->handle = handle;
	if (nouveau_ht_object_insert(dev, channel, handle, obj)) {
		nouveau_object_free(dev, obj);
		nouveau_mem_free_block(mem);
		DRM_ERROR("Error inserting notifier ctxdma into RAMHT\n");
		return DRM_ERR(ENOMEM);
	}

	*b_offset = mem->start;
	return 0;
}

int
nouveau_ioctl_notifier_alloc(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_nouveau_notifier_alloc_t na;
	int ret;

	DRM_COPY_FROM_USER_IOCTL(na, (drm_nouveau_notifier_alloc_t __user*)data,
				 sizeof(na));

	if (!nouveau_fifo_owner(dev, filp, na.channel)) {
		DRM_ERROR("pid %d doesn't own channel %d\n",
			  DRM_CURRENTPID, na.channel);
		return DRM_ERR(EPERM);
	}

	ret = nouveau_notifier_alloc(dev, na.channel, na.handle,
				     na.count, &na.offset);
	if (ret)
		return ret;

	DRM_COPY_TO_USER_IOCTL((drm_nouveau_notifier_alloc_t __user*)data,
			       na, sizeof(na));
	return 0;
}
