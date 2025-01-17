/*
 *  Global object built-ins
 */

#include "duk_internal.h"

/*
 *  Encoding/decoding helpers
 */

/* Macros for creating and checking bitmasks for character encoding.
 * Bit number is a bit counterintuitive, but minimizes code size.
 */
#define DUK__MKBITS(a,b,c,d,e,f,g,h)  ((duk_uint8_t) ( \
	((a) << 0) | ((b) << 1) | ((c) << 2) | ((d) << 3) | \
	((e) << 4) | ((f) << 5) | ((g) << 6) | ((h) << 7) \
	))
#define DUK__CHECK_BITMASK(table,cp)  ((table)[(cp) >> 3] & (1 << ((cp) & 0x07)))

/* E5.1 Section 15.1.3.3: uriReserved + uriUnescaped + '#' */
DUK_LOCAL duk_uint8_t duk__encode_uriunescaped_table[16] = {
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0),  /* 0x00-0x0f */
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0),  /* 0x10-0x1f */
	DUK__MKBITS(0, 1, 0, 1, 1, 0, 1, 1), DUK__MKBITS(1, 1, 1, 1, 1, 1, 1, 1),  /* 0x20-0x2f */
	DUK__MKBITS(1, 1, 1, 1, 1, 1, 1, 1), DUK__MKBITS(1, 1, 1, 1, 0, 1, 0, 1),  /* 0x30-0x3f */
	DUK__MKBITS(1, 1, 1, 1, 1, 1, 1, 1), DUK__MKBITS(1, 1, 1, 1, 1, 1, 1, 1),  /* 0x40-0x4f */
	DUK__MKBITS(1, 1, 1, 1, 1, 1, 1, 1), DUK__MKBITS(1, 1, 1, 0, 0, 0, 0, 1),  /* 0x50-0x5f */
	DUK__MKBITS(0, 1, 1, 1, 1, 1, 1, 1), DUK__MKBITS(1, 1, 1, 1, 1, 1, 1, 1),  /* 0x60-0x6f */
	DUK__MKBITS(1, 1, 1, 1, 1, 1, 1, 1), DUK__MKBITS(1, 1, 1, 0, 0, 0, 1, 0),  /* 0x70-0x7f */
};

/* E5.1 Section 15.1.3.4: uriUnescaped */
DUK_LOCAL duk_uint8_t duk__encode_uricomponent_unescaped_table[16] = {
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0),  /* 0x00-0x0f */
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0),  /* 0x10-0x1f */
	DUK__MKBITS(0, 1, 0, 0, 0, 0, 0, 1), DUK__MKBITS(1, 1, 1, 0, 0, 1, 1, 0),  /* 0x20-0x2f */
	DUK__MKBITS(1, 1, 1, 1, 1, 1, 1, 1), DUK__MKBITS(1, 1, 0, 0, 0, 0, 0, 0),  /* 0x30-0x3f */
	DUK__MKBITS(0, 1, 1, 1, 1, 1, 1, 1), DUK__MKBITS(1, 1, 1, 1, 1, 1, 1, 1),  /* 0x40-0x4f */
	DUK__MKBITS(1, 1, 1, 1, 1, 1, 1, 1), DUK__MKBITS(1, 1, 1, 0, 0, 0, 0, 1),  /* 0x50-0x5f */
	DUK__MKBITS(0, 1, 1, 1, 1, 1, 1, 1), DUK__MKBITS(1, 1, 1, 1, 1, 1, 1, 1),  /* 0x60-0x6f */
	DUK__MKBITS(1, 1, 1, 1, 1, 1, 1, 1), DUK__MKBITS(1, 1, 1, 0, 0, 0, 1, 0),  /* 0x70-0x7f */
};

/* E5.1 Section 15.1.3.1: uriReserved + '#' */
DUK_LOCAL duk_uint8_t duk__decode_uri_reserved_table[16] = {
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0),  /* 0x00-0x0f */
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0),  /* 0x10-0x1f */
	DUK__MKBITS(0, 0, 0, 1, 1, 0, 1, 0), DUK__MKBITS(0, 0, 0, 1, 1, 0, 0, 1),  /* 0x20-0x2f */
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 1, 1, 0, 1, 0, 1),  /* 0x30-0x3f */
	DUK__MKBITS(1, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0),  /* 0x40-0x4f */
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0),  /* 0x50-0x5f */
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0),  /* 0x60-0x6f */
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0),  /* 0x70-0x7f */
};

/* E5.1 Section 15.1.3.2: empty */
DUK_LOCAL duk_uint8_t duk__decode_uri_component_reserved_table[16] = {
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0),  /* 0x00-0x0f */
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0),  /* 0x10-0x1f */
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0),  /* 0x20-0x2f */
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0),  /* 0x30-0x3f */
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0),  /* 0x40-0x4f */
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0),  /* 0x50-0x5f */
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0),  /* 0x60-0x6f */
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0),  /* 0x70-0x7f */
};

#ifdef DUK_USE_SECTION_B
/* E5.1 Section B.2.2, step 7. */
DUK_LOCAL duk_uint8_t duk__escape_unescaped_table[16] = {
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0),  /* 0x00-0x0f */
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0),  /* 0x10-0x1f */
	DUK__MKBITS(0, 0, 0, 0, 0, 0, 0, 0), DUK__MKBITS(0, 0, 1, 1, 0, 1, 1, 1),  /* 0x20-0x2f */
	DUK__MKBITS(1, 1, 1, 1, 1, 1, 1, 1), DUK__MKBITS(1, 1, 0, 0, 0, 0, 0, 0),  /* 0x30-0x3f */
	DUK__MKBITS(1, 1, 1, 1, 1, 1, 1, 1), DUK__MKBITS(1, 1, 1, 1, 1, 1, 1, 1),  /* 0x40-0x4f */
	DUK__MKBITS(1, 1, 1, 1, 1, 1, 1, 1), DUK__MKBITS(1, 1, 1, 0, 0, 0, 0, 1),  /* 0x50-0x5f */
	DUK__MKBITS(0, 1, 1, 1, 1, 1, 1, 1), DUK__MKBITS(1, 1, 1, 1, 1, 1, 1, 1),  /* 0x60-0x6f */
	DUK__MKBITS(1, 1, 1, 1, 1, 1, 1, 1), DUK__MKBITS(1, 1, 1, 0, 0, 0, 0, 0)   /* 0x70-0x7f */
};
#endif  /* DUK_USE_SECTION_B */

