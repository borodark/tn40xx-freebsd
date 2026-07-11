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
#ifndef _TN40XX_IOCTL_H
#define _TN40XX_IOCTL_H

/* ioctl ops */

enum
{
    OP_INFO,
	OP_READ_REG,
	OP_WRITE_REG,
	OP_MDIO_READ,
	OP_MDIO_WRITE,
	op_TRACE_ON,
	op_TRACE_OFF,
	op_TRACE_ONCE,
	op_TRACE_PRINT,
	OP_DBG,
	OP_MEMLOG_DMESG,
	OP_MEMLOG_PRINT,
};
enum
{
	DBG_STOP_DBG			= 0,
	DBG_START_DBG			= 1,
	DBG_SUSPEND 			= 2,
	DBG_RESUME  			= 3,
	DBG_PRINT_PAGE_TABLE 	= 4,
	DBG_PRINT_COUNTERS 		= 5,
	DBG_CLEAR_COUNTERS 		= 6,
};

#define IOCTL_DATA_SIZE		(3)

typedef struct _tn40_ioctl_
{
	unsigned int data[IOCTL_DATA_SIZE];
	char		*buf;
} tn40_ioctl_t;

#endif /* _TN40XX_IOCTL_H */

