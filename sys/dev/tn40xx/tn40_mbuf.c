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
#include "tn40_mbuf.h"

#define TN40_CHECK_MBUF_COPY

//-------------------------------------------------------------------------------------------------

static int tn40_mbuf_find(struct mbuf *mBuf, int count, struct mbuf **mHead, struct mbuf **mLast)
{
	int j, l, len, k = 0, coLen=mBuf->m_pkthdr.len;

	struct mbuf *m, *m1;

	for (m = mBuf; m; m=m->m_next)
	{
		len 	= 0;
		m1 		= m->m_next;			// Skip the header
		for (j = 0; j < count; j++)
		{
			len += m1->m_len;
			if (m1->m_next != NULL)
			{
				m1 	= m1->m_next;
			}
			else
			{
				break;
			}
		}
		if (j >= count - 1)
		{
			if (len < coLen)
			{
				coLen  = len;
				*mHead = m;				// The one *BEFORE* the first
				*mLast = m1;			// The last one;
				l	   = k;				// size
			}
		}
		else
		{
			break;
		}
		k += 1;
	}
	DBG("Found (%p) at %d size %d bytes %d last %p\n", *mHead, l, count, coLen, *mLast);

	return coLen;

} // tn40_mbuf_find()

//-------------------------------------------------------------------------------------------------

static int tn40_mbuf_copy(struct mbuf *fromBuf, struct mbuf *toBuf, int nCopy)
{
	int 		len, fLen, tLen;
	caddr_t 	from, to;
	struct mbuf *m;
	int 		j 		= 0;
	int			nBytes 	= 0;
	ENTER;

	DBG("copy from %p to %p\n", fromBuf, toBuf);
	from = fromBuf->m_data;
	to	 = toBuf->m_data;
	fLen = fromBuf->m_len;
	tLen = toBuf->m_len;
	for (; ; )
	{
		len  = min(fLen, tLen);
#ifdef TN40_CHECK_MBUF_COPY
		if ((from == NULL) || (to == NULL))
		{
			ERR("tn40_mbuf_copy() internal error from %p to %p\n", from, to);
			return 0;
		}
		if (from +len > fromBuf->m_data + fromBuf->m_len)
		{
			ERR("tn40_mbuf_copy() internal error from %p + %d > %p + %d\n", from, len, fromBuf->m_data, fromBuf->m_len);
			return 0;

		}
		if (to +len > toBuf->m_data + toBuf->m_len)
		{
			ERR("tn40_mbuf_copy() internal error to %p + %d > %p + %d\n", to, len, toBuf->m_data, toBuf->m_len);
			return 0;

		}
#endif
		memcpy(to, from, len);
		nBytes += len;
		fLen -= len;
		tLen -= len;
		if (fLen == 0)
		{
			j += 1;
			m  = fromBuf;
			DBG("Copied from %p len %d\n", m, m->m_len);
			if (j == nCopy)
			{
				m_free(m);
				break;
			}
			fromBuf = fromBuf->m_next;
			fLen 	= fromBuf->m_len;
			from 	= fromBuf->m_data;
			m_free(m);
		}
		else
		{
			from 	+= len;
		}
		if (tLen == 0)
		{
			toBuf 	= toBuf->m_next;
			tLen 	= toBuf->m_len;
			to 		= toBuf->m_data;
		}
		else
		{
			to	+= len;
		}
	}
	toBuf->m_len  -= tLen;
	toBuf->m_next = fromBuf->m_next;		// Link the new mBuf head
	DBG("linked new mbuf tail %p to %p\n", toBuf, fromBuf->m_next);

	return nBytes;

} // tn40_mbuf_copy()

//-------------------------------------------------------------------------------------------------

int tn40_mbuf_shrink(struct mbuf *mBuf, int nr_frags)
{
	int			coLen, nAllocs, j;
	struct mbuf *mHead, *mLast, *m, *m1;
	struct mbuf *newBuf 	= NULL;
	int			mBufSize	= 0;
	int			nBytes		= 0;
	int			err			= 0;
	int 		nCopy 		= nr_frags - MAX_PBL +1;
	int			nBufs 		= MAX_PBL;	// for the first iteration

	if (nr_frags > MAX_PBL)
	{
		for (m = mBuf; m; m = m->m_next) mBufSize += 1;
		// Find the start mbuf and the number of the new mbufs to allocate
		do
		{
			nCopy 	+= nBufs - MAX_PBL;
			if (nCopy >= mBufSize)				// this Mbuf cannot be shriked
			{
				nBufs = mBufSize;
				break;
			}
			coLen	= tn40_mbuf_find(mBuf, nCopy, &mHead, &mLast);
			nAllocs = (coLen + NEW_MBUF_SIZE) / NEW_MBUF_SIZE;
			nBufs 	= mBufSize - nCopy + nAllocs;
		} while (nBufs > MAX_PBL);
		// Allocate new mbufs
		if (nBufs <= MAX_PBL)
		{
			for (j = 0; j < nAllocs; j++)
			{
				m = m_getjcl(M_NOWAIT, MT_DATA, 0, NEW_MBUF_SIZE);
				if (m != NULL)
				{
					m->m_len = NEW_MBUF_SIZE;
					if (newBuf == NULL)
					{
						newBuf = m1 = m;
					}
					else
					{
						m1->m_next = m;
						m1 = m;
					}
				}
				else
				{
					if (newBuf)
					{
						m_freem(newBuf);
						newBuf 	= NULL;
						err 	= ENOBUFS; // cannot allocate new mBuf
						break;
					}
				}
			}
			DBG("New mbuf %p count %d\n", newBuf, nAllocs);
		}
		else
		{
			err = E2BIG;	// mBuf too long
		}
		if ((err == 0) && (newBuf != NULL))
		{
			nBytes = tn40_mbuf_copy(mHead->m_next, newBuf, nCopy);
			mHead->m_next = newBuf;				// Link the new mBuf
			DBG("link %p to new mbuf %p\n", mHead, newBuf);
		}
		DBG("m %p len %d frags %d size %d -> %d copied %d \n", mBuf, m_length(mBuf, &m), nr_frags, mBufSize, nBufs, nBytes);
	}

    RET(err);

}// tn40_mbuf_shrink()