typedef struct {
	duk_hthread *thr;
	duk_hstring *h_str;
	duk_hbuffer_dynamic *h_buf;
	duk_uint8_t *p;
	duk_uint8_t *p_start;
	duk_uint8_t *p_end;
} duk__transform_context;

typedef void (*duk__transform_callback)(duk__transform_context *tfm_ctx, void *udata, duk_codepoint_t cp);

/* XXX: refactor and share with other code */
DUK_LOCAL duk_small_int_t duk__decode_hex_escape(duk_uint8_t *p, duk_small_int_t n) {
	duk_small_int_t ch;
	duk_small_int_t t = 0;

	while (n > 0) {
		t = t * 16;
		ch = (duk_small_int_t) duk_hex_dectab[*p++];
		if (DUK_LIKELY(ch >= 0)) {
			t += ch;
		} else {
			return -1;
		}
		n--;
	}
	return t;
}

DUK_LOCAL int duk__transform_helper(duk_context *ctx, duk__transform_callback callback, void *udata) {
	duk_hthread *thr = (duk_hthread *) ctx;
	duk__transform_context tfm_ctx_alloc;
	duk__transform_context *tfm_ctx = &tfm_ctx_alloc;
	duk_codepoint_t cp;

	tfm_ctx->thr = thr;

	tfm_ctx->h_str = duk_to_hstring(ctx, 0);
	DUK_ASSERT(tfm_ctx->h_str != NULL);

	(void) duk_push_dynamic_buffer(ctx, 0);
	tfm_ctx->h_buf = (duk_hbuffer_dynamic *) duk_get_hbuffer(ctx, -1);
	DUK_ASSERT(tfm_ctx->h_buf != NULL);
	DUK_ASSERT(DUK_HBUFFER_HAS_DYNAMIC(tfm_ctx->h_buf));

	tfm_ctx->p_start = DUK_HSTRING_GET_DATA(tfm_ctx->h_str);
	tfm_ctx->p_end = tfm_ctx->p_start + DUK_HSTRING_GET_BYTELEN(tfm_ctx->h_str);
	tfm_ctx->p = tfm_ctx->p_start;

	while (tfm_ctx->p < tfm_ctx->p_end) {
		cp = (duk_codepoint_t) duk_unicode_decode_xutf8_checked(thr, &tfm_ctx->p, tfm_ctx->p_start, tfm_ctx->p_end);
		callback(tfm_ctx, udata, cp);
	}

	duk_to_string(ctx, -1);
	return 1;
}

DUK_LOCAL void duk__transform_callback_encode_uri(duk__transform_context *tfm_ctx, void *udata, duk_codepoint_t cp) {
	duk_uint8_t xutf8_buf[DUK_UNICODE_MAX_XUTF8_LENGTH];
	duk_uint8_t buf[3];
	duk_small_int_t len;
	duk_codepoint_t cp1, cp2;
	duk_small_int_t i, t;
	duk_uint8_t *unescaped_table = (duk_uint8_t *) udata;

	if (cp < 0) {
		goto uri_error;
	} else if ((cp < 0x80L) && DUK__CHECK_BITMASK(unescaped_table, cp)) {
		duk_hbuffer_append_byte(tfm_ctx->thr, tfm_ctx->h_buf, (duk_uint8_t) cp);
		return;
	} else if (cp >= 0xdc00L && cp <= 0xdfffL) {
		goto uri_error;
	} else if (cp >= 0xd800L && cp <= 0xdbffL) {
		/* Needs lookahead */
		if (duk_unicode_decode_xutf8(tfm_ctx->thr, &tfm_ctx->p, tfm_ctx->p_start, tfm_ctx->p_end, (duk_ucodepoint_t *) &cp2) == 0) {
			goto uri_error;
		}
		if (!(cp2 >= 0xdc00L && cp2 <= 0xdfffL)) {
			goto uri_error;
		}
		cp1 = cp;
		cp = ((cp1 - 0xd800L) << 10) + (cp2 - 0xdc00L) + 0x10000L;
	} else if (cp > 0x10ffffL) {
		/* Although we can allow non-BMP characters (they'll decode
		 * back into surrogate pairs), we don't allow extended UTF-8
		 * characters; they would encode to URIs which won't decode
		 * back because of strict UTF-8 checks in URI decoding.
		 * (However, we could just as well allow them here.)
		 */
		goto uri_error;
	} else {
		/* Non-BMP characters within valid UTF-8 range: encode as is.
		 * They'll decode back into surrogate pairs.
		 */
		;
	}

	len = duk_unicode_encode_xutf8((duk_ucodepoint_t) cp, xutf8_buf);
	buf[0] = (duk_uint8_t) '%';
	for (i = 0; i < len; i++) {
		t = (int) xutf8_buf[i];
		buf[1] = (duk_uint8_t) duk_uc_nybbles[t >> 4];
		buf[2] = (duk_uint8_t) duk_uc_nybbles[t & 0x0f];
		duk_hbuffer_append_bytes(tfm_ctx->thr, tfm_ctx->h_buf, buf, 3);
	}
	return;

 uri_error:
	DUK_ERROR(tfm_ctx->thr, DUK_ERR_URI_ERROR, "invalid input");
}

