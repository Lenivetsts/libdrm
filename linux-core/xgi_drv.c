/* xgi_drv.c -- XGI driver -*- linux-c -*-
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include "drmP.h"
#include "xgi_drm.h"
#include "xgi_drv.h"

#include "drm_pciids.h"

static struct pci_device_id pciidlist[] = {
	xgi_PCI_IDS
};

static int xgi_driver_load(drm_device_t *dev, unsigned long chipset)
{
	drm_xgi_private_t *dev_priv;
	int ret;

	dev_priv = drm_calloc(1, sizeof(drm_xgi_private_t), DRM_MEM_DRIVER);
	if (dev_priv == NULL)
		return DRM_ERR(ENOMEM);

	dev->dev_private = (void *)dev_priv;
	dev_priv->chipset = chipset;
	ret = drm_sman_init(&dev_priv->sman, 2, 12, 8);
	if (ret) {
		drm_free(dev_priv, sizeof(dev_priv), DRM_MEM_DRIVER);
	}

	return ret;
}

static int xgi_driver_unload(drm_device_t *dev)
{
	drm_xgi_private_t *dev_priv = dev->dev_private;

	drm_sman_takedown(&dev_priv->sman);
	drm_free(dev_priv, sizeof(*dev_priv), DRM_MEM_DRIVER);

	return 0;
}



static int probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static struct drm_driver driver = {
	.driver_features = DRIVER_USE_AGP | DRIVER_USE_MTRR,
	.load = xgi_driver_load,
	.unload = xgi_driver_unload,
	.reclaim_buffers = NULL,
	.reclaim_buffers_idlelocked = xgi_reclaim_buffers_locked,
	.lastclose = xgi_lastclose,
	.get_map_ofs = drm_core_get_map_ofs,
	.get_reg_ofs = drm_core_get_reg_ofs,
	.ioctls = xgi_ioctls,
	.fops = {
		.owner = THIS_MODULE,
		.open = drm_open,
		.release = drm_release,
		.ioctl = drm_ioctl,
		.mmap = drm_mmap,
		.poll = drm_poll,
		.fasync = drm_fasync,
	},
	.pci_driver = {
		.name          = DRIVER_NAME,
		.id_table      = pciidlist,
		.probe = probe,
		.remove = __devexit_p(drm_cleanup_pci),
	 },
	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,
};

static int probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	return drm_get_dev(pdev, ent, &driver);
}

static int __init xgi_init(void)
{
	driver.num_ioctls = xgi_max_ioctl;
	return drm_init(&driver, pciidlist);
}

static void __exit xgi_exit(void)
{
	drm_exit(&driver);
}

module_init(xgi_init);
module_exit(xgi_exit);

MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL and additional rights");