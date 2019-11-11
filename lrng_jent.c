// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/*
 * LRNG Fast Noise Source: Jitter RNG
 *
 * Copyright (C) 2016 - 2019, Stephan Mueller <smueller@chronox.de>
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ALL OF
 * WHICH ARE HEREBY DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF NOT ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "lrng_internal.h"

/*
 * Estimated entropy of data is a 16th of LRNG_DRNG_SECURITY_STRENGTH_BITS.
 * Albeit a full entropy assessment is provided for the noise source indicating
 * that it provides high entropy rates and considering that it deactivates
 * when it detects insufficient hardware, the chosen under estimation of
 * entropy is considered to be acceptable to all reviewers.
 */
static u32 jitterrng = LRNG_DRNG_SECURITY_STRENGTH_BITS>>4;
module_param(jitterrng, uint, 0644);
MODULE_PARM_DESC(jitterrng, "Entropy in bits of 256 data bits from Jitter "
			    "RNG noise source");

/**
 * Get Jitter RNG entropy
 *
 * @outbuf buffer to store entropy
 * @outbuflen length of buffer
 * @return > 0 on success where value provides the added entropy in bits
 *	   0 if no fast source was available
 */
struct rand_data;
struct rand_data *jent_lrng_entropy_collector(void);
int jent_read_entropy(struct rand_data *ec, unsigned char *data,
		      unsigned int len);
static struct rand_data *lrng_jent_state;

u32 lrng_get_jent(u8 *outbuf, unsigned int outbuflen)
{
	int ret;
	u32 ent_bits = jitterrng;
	unsigned long flags;
	static DEFINE_SPINLOCK(lrng_jent_lock);
	static int lrng_jent_initialized = 0;

	spin_lock_irqsave(&lrng_jent_lock, flags);

	if (!ent_bits || (lrng_jent_initialized == -1)) {
		spin_unlock_irqrestore(&lrng_jent_lock, flags);
		return 0;
	}

	if (!lrng_jent_initialized) {
		lrng_jent_state = jent_lrng_entropy_collector();
		if (!lrng_jent_state) {
			jitterrng = 0;
			lrng_jent_initialized = -1;
			spin_unlock_irqrestore(&lrng_jent_lock, flags);
			pr_info("Jitter RNG unusable on current system\n");
			return 0;
		}
		lrng_jent_initialized = 1;
		pr_debug("Jitter RNG working on current system\n");
	}
	ret = jent_read_entropy(lrng_jent_state, outbuf, outbuflen);
	spin_unlock_irqrestore(&lrng_jent_lock, flags);

	if (ret) {
		pr_debug("Jitter RNG failed with %d\n", ret);
		return 0;
	}

	/* Obtain entropy statement */
	if (outbuflen != LRNG_DRNG_SECURITY_STRENGTH_BYTES)
		ent_bits = (ent_bits * outbuflen<<3) /
			   LRNG_DRNG_SECURITY_STRENGTH_BITS;
	/* Cap entropy to buffer size in bits */
	ent_bits = min_t(u32, ent_bits, outbuflen<<3);
	pr_debug("obtained %u bits of entropy from Jitter RNG noise source\n",
		 ent_bits);

	return ent_bits;
}

u32 lrng_jent_entropylevel(void)
{
	return min_t(u32, jitterrng, LRNG_DRNG_SECURITY_STRENGTH_BITS);
}