DUK_LOCAL void duk__transform_callback_decode_uri(duk__transform_context *tfm_ctx, void *udata, duk_codepoint_t cp) {
	duk_uint8_t *reserved_table = (duk_uint8_t *) udata;
	duk_small_uint_t utf8_blen;
	duk_codepoint_t min_cp;
	duk_small_int_t t;  /* must be signed */
	duk_small_uint_t i;

	if (cp == (duk_codepoint_t) '%') {
		duk_uint8_t *p = tfm_ctx->p;
		duk_size_t left = (duk_size_t) (tfm_ctx->p_end - p);  /* bytes left */

		DUK_DDD(DUK_DDDPRINT("percent encoding, left=%ld", (long) left));

		if (left < 2) {
			goto uri_error;
		}

		t = duk__decode_hex_escape(p, 2);
		DUK_DDD(DUK_DDDPRINT("first byte: %ld", (long) t));
		if (t < 0) {
			goto uri_error;
		}

		if (t < 0x80) {
			if (DUK__CHECK_BITMASK(reserved_table, t)) {
				/* decode '%xx' to '%xx' if decoded char in reserved set */
				DUK_ASSERT(tfm_ctx->p - 1 >= tfm_ctx->p_start);
				duk_hbuffer_append_bytes(tfm_ctx->thr, tfm_ctx->h_buf, (duk_uint8_t *) (p - 1), 3);
			} else {
				duk_hbuffer_append_byte(tfm_ctx->thr, tfm_ctx->h_buf, (duk_uint8_t) t);
			}
			tfm_ctx->p += 2;
			return;
		}

		/* Decode UTF-8 codepoint from a sequence of hex escapes.  The
		 * first byte of the sequence has been decoded to 't'.
		 *
		 * Note that UTF-8 validation must be strict according to the
		 * specification: E5.1 Section 15.1.3, decode algorithm step
		 * 4.d.vii.8.  URIError from non-shortest encodings is also
		 * specifically noted in the spec.
		 */

		DUK_ASSERT(t >= 0x80);
		if (t < 0xc0) {
			/* continuation byte */
			goto uri_error;
		} else if (t < 0xe0) {
			/* 110x xxxx; 2 bytes */
			utf8_blen = 2;
			min_cp = 0x80L;
			cp = t & 0x1f;
		} else if (t < 0xf0) {
			/* 1110 xxxx; 3 bytes */
			utf8_blen = 3;
			min_cp = 0x800L;
			cp = t & 0x0f;
		} else if (t < 0xf8) {
			/* 1111 0xxx; 4 bytes */
			utf8_blen = 4;
			min_cp = 0x10000L;
			cp = t & 0x07;
		} else {
			/* extended utf-8 not allowed for URIs */
			goto uri_error;
		}

		if (left < utf8_blen * 3 - 1) {
			/* '%xx%xx...%xx', p points to char after first '%' */
			goto uri_error;
		}

		p += 3;
		for (i = 1; i < utf8_blen; i++) {
			/* p points to digit part ('%xy', p points to 'x') */
			t = duk__decode_hex_escape(p, 2);
			DUK_DDD(DUK_DDDPRINT("i=%ld utf8_blen=%ld cp=%ld t=0x%02lx",
			                     (long) i, (long) utf8_blen, (long) cp, (unsigned long) t));
			if (t < 0) {
				goto uri_error;
			}
			if ((t & 0xc0) != 0x80) {
				goto uri_error;
			}
			cp = (cp << 6) + (t & 0x3f);
			p += 3;
		}
		p--;  /* p overshoots */
		tfm_ctx->p = p;

		DUK_DDD(DUK_DDDPRINT("final cp=%ld, min_cp=%ld", (long) cp, (long) min_cp));

		if (cp < min_cp || cp > 0x10ffffL || (cp >= 0xd800L && cp <= 0xdfffL)) {
			goto uri_error;
		}

		/* The E5.1 algorithm checks whether or not a decoded codepoint
		 * is below 0x80 and perhaps may be in the "reserved" set.
		 * This seems pointless because the single byte UTF-8 case is
		 * handled separately, and non-shortest encodings are rejected.
		 * So, 'cp' cannot be below 0x80 here, and thus cannot be in
		 * the reserved set.
		 */

		/* utf-8 validation ensures these */
		DUK_ASSERT(cp >= 0x80L && cp <= 0x10ffffL);

		if (cp >= 0x10000L) {
			cp -= 0x10000L;
			DUK_ASSERT(cp < 0x100000L);
			duk_hbuffer_append_xutf8(tfm_ctx->thr, tfm_ctx->h_buf, (duk_ucodepoint_t) ((cp >> 10) + 0xd800L));
			duk_hbuffer_append_xutf8(tfm_ctx->thr, tfm_ctx->h_buf, (duk_ucodepoint_t) ((cp & 0x03ffUL) + 0xdc00L));
		} else {
			duk_hbuffer_append_xutf8(tfm_ctx->thr, tfm_ctx->h_buf, (duk_ucodepoint_t) cp);
		}
	} else {
		duk_hbuffer_append_xutf8(tfm_ctx->thr, tfm_ctx->h_buf, (duk_ucodepoint_t) cp);
	}
	return;

 uri_error:
	DUK_ERROR(tfm_ctx->thr, DUK_ERR_URI_ERROR, "invalid input");
}

#ifdef DUK_USE_SECTION_B
DUK_LOCAL void duk__transform_callback_escape(duk__transform_context *tfm_ctx, void *udata, duk_codepoint_t cp) {
	duk_uint8_t buf[6];
	duk_small_int_t len;

	DUK_UNREF(udata);

	if (cp < 0) {
		goto esc_error;
	} else if ((cp < 0x80L) && DUK__CHECK_BITMASK(duk__escape_unescaped_table, cp)) {
		buf[0] = (duk_uint8_t) cp;
		len = 1;
	} else if (cp < 0x100L) {
		buf[0] = (duk_uint8_t) '%';
		buf[1] = (duk_uint8_t) duk_uc_nybbles[cp >> 4];
		buf[2] = (duk_uint8_t) duk_uc_nybbles[cp & 0x0f];
		len = 3;
	} else if (cp < 0x10000L) {
		buf[0] = (duk_uint8_t) '%';
		buf[1] = (duk_uint8_t) 'u';
		buf[2] = (duk_uint8_t) duk_uc_nybbles[cp >> 12];
		buf[3] = (duk_uint8_t) duk_uc_nybbles[(cp >> 8) & 0x0f];
		buf[4] = (duk_uint8_t) duk_uc_nybbles[(cp >> 4) & 0x0f];
		buf[5] = (duk_uint8_t) duk_uc_nybbles[cp & 0x0f];
		len = 6;
	} else {
		/* Characters outside BMP cannot be escape()'d.  We could
		 * encode them as surrogate pairs (for codepoints inside
		 * valid UTF-8 range, but not extended UTF-8).  Because
		 * escape() and unescape() are legacy functions, we don't.
		 */
		goto esc_error;
	}

	duk_hbuffer_append_bytes(tfm_ctx->thr, tfm_ctx->h_buf, buf, len);
	return;

 esc_error:
	DUK_ERROR(tfm_ctx->thr, DUK_ERR_TYPE_ERROR, "invalid input");
}

