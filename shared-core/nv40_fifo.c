/*
 * Copyright (C) 2007 Ben Skeggs.
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
#include "nouveau_drv.h"
#include "nouveau_drm.h"

#define RAMFC_WR(offset, val)	NV_WRITE(fifoctx + NV40_RAMFC_##offset, (val))
#define RAMFC_RD(offset)	NV_READ (fifoctx + NV40_RAMFC_##offset)

int
nv40_fifo_create_context(drm_device_t *dev, int channel)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_fifo *chan = &dev_priv->fifos[channel];
	uint32_t fifoctx, grctx, pushbuf;
	int i;

	fifoctx = NV_RAMIN + dev_priv->ramfc_offset + channel*128;
	for (i=0;i<128;i+=4)
		NV_WRITE(fifoctx + i, 0);

	grctx   = nouveau_chip_instance_get(dev, chan->ramin_grctx);
	pushbuf = nouveau_chip_instance_get(dev, chan->cmdbuf_obj->instance);

	/* Fill entries that are seen filled in dumps of nvidia driver just
	 * after channel's is put into DMA mode
	 */
	RAMFC_WR(DMA_PUT       , chan->pushbuf_base);
	RAMFC_WR(DMA_GET       , chan->pushbuf_base);
	RAMFC_WR(DMA_INSTANCE  , pushbuf);
	RAMFC_WR(DMA_FETCH     , NV_PFIFO_CACHE1_DMA_FETCH_TRIG_128_BYTES |
				 NV_PFIFO_CACHE1_DMA_FETCH_SIZE_128_BYTES |
				 NV_PFIFO_CACHE1_DMA_FETCH_MAX_REQS_8 |
#ifdef __BIG_ENDIAN
				 NV_PFIFO_CACHE1_BIG_ENDIAN |
#endif
				 0x30000000 /* no idea.. */);
	RAMFC_WR(DMA_SUBROUTINE, 0);
	RAMFC_WR(GRCTX_INSTANCE, grctx);
	RAMFC_WR(DMA_TIMESLICE , 0x0001FFFF);

	return 0;
}

void
nv40_fifo_destroy_context(drm_device_t *dev, int channel)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	uint32_t fifoctx;
	int i;

	fifoctx = NV_RAMIN + dev_priv->ramfc_offset + channel*128;
	for (i=0;i<128;i+=4)
		NV_WRITE(fifoctx + i, 0);
}

int
nv40_fifo_load_context(drm_device_t *dev, int channel)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	uint32_t fifoctx;
	uint32_t tmp, tmp2;

	fifoctx = NV_RAMIN + dev_priv->ramfc_offset + channel*128;

	NV_WRITE(NV04_PFIFO_CACHE1_DMA_GET          , RAMFC_RD(DMA_GET));
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_PUT          , RAMFC_RD(DMA_PUT));
	NV_WRITE(NV10_PFIFO_CACHE1_REF_CNT          , RAMFC_RD(REF_CNT));
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_INSTANCE     , RAMFC_RD(DMA_INSTANCE));
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_DCOUNT       , RAMFC_RD(DMA_DCOUNT));
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_STATE        , RAMFC_RD(DMA_STATE));

	/* No idea what 0x2058 is.. */
	tmp   = RAMFC_RD(DMA_FETCH);
	tmp2  = NV_READ(0x2058) & 0xFFF;
	tmp2 |= (tmp & 0x30000000);
	NV_WRITE(0x2058, tmp2);
	tmp  &= ~0x30000000;
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_FETCH        , tmp);

	NV_WRITE(NV04_PFIFO_CACHE1_ENGINE           , RAMFC_RD(ENGINE));
	NV_WRITE(NV04_PFIFO_CACHE1_PULL1            , RAMFC_RD(PULL1_ENGINE));
	NV_WRITE(NV10_PFIFO_CACHE1_ACQUIRE_VALUE    , RAMFC_RD(ACQUIRE_VALUE));
	NV_WRITE(NV10_PFIFO_CACHE1_ACQUIRE_TIMESTAMP, RAMFC_RD(ACQUIRE_TIMESTAMP));
	NV_WRITE(NV10_PFIFO_CACHE1_ACQUIRE_TIMEOUT  , RAMFC_RD(ACQUIRE_TIMEOUT));
	NV_WRITE(NV10_PFIFO_CACHE1_SEMAPHORE        , RAMFC_RD(SEMAPHORE));
	NV_WRITE(NV10_PFIFO_CACHE1_DMA_SUBROUTINE   , RAMFC_RD(DMA_SUBROUTINE));
	NV_WRITE(NV40_PFIFO_GRCTX_INSTANCE          , RAMFC_RD(GRCTX_INSTANCE));
	NV_WRITE(0x32e4, RAMFC_RD(UNK_40));
	/* NVIDIA does this next line twice... */
	NV_WRITE(0x32e8, RAMFC_RD(UNK_44));
	NV_WRITE(0x2088, RAMFC_RD(UNK_4C));
	NV_WRITE(0x3300, RAMFC_RD(UNK_50));

	/* not sure what part is PUT, and which is GET.. never seen a non-zero
	 * value appear in a mmio-trace yet..
	 */
