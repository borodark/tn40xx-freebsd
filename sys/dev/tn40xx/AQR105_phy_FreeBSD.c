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

void AQR105_register_settings(struct bdx_priv *priv);
int  AQR105_setMedia			(struct bdx_priv *priv, int ifm_media);
//-------------------------------------------------------------------------------------------------

int AQR105_setMedia(struct bdx_priv *priv, int ifm_media)
{
	int rVal, speed = -1;

	if (ifm_media == 0)
	{
		speed = 0;
		priv->autoneg = AUTONEG_ENABLE;
	}
	else
	{
		priv->autoneg = AUTONEG_DISABLE;
		switch (ifm_media)
		{
			case IFM_10G_T:
				speed = 10000;
				break;

			case IFM_5000_T:
				speed = 5000;
				break;

			case IFM_2500_T:
				speed = 2500;
				break;

			case IFM_1000_T:
				speed = 1000;
				break;

			case IFM_100_T:
				speed = 100;
				break;

			default:
				ERR("Unsupported mediad 0x%x\n", ifm_media);
				break;
		}
	}
	if (speed >= 0)
	{
		rVal =  AQR105_set_speed(priv, speed);
	}
	else
	{
		rVal = -1;
	}
	return rVal;

} // AQR105_setMedia()

//-------------------------------------------------------------------------------------------------

__init void AQR105_register_settings(struct bdx_priv *priv)
{
	tn40_priv_t		*tn40_priv 	= CONTAINING_RECORD(priv, tn40_priv_t, bdx_priv);
	struct ifmedia	*ifm 		= &tn40_priv->media;

    priv->autoneg = AUTONEG_ENABLE;
    ifmedia_add(ifm, IFM_ETHER | IFM_AUTO,  			0, NULL);
    ifmedia_add(ifm, IFM_ETHER | IFM_10G_T  | IFM_FDX,  0, NULL);
    ifmedia_add(ifm, IFM_ETHER | IFM_5000_T | IFM_FDX,  0, NULL);
    ifmedia_add(ifm, IFM_ETHER | IFM_2500_T | IFM_FDX,  0, NULL);
    ifmedia_add(ifm, IFM_ETHER | IFM_1000_T | IFM_FDX,  0, NULL);
    ifmedia_add(ifm, IFM_ETHER | IFM_100_T  | IFM_FDX,  0, NULL);
    ifmedia_set(ifm, IFM_ETHER | IFM_AUTO);

    priv->phy_ops.set_settings = AQR105_setMedia;

} // AQR105_register_settings()

//-------------------------------------------------------------------------------------------------