DUK_LOCAL void duk__transform_callback_unescape(duk__transform_context *tfm_ctx, void *udata, duk_codepoint_t cp) {
	duk_small_int_t t;

	DUK_UNREF(udata);

	if (cp == (duk_codepoint_t) '%') {
		duk_uint8_t *p = tfm_ctx->p;
		duk_size_t left = (duk_size_t) (tfm_ctx->p_end - p);  /* bytes left */

		if (left >= 5 && p[0] == 'u' &&
		    ((t = duk__decode_hex_escape(p + 1, 4)) >= 0)) {
			cp = (duk_codepoint_t) t;
			tfm_ctx->p += 5;
		} else if (left >= 2 &&
		           ((t = duk__decode_hex_escape(p, 2)) >= 0)) {
			cp = (duk_codepoint_t) t;
			tfm_ctx->p += 2;
		}
	}

	duk_hbuffer_append_xutf8(tfm_ctx->thr, tfm_ctx->h_buf, cp);
}
#endif  /* DUK_USE_SECTION_B */

/*
 *  Eval
 *
 *  Eval needs to handle both a "direct eval" and an "indirect eval".
 *  Direct eval handling needs access to the caller's activation so that its
 *  lexical environment can be accessed.  A direct eval is only possible from
 *  Ecmascript code; an indirect eval call is possible also from C code.
 *  When an indirect eval call is made from C code, there may not be a
 *  calling activation at all which needs careful handling.
 */

DUK_INTERNAL duk_ret_t duk_bi_global_object_eval(duk_context *ctx) {
	duk_hthread *thr = (duk_hthread *) ctx;
	duk_hstring *h;
	duk_activation *act_caller;
	duk_activation *act_eval;
	duk_activation *act;
	duk_hcompiledfunction *func;
	duk_hobject *outer_lex_env;
	duk_hobject *outer_var_env;
	duk_bool_t this_to_global = 1;
	duk_small_uint_t comp_flags;

	DUK_ASSERT_TOP(ctx, 1);
	DUK_ASSERT(thr->callstack_top >= 1);  /* at least this function exists */
	DUK_ASSERT(((thr->callstack + thr->callstack_top - 1)->flags & DUK_ACT_FLAG_DIRECT_EVAL) == 0 || /* indirect eval */
	           (thr->callstack_top >= 2));  /* if direct eval, calling activation must exist */

	/*
	 *  callstack_top - 1 --> this function
	 *  callstack_top - 2 --> caller (may not exist)
	 *
	 *  If called directly from C, callstack_top might be 1.  If calling
	 *  activation doesn't exist, call must be indirect.
	 */

	h = duk_get_hstring(ctx, 0);
	if (!h) {
		return 1;  /* return arg as-is */
	}

	/* [ source ] */

	comp_flags = DUK_JS_COMPILE_FLAG_EVAL;
	act_eval = thr->callstack + thr->callstack_top - 1;    /* this function */
	if (thr->callstack_top >= 2) {
		/* Have a calling activation, check for direct eval (otherwise
		 * assume indirect eval.
		 */
		act_caller = thr->callstack + thr->callstack_top - 2;  /* caller */
		if ((act_caller->flags & DUK_ACT_FLAG_STRICT) &&
		    (act_eval->flags & DUK_ACT_FLAG_DIRECT_EVAL)) {
			/* Only direct eval inherits strictness from calling code
			 * (E5.1 Section 10.1.1).
			 */
			comp_flags |= DUK_JS_COMPILE_FLAG_STRICT;
		}
	} else {
		DUK_ASSERT((act_eval->flags & DUK_ACT_FLAG_DIRECT_EVAL) == 0);
	}
	act_caller = NULL;  /* avoid dereference after potential callstack realloc */
	act_eval = NULL;

	duk_push_hstring_stridx(ctx, DUK_STRIDX_INPUT);  /* XXX: copy from caller? */
	duk_js_compile(thr,
	               (const duk_uint8_t *) DUK_HSTRING_GET_DATA(h),
	               (duk_size_t) DUK_HSTRING_GET_BYTELEN(h),
	               comp_flags);
	func = (duk_hcompiledfunction *) duk_get_hobject(ctx, -1);
	DUK_ASSERT(func != NULL);
	DUK_ASSERT(DUK_HOBJECT_IS_COMPILEDFUNCTION((duk_hobject *) func));

	/* [ source template ] */

	/* E5 Section 10.4.2 */
	DUK_ASSERT(thr->callstack_top >= 1);
	act = thr->callstack + thr->callstack_top - 1;  /* this function */
	if (act->flags & DUK_ACT_FLAG_DIRECT_EVAL) {
		DUK_ASSERT(thr->callstack_top >= 2);
		act = thr->callstack + thr->callstack_top - 2;  /* caller */
		if (act->lex_env == NULL) {
			DUK_ASSERT(act->var_env == NULL);
			DUK_DDD(DUK_DDDPRINT("delayed environment initialization"));

			/* this may have side effects, so re-lookup act */
			duk_js_init_activation_environment_records_delayed(thr, act);
			act = thr->callstack + thr->callstack_top - 2;
		}
		DUK_ASSERT(act->lex_env != NULL);
		DUK_ASSERT(act->var_env != NULL);

		this_to_global = 0;

		if (DUK_HOBJECT_HAS_STRICT((duk_hobject *) func)) {
			duk_hobject *new_env;
			duk_hobject *act_lex_env;

			DUK_DDD(DUK_DDDPRINT("direct eval call to a strict function -> "
			                     "var_env and lex_env to a fresh env, "
			                     "this_binding to caller's this_binding"));

			act = thr->callstack + thr->callstack_top - 2;  /* caller */
			act_lex_env = act->lex_env;
			act = NULL;  /* invalidated */

			(void) duk_push_object_helper_proto(ctx,
			                                    DUK_HOBJECT_FLAG_EXTENSIBLE |
			                                    DUK_HOBJECT_CLASS_AS_FLAGS(DUK_HOBJECT_CLASS_DECENV),
			                                    act_lex_env);
			new_env = duk_require_hobject(ctx, -1);
			DUK_ASSERT(new_env != NULL);
			DUK_DDD(DUK_DDDPRINT("new_env allocated: %!iO",
			                     (duk_heaphdr *) new_env));

			outer_lex_env = new_env;
			outer_var_env = new_env;

			duk_insert(ctx, 0);  /* stash to bottom of value stack to keep new_env reachable */

			/* compiler's responsibility */
			DUK_ASSERT(DUK_HOBJECT_HAS_NEWENV((duk_hobject *) func));
		} else {
			DUK_DDD(DUK_DDDPRINT("direct eval call to a non-strict function -> "
			                     "var_env and lex_env to caller's envs, "
			                     "this_binding to caller's this_binding"));

			outer_lex_env = act->lex_env;
			outer_var_env = act->var_env;

			/* compiler's responsibility */
			DUK_ASSERT(!DUK_HOBJECT_HAS_NEWENV((duk_hobject *) func));
		}
	} else {
		DUK_DDD(DUK_DDDPRINT("indirect eval call -> var_env and lex_env to "
		                     "global object, this_binding to global object"));

		this_to_global = 1;
		outer_lex_env = thr->builtins[DUK_BIDX_GLOBAL_ENV];
		outer_var_env = thr->builtins[DUK_BIDX_GLOBAL_ENV];
	}
	act = NULL;

	duk_js_push_closure(thr, func, outer_var_env, outer_lex_env);

	/* [ source template closure ] */

	if (this_to_global) {
		DUK_ASSERT(thr->builtins[DUK_BIDX_GLOBAL] != NULL);
		duk_push_hobject_bidx(ctx, DUK_BIDX_GLOBAL);
	} else {
		duk_tval *tv;
		DUK_ASSERT(thr->callstack_top >= 2);
		act = thr->callstack + thr->callstack_top - 2;  /* caller */
		tv = thr->valstack + act->idx_bottom - 1;  /* this is just beneath bottom */
		DUK_ASSERT(tv >= thr->valstack);
		duk_push_tval(ctx, tv);
	}

	DUK_DDD(DUK_DDDPRINT("eval -> lex_env=%!iO, var_env=%!iO, this_binding=%!T",
	                     (duk_heaphdr *) outer_lex_env,
	                     (duk_heaphdr *) outer_var_env,
	                     (duk_tval *) duk_get_tval(ctx, -1)));

	/* [ source template closure this ] */

	duk_call_method(ctx, 0);

	/* [ source template result ] */

	return 1;
}

/*
 *  Parsing of ints and floats
 */

DUK_INTERNAL duk_ret_t duk_bi_global_object_parse_int(duk_context *ctx) {
	duk_bool_t strip_prefix;
	duk_int32_t radix;
	duk_small_uint_t s2n_flags;

	DUK_ASSERT_TOP(ctx, 2);
	duk_to_string(ctx, 0);

	strip_prefix = 1;
	radix = duk_to_int32(ctx, 1);
	if (radix != 0) {
		if (radix < 2 || radix > 36) {
			goto ret_nan;
		}
		/* For octal, setting strip_prefix=0 is not necessary, as zero
		 * is tolerated anyway:
		 *
		 *   parseInt('123', 8) === parseInt('0123', 8)     with or without strip_prefix
		 *   parseInt('123', 16) === parseInt('0x123', 16)  requires strip_prefix = 1
		 */
		if (radix != 16) {
			strip_prefix = 0;
		}
	} else {
		radix = 10;
	}

	s2n_flags = DUK_S2N_FLAG_TRIM_WHITE |
	            DUK_S2N_FLAG_ALLOW_GARBAGE |
	            DUK_S2N_FLAG_ALLOW_PLUS |
	            DUK_S2N_FLAG_ALLOW_MINUS |
	            DUK_S2N_FLAG_ALLOW_LEADING_ZERO |
#ifdef DUK_USE_OCTAL_SUPPORT
	            (strip_prefix ? (DUK_S2N_FLAG_ALLOW_AUTO_HEX_INT | DUK_S2N_FLAG_ALLOW_AUTO_OCT_INT) : 0)
#else
	            (strip_prefix ? DUK_S2N_FLAG_ALLOW_AUTO_HEX_INT : 0)
#endif
	            ;

	duk_dup(ctx, 0);
	duk_numconv_parse(ctx, radix, s2n_flags);
	return 1;

 ret_nan:
	duk_push_nan(ctx);
	return 1;
}

DUK_INTERNAL duk_ret_t duk_bi_global_object_parse_float(duk_context *ctx) {
	duk_small_uint_t s2n_flags;
	duk_int32_t radix;

	DUK_ASSERT_TOP(ctx, 1);
	duk_to_string(ctx, 0);

	radix = 10;

	/* XXX: check flags */
	s2n_flags = DUK_S2N_FLAG_TRIM_WHITE |
	            DUK_S2N_FLAG_ALLOW_EXP |
	            DUK_S2N_FLAG_ALLOW_GARBAGE |
	            DUK_S2N_FLAG_ALLOW_PLUS |
	            DUK_S2N_FLAG_ALLOW_MINUS |
	            DUK_S2N_FLAG_ALLOW_INF |
	            DUK_S2N_FLAG_ALLOW_FRAC |
	            DUK_S2N_FLAG_ALLOW_NAKED_FRAC |
	            DUK_S2N_FLAG_ALLOW_EMPTY_FRAC |
	            DUK_S2N_FLAG_ALLOW_LEADING_ZERO;

	duk_numconv_parse(ctx, radix, s2n_flags);
	return 1;
}

/*
 *  Number checkers
 */

DUK_INTERNAL duk_ret_t duk_bi_global_object_is_nan(duk_context *ctx) {
	duk_double_t d = duk_to_number(ctx, 0);
	duk_push_boolean(ctx, DUK_ISNAN(d));
	return 1;
}

DUK_INTERNAL duk_ret_t duk_bi_global_object_is_finite(duk_context *ctx) {
	duk_double_t d = duk_to_number(ctx, 0);
	duk_push_boolean(ctx, DUK_ISFINITE(d));
	return 1;
}

/*
 *  URI handling
 */

DUK_INTERNAL duk_ret_t duk_bi_global_object_decode_uri(duk_context *ctx) {
	return duk__transform_helper(ctx, duk__transform_callback_decode_uri, (void *) duk__decode_uri_reserved_table);
}

DUK_INTERNAL duk_ret_t duk_bi_global_object_decode_uri_component(duk_context *ctx) {
	return duk__transform_helper(ctx, duk__transform_callback_decode_uri, (void *) duk__decode_uri_component_reserved_table);
}

DUK_INTERNAL duk_ret_t duk_bi_global_object_encode_uri(duk_context *ctx) {
	return duk__transform_helper(ctx, duk__transform_callback_encode_uri, (void *) duk__encode_uriunescaped_table);
}

DUK_INTERNAL duk_ret_t duk_bi_global_object_encode_uri_component(duk_context *ctx) {
	return duk__transform_helper(ctx, duk__transform_callback_encode_uri, (void *) duk__encode_uricomponent_unescaped_table);
}