#if 0
	tmp = NV_READ(UNK_84);
	NV_WRITE(NV_PFIFO_CACHE1_GET, tmp ???);
	NV_WRITE(NV_PFIFO_CACHE1_PUT, tmp ???);
#endif

	/* Don't clobber the TIMEOUT_ENABLED flag when restoring from RAMFC */
	tmp  = NV_READ(NV04_PFIFO_DMA_TIMESLICE) & ~0x1FFFF;
	tmp |= RAMFC_RD(DMA_TIMESLICE) & 0x1FFFF;
	NV_WRITE(NV04_PFIFO_DMA_TIMESLICE, tmp);

	/* Set channel active, and in DMA mode */
	NV_WRITE(NV03_PFIFO_CACHE1_PUSH1  , 0x00010000 | channel);
	/* Reset DMA_CTL_AT_INFO to INVALID */
	tmp = NV_READ(NV04_PFIFO_CACHE1_DMA_CTL) & ~(1<<31);
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_CTL, tmp);

	return 0;
}

int
nv40_fifo_save_context(drm_device_t *dev, int channel)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	uint32_t fifoctx;
	uint32_t tmp;

	fifoctx = NV_RAMIN + dev_priv->ramfc_offset + channel*128;

	RAMFC_WR(DMA_PUT          , NV_READ(NV04_PFIFO_CACHE1_DMA_PUT));
	RAMFC_WR(DMA_GET          , NV_READ(NV04_PFIFO_CACHE1_DMA_GET));
	RAMFC_WR(REF_CNT          , NV_READ(NV10_PFIFO_CACHE1_REF_CNT));
	RAMFC_WR(DMA_INSTANCE     , NV_READ(NV04_PFIFO_CACHE1_DMA_INSTANCE));
	RAMFC_WR(DMA_DCOUNT       , NV_READ(NV04_PFIFO_CACHE1_DMA_DCOUNT));
	RAMFC_WR(DMA_STATE        , NV_READ(NV04_PFIFO_CACHE1_DMA_STATE));

	tmp  = NV_READ(NV04_PFIFO_CACHE1_DMA_FETCH);
	tmp |= NV_READ(0x2058) & 0x30000000;
	RAMFC_WR(DMA_FETCH	  , tmp);

	RAMFC_WR(ENGINE           , NV_READ(NV04_PFIFO_CACHE1_ENGINE));
	RAMFC_WR(PULL1_ENGINE     , NV_READ(NV04_PFIFO_CACHE1_PULL1));
	RAMFC_WR(ACQUIRE_VALUE    , NV_READ(NV10_PFIFO_CACHE1_ACQUIRE_VALUE));
	tmp = NV_READ(NV10_PFIFO_CACHE1_ACQUIRE_TIMESTAMP);
	RAMFC_WR(ACQUIRE_TIMESTAMP, tmp);
	RAMFC_WR(ACQUIRE_TIMEOUT  , NV_READ(NV10_PFIFO_CACHE1_ACQUIRE_TIMEOUT));
	RAMFC_WR(SEMAPHORE        , NV_READ(NV10_PFIFO_CACHE1_SEMAPHORE));

	/* NVIDIA read 0x3228 first, then write DMA_GET here.. maybe something
	 * more involved depending on the value of 0x3228?
	 */
	RAMFC_WR(DMA_SUBROUTINE   , NV_READ(NV04_PFIFO_CACHE1_DMA_GET));

	RAMFC_WR(GRCTX_INSTANCE   , NV_READ(NV40_PFIFO_GRCTX_INSTANCE));

	/* No idea what the below is for exactly, ripped from a mmio-trace */
	RAMFC_WR(UNK_40           , NV_READ(NV40_PFIFO_UNK32E4));

	/* NVIDIA do this next line twice.. bug? */
	RAMFC_WR(UNK_44           , NV_READ(0x32e8));
	RAMFC_WR(UNK_4C           , NV_READ(0x2088));
	RAMFC_WR(UNK_50           , NV_READ(0x3300));

#if 0 /* no real idea which is PUT/GET in UNK_48.. */
	tmp  = NV_READ(NV04_PFIFO_CACHE1_GET);
	tmp |= (NV_READ(NV04_PFIFO_CACHE1_PUT) << 16);
	RAMFC_WR(UNK_48           , tmp);
#endif

	return 0;
}
