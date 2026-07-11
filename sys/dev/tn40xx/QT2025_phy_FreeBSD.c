/*
 * Copyright (c) 2019 Tehuti Networks Ltd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "tn40.h"

void QT2025_register_settings(struct bdx_priv *priv);
int  QT2025_setMedia			(struct bdx_priv *priv, int ifm_media);
//-------------------------------------------------------------------------------------------------

int QT2025_setMedia(struct bdx_priv *priv, int ifm_media)
{
    tn40_priv_t		*tn40_priv 	= CONTAINING_RECORD(priv, tn40_priv_t, bdx_priv);
    if_t		ifp 		= tn40_priv->ifp;

	ERR("%s Does not support ifconfig media option\n", if_name(ifp));

	return EPERM;

} // QT2025_setMedia()

//-------------------------------------------------------------------------------------------------

__init void QT2025_register_settings(struct bdx_priv *priv)
{
	tn40_priv_t		*tn40_priv 	= CONTAINING_RECORD(priv, tn40_priv_t, bdx_priv);
	struct ifmedia	*ifm 		= &tn40_priv->media;

    priv->autoneg = AUTONEG_ENABLE;
    ifmedia_add(ifm, IFM_ETHER | IFM_10G_LR | IFM_FDX,  0, NULL);
    ifmedia_add(ifm, IFM_ETHER | IFM_10G_SR | IFM_FDX,  0, NULL);
    ifmedia_set(ifm, IFM_ETHER | IFM_10G_LR | IFM_FDX);

    priv->phy_ops.set_settings = QT2025_setMedia;

} // QT2025_register_settings()