#ifdef DUK_USE_SECTION_B
DUK_INTERNAL duk_ret_t duk_bi_global_object_escape(duk_context *ctx) {
	return duk__transform_helper(ctx, duk__transform_callback_escape, (void *) NULL);
}

DUK_INTERNAL duk_ret_t duk_bi_global_object_unescape(duk_context *ctx) {
	return duk__transform_helper(ctx, duk__transform_callback_unescape, (void *) NULL);
}
#else  /* DUK_USE_SECTION_B */
DUK_INTERNAL duk_ret_t duk_bi_global_object_escape(duk_context *ctx) {
	DUK_UNREF(ctx);
	return DUK_RET_UNSUPPORTED_ERROR;
}

DUK_INTERNAL duk_ret_t duk_bi_global_object_unescape(duk_context *ctx) {
	DUK_UNREF(ctx);
	return DUK_RET_UNSUPPORTED_ERROR;
}
#endif  /* DUK_USE_SECTION_B */

#ifdef DUK_USE_BROWSER_LIKE
#ifdef DUK_USE_FILE_IO
DUK_LOCAL duk_ret_t duk__print_alert_helper(duk_context *ctx, duk_file *f_out) {
	duk_idx_t nargs;
	duk_idx_t i;
	const char *str;
	duk_size_t len;
	char nl = '\n';

	/* If argument count is 1 and first argument is a buffer, write the buffer
	 * as raw data into the file without a newline; this allows exact control
	 * over stdout/stderr without an additional entrypoint (useful for now).
	 */

	nargs = duk_get_top(ctx);
	if (nargs == 1 && duk_is_buffer(ctx, 0)) {
		const char *buf = NULL;
		duk_size_t sz = 0;
		buf = (const char *) duk_get_buffer(ctx, 0, &sz);
		if (buf && sz > 0) {
			DUK_FWRITE(buf, 1, sz, f_out);
		}
		goto flush;
	}

	/* XXX: What are the best semantics / specification for print()?
	 * Now apply ToString() to arguments and join with a single space.
	 */
	/* XXX: ToString() coerce inplace instead? */

	if (nargs > 0) {
		for (i = 0; i < nargs; i++) {
			if (i != 0) {
				duk_push_hstring_stridx(ctx, DUK_STRIDX_SPACE);
			}
			duk_dup(ctx, i);
			duk_to_string(ctx, -1);
		}

		duk_concat(ctx, 2 * nargs - 1);

		str = duk_get_lstring(ctx, -1, &len);
		if (str) {
			DUK_FWRITE(str, 1, len, f_out);
		}
	}

	DUK_FWRITE((const char *) &nl, 1, 1, f_out);

 flush:
	DUK_FFLUSH(f_out);
	return 0;
}

DUK_INTERNAL duk_ret_t duk_bi_global_object_print(duk_context *ctx) {
	return duk__print_alert_helper(ctx, DUK_STDOUT);
}

DUK_INTERNAL duk_ret_t duk_bi_global_object_alert(duk_context *ctx) {
	return duk__print_alert_helper(ctx, DUK_STDERR);
}
#else  /* DUK_USE_FILE_IO */
/* Supported but no file I/O -> silently ignore, no error */
DUK_INTERNAL duk_ret_t duk_bi_global_object_print(duk_context *ctx) {
	DUK_UNREF(ctx);
	return 0;
}

DUK_INTERNAL duk_ret_t duk_bi_global_object_alert(duk_context *ctx) {
	DUK_UNREF(ctx);
	return 0;
}
#endif  /* DUK_USE_FILE_IO */
#else  /* DUK_USE_BROWSER_LIKE */
DUK_INTERNAL duk_ret_t duk_bi_global_object_print(duk_context *ctx) {
	DUK_UNREF(ctx);
	return DUK_RET_UNSUPPORTED_ERROR;
}

DUK_INTERNAL duk_ret_t duk_bi_global_object_alert(duk_context *ctx) {
	DUK_UNREF(ctx);
	return DUK_RET_UNSUPPORTED_ERROR;
}
#endif  /* DUK_USE_BROWSER_LIKE */

/*
 *  CommonJS require() and modules support
 */

#if defined(DUK_USE_COMMONJS_MODULES)
DUK_LOCAL void duk__bi_global_resolve_module_id(duk_context *ctx, const char *req_id, const char *mod_id) {
	duk_hthread *thr = (duk_hthread *) ctx;
	duk_size_t mod_id_len;
	duk_size_t req_id_len;
	duk_uint8_t buf_in[DUK_BI_COMMONJS_MODULE_ID_LIMIT];
	duk_uint8_t buf_out[DUK_BI_COMMONJS_MODULE_ID_LIMIT];
	duk_uint8_t *p;
	duk_uint8_t *q;

	DUK_ASSERT(req_id != NULL);
	/* mod_id may be NULL */
	DUK_ASSERT(sizeof(buf_out) >= sizeof(buf_in));  /* bound checking requires this */

	/*
	 *  A few notes on the algorithm:
	 *
	 *    - Terms are not allowed to begin with a period unless the term
	 *      is either '.' or '..'.  This simplifies implementation (and
	 *      is within CommonJS modules specification).
	 *
	 *    - There are few output bound checks here.  This is on purpose:
	 *      we check the input length and rely on the output never being
	 *      longer than the input, so we cannot run out of output space.
	 *
	 *    - Non-ASCII characters are processed as individual bytes and
	 *      need no special treatment.  However, U+0000 terminates the
	 *      algorithm; this is not an issue because U+0000 is not a
	 *      desirable term character anyway.
	 */

	/*
	 *  Set up the resolution input which is the requested ID directly
	 *  (if absolute or no current module path) or with current module
	 *  ID prepended (if relative and current module path exists).
	 *
	 *  Suppose current module is 'foo/bar' and relative path is './quux'.
	 *  The 'bar' component must be replaced so the initial input here is
	 *  'foo/bar/.././quux'.
	 */

	req_id_len = DUK_STRLEN(req_id);
	if (mod_id != NULL && req_id[0] == '.') {
		mod_id_len = DUK_STRLEN(mod_id);
		if (mod_id_len + 4 + req_id_len + 1 >= sizeof(buf_in)) {
			DUK_DD(DUK_DDPRINT("resolve error: current and requested module ID don't fit into resolve input buffer"));
			goto resolve_error;
		}
		(void) DUK_SNPRINTF((char *) buf_in, sizeof(buf_in), "%s/../%s", (const char *) mod_id, (const char *) req_id);
	} else {
		if (req_id_len + 1 >= sizeof(buf_in)) {
			DUK_DD(DUK_DDPRINT("resolve error: requested module ID doesn't fit into resolve input buffer"));
			goto resolve_error;
		}
		(void) DUK_SNPRINTF((char *) buf_in, sizeof(buf_in), "%s", (const char *) req_id);
	}
	buf_in[sizeof(buf_in) - 1] = (duk_uint8_t) 0;

	DUK_DDD(DUK_DDDPRINT("input module id: '%s'", (const char *) buf_in));

	/*
	 *  Resolution loop.  At the top of the loop we're expecting a valid
	 *  term: '.', '..', or a non-empty identifier not starting with a period.
	 */

	p = buf_in;
	q = buf_out;
	for (;;) {
		duk_uint_fast8_t c;

		/* Here 'p' always points to the start of a term. */
		DUK_DDD(DUK_DDDPRINT("resolve loop top: p -> '%s', q=%p, buf_out=%p",
		                     (const char *) p, (void *) q, (void *) buf_out));

		c = *p++;
		if (DUK_UNLIKELY(c == 0)) {
			DUK_DD(DUK_DDPRINT("resolve error: requested ID must end with a non-empty term"));
			goto resolve_error;
		} else if (DUK_UNLIKELY(c == '.')) {
			c = *p++;
			if (c == '/') {
				/* Term was '.' and is eaten entirely (including dup slashes). */
				goto eat_dup_slashes;
			}
			if (c == '.' && *p == '/') {
				/* Term was '..', backtrack resolved name by one component.
				 *  q[-1] = previous slash (or beyond start of buffer)
				 *  q[-2] = last char of previous component (or beyond start of buffer)
				 */
				p++;  /* eat (first) input slash */
				DUK_ASSERT(q >= buf_out);
				if (q == buf_out) {
					DUK_DD(DUK_DDPRINT("resolve error: term was '..' but nothing to backtrack"));
					goto resolve_error;
				}
				DUK_ASSERT(*(q - 1) == '/');
				q--;  /* backtrack to last output slash */
				for (;;) {
					/* Backtrack to previous slash or start of buffer. */
					DUK_ASSERT(q >= buf_out);
					if (q == buf_out) {
						break;
					}
					if (*(q - 1) == '/') {
						break;
					}
					q--;
				}
				goto eat_dup_slashes;
			}
			DUK_DD(DUK_DDPRINT("resolve error: term begins with '.' but is not '.' or '..' (not allowed now)"));
			goto resolve_error;
		} else if (DUK_UNLIKELY(c == '/')) {
			/* e.g. require('/foo'), empty terms not allowed */
			DUK_DD(DUK_DDPRINT("resolve error: empty term (not allowed now)"));
			goto resolve_error;
		} else {
			for (;;) {
				/* Copy term name until end or '/'. */
				*q++ = c;
				c = *p++;
				if (DUK_UNLIKELY(c == 0)) {
					goto loop_done;
				} else if (DUK_UNLIKELY(c == '/')) {
					*q++ = '/';
					break;
				} else {
					/* write on next loop */
				}
			}
		}

	 eat_dup_slashes:
		for (;;) {
			/* eat dup slashes */
			c = *p;
			if (DUK_LIKELY(c != '/')) {
				break;
			}
			p++;
		}
	}
 loop_done:

	duk_push_lstring(ctx, (const char *) buf_out, (size_t) (q - buf_out));
	return;

 resolve_error:
	DUK_ERROR(thr, DUK_ERR_TYPE_ERROR, "cannot resolve module id: %s", (const char *) req_id);
}
#endif  /* DUK_USE_COMMONJS_MODULES */

#if defined(DUK_USE_COMMONJS_MODULES)
DUK_INTERNAL duk_ret_t duk_bi_global_object_require(duk_context *ctx) {
	const char *str_req_id;  /* requested identifier */
	const char *str_mod_id;  /* require.id of current module */

	/* NOTE: we try to minimize code size by avoiding unnecessary pops,
	 * so the stack looks a bit cluttered in this function.  DUK_ASSERT_TOP()
	 * assertions are used to ensure stack configuration is correct at each
	 * step.
	 */

	/*
	 *  Resolve module identifier into canonical absolute form.
	 */

	str_req_id = duk_require_string(ctx, 0);
	duk_push_current_function(ctx);
	duk_get_prop_stridx(ctx, -1, DUK_STRIDX_ID);
	str_mod_id = duk_get_string(ctx, 2);  /* ignore non-strings */
	DUK_DDD(DUK_DDDPRINT("resolve module id: requested=%!T, currentmodule=%!T",
	                     (duk_tval *) duk_get_tval(ctx, 0),
	                     (duk_tval *) duk_get_tval(ctx, 2)));
	duk__bi_global_resolve_module_id(ctx, str_req_id, str_mod_id);
	str_req_id = NULL;
	str_mod_id = NULL;
	DUK_DDD(DUK_DDDPRINT("resolved module id: requested=%!T, currentmodule=%!T, result=%!T",
	                     (duk_tval *) duk_get_tval(ctx, 0),
	                     (duk_tval *) duk_get_tval(ctx, 2),
	                     (duk_tval *) duk_get_tval(ctx, 3)));

	/* [ requested_id require require.id resolved_id ] */
	DUK_ASSERT_TOP(ctx, 4);

	/*
	 *  Cached module check.
	 *
	 *  If module has been loaded or its loading has already begun without
	 *  finishing, return the same cached value ('exports').  The value is
	 *  registered when module load starts so that circular references can
	 *  be supported to some extent.
	 */

	/* [ requested_id require require.id resolved_id ] */
	DUK_ASSERT_TOP(ctx, 4);

	duk_push_hobject_bidx(ctx, DUK_BIDX_DUKTAPE);
	duk_get_prop_stridx(ctx, 4, DUK_STRIDX_MOD_LOADED);  /* Duktape.modLoaded */
	(void) duk_require_hobject(ctx, 5);

	/* [ requested_id require require.id resolved_id Duktape Duktape.modLoaded ] */
	DUK_ASSERT_TOP(ctx, 6);

	duk_dup(ctx, 3);
	if (duk_get_prop(ctx, 5)) {
		/* [ requested_id require require.id resolved_id Duktape Duktape.modLoaded Duktape.modLoaded[id] ] */
		DUK_DD(DUK_DDPRINT("module already loaded: %!T",
		                   (duk_tval *) duk_get_tval(ctx, 3)));
		return 1;
	}

	/* [ requested_id require require.id resolved_id Duktape Duktape.modLoaded undefined ] */
	DUK_ASSERT_TOP(ctx, 7);

	/*
	 *  Module not loaded (and loading not started previously).
	 *
	 *  Create a new require() function with 'id' set to resolved ID
	 *  of module being loaded.  Also create 'exports' and 'module'
	 *  tables but don't register exports to the loaded table yet.
	 *  We don't want to do that unless the user module search callbacks
	 *  succeeds in finding the module.
	 */

	DUK_DD(DUK_DDPRINT("module not yet loaded: %!T",
	                   (duk_tval *) duk_get_tval(ctx, 3)));

	/* Fresh require: require.id is left configurable (but not writable)
	 * so that is not easy to accidentally tweak it, but it can still be
	 * done with Object.defineProperty().
	 *
	 * XXX: require.id could also be just made non-configurable, as there
	 * is no practical reason to touch it.
	 */
	duk_push_c_function(ctx, duk_bi_global_object_require, 1 /*nargs*/);
	duk_dup(ctx, 3);
	duk_def_prop_stridx(ctx, 7, DUK_STRIDX_ID, DUK_PROPDESC_FLAGS_C);  /* a fresh require() with require.id = resolved target module id */

	/* Exports table. */
	duk_push_object(ctx);

	/* Module table: module.id is non-writable and non-configurable, as
	 * the CommonJS spec suggests this if possible.
	 */
	duk_push_object(ctx);
	duk_dup(ctx, 3);  /* resolved id: require(id) must return this same module */
	duk_def_prop_stridx(ctx, 9, DUK_STRIDX_ID, DUK_PROPDESC_FLAGS_NONE);

	/* [ requested_id require require.id resolved_id Duktape Duktape.modLoaded undefined fresh_require exports module ] */
	DUK_ASSERT_TOP(ctx, 10);

	/*
	 *  Call user provided module search function and build the wrapped
	 *  module source code (if necessary).  The module search function
	 *  can be used to implement pure Ecmacsript, pure C, and mixed
	 *  Ecmascript/C modules.
	 *
	 *  The module search function can operate on the exports table directly
	 *  (e.g. DLL code can register values to it).  It can also return a
	 *  string which is interpreted as module source code (if a non-string
	 *  is returned the module is assumed to be a pure C one).  If a module
	 *  cannot be found, an error must be thrown by the user callback.
	 *
	 *  NOTE: the current arrangement allows C modules to be implemented
	 *  but since the exports table is registered to Duktape.modLoaded only
	 *  after the search function returns, circular requires / partially
	 *  loaded modules don't work for C modules.  This is rarely an issue,
	 *  as C modules usually simply expose a set of helper functions.
	 */

	duk_push_string(ctx, "(function(require,exports,module){");

	/* Duktape.modSearch(resolved_id, fresh_require, exports, module). */
	duk_get_prop_stridx(ctx, 4, DUK_STRIDX_MOD_SEARCH);  /* Duktape.modSearch */
	duk_dup(ctx, 3);
	duk_dup(ctx, 7);
	duk_dup(ctx, 8);
	duk_dup(ctx, 9);  /* [ ... Duktape.modSearch resolved_id fresh_require exports module ] */
	duk_call(ctx, 4 /*nargs*/);  /* -> [ ... source ] */
	DUK_ASSERT_TOP(ctx, 12);

	/* Because user callback did not throw an error, remember exports table. */
	duk_dup(ctx, 3);
	duk_dup(ctx, 8);
	duk_def_prop(ctx, 5, DUK_PROPDESC_FLAGS_EC);  /* Duktape.modLoaded[resolved_id] = exports */

	/* If user callback did not return source code, module loading
	 * is finished (user callback initialized exports table directly).
	 */
	if (!duk_is_string(ctx, 11)) {
		/* User callback did not return source code, so
		 * module loading is finished.
		 */
		duk_dup(ctx, 8);
		return 1;
	}

	/* Finish the wrapped module source.  Force resolved module ID as the
	 * fileName so it gets set for functions defined within a module.  This
	 * also ensures loggers created within the module get the module ID as
	 * their default logger name.
	 */
	duk_push_string(ctx, "})");
	duk_concat(ctx, 3);
	duk_dup(ctx, 3);  /* resolved module ID for fileName */
	duk_eval_raw(ctx, NULL, 0, DUK_COMPILE_EVAL);

	/* XXX: The module wrapper function is currently anonymous and is shown
	 * in stack traces.  It would be nice to force it to match the module
	 * name (perhaps just the cleaned up last term).  At the moment 'name'
	 * is write protected so we can't change it directly.  Note that we must
	 * not introduce an actual name binding into the function scope (which
	 * is usually the case with a named function) because it would affect
	 * the scope seen by the module and shadow accesses to globals of the
	 * same name.
	 */

	/*
	 *  Call the wrapped module function.
	 */

	/* [ requested_id require require.id resolved_id Duktape Duktape.modLoaded undefined fresh_require exports module mod_func ] */
	DUK_ASSERT_TOP(ctx, 11);

	duk_dup(ctx, 8);  /* exports (this binding) */
	duk_dup(ctx, 7);  /* fresh require (argument) */
	duk_dup(ctx, 8);  /* exports (argument) */
	duk_dup(ctx, 9);  /* module (argument) */

	/* [ requested_id require require.id resolved_id Duktape Duktape.modLoaded undefined fresh_require exports module mod_func exports fresh_require exports module ] */
	DUK_ASSERT_TOP(ctx, 15);

	duk_call_method(ctx, 3 /*nargs*/);

	/* [ requested_id require require.id resolved_id Duktape Duktape.modLoaded undefined fresh_require exports module result(ignored) ] */
	DUK_ASSERT_TOP(ctx, 11);

	duk_pop_2(ctx);
	return 1;  /* return exports */
}
#else
DUK_INTERNAL duk_ret_t duk_bi_global_object_require(duk_context *ctx) {
	DUK_UNREF(ctx);
	return DUK_RET_UNSUPPORTED_ERROR;
}
#endif  /* DUK_USE_COMMONJS_MODULES */
