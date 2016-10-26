#ifndef JSONPUCK_H_INCLUDED
#define JSONPUCK_H_INCLUDED

#if defined(__cplusplus) && !defined(__STDC_CONSTANT_MACROS)
#define __STDC_CONSTANT_MACROS 1 /* make С++ to be happy */
#endif
#if defined(__cplusplus) && !defined(__STDC_LIMIT_MACROS)
#define __STDC_LIMIT_MACROS 1    /* make С++ to be happy */
#endif
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/*
 * {{{ Platform-specific definitions
 */

/** \cond false **/

#if defined(__CC_ARM)         /* set the alignment to 1 for armcc compiler */
#define JS_PACKED    __packed
#else
#define JS_PACKED  __attribute__((packed))
#endif

#if defined(__GNUC__) && !defined(__GNUC_STDC_INLINE__)
#if !defined(JS_SOURCE)
#define JS_PROTO extern inline
#define JS_IMPL extern inline
#else /* defined(JS_SOURCE) */
#define JS_PROTO
#define JS_IMPL
#endif
#define JS_ALWAYSINLINE
#else /* C99 inline */
#if !defined(JS_SOURCE)
#define JS_PROTO inline
#define JS_IMPL inline
#else /* defined(JS_SOURCE) */
#define JS_PROTO extern inline
#define JS_IMPL inline
#endif
#define JS_ALWAYSINLINE __attribute__((always_inline))
#endif /* GNU inline or C99 inline */

#if !defined __GNUC_MINOR__ || defined __INTEL_COMPILER || \
	defined __SUNPRO_C || defined __SUNPRO_CC
#define JS_GCC_VERSION(major, minor) 0
#else
#define JS_GCC_VERSION(major, minor) (__GNUC__ > (major) || \
	(__GNUC__ == (major) && __GNUC_MINOR__ >= (minor)))
#endif

#if !defined(__has_builtin)
#define __has_builtin(x) 0 /* clang */
#endif

#if JS_GCC_VERSION(2, 9) || __has_builtin(__builtin_expect)
#define js_likely(x) __builtin_expect((x), 1)
#define js_unlikely(x) __builtin_expect((x), 0)
#else
#define js_likely(x) (x)
#define js_unlikely(x) (x)
#endif

#if JS_GCC_VERSION(4, 5) || __has_builtin(__builtin_unreachable)
#define js_unreachable() (assert(0), __builtin_unreachable())
#else
JS_PROTO void
js_unreachable(void) __attribute__((noreturn));
JS_PROTO void
js_unreachable(void) { assert(0); abort(); }
#define js_unreachable() (assert(0))
#endif

#define JS_LOAD_STORE(name, type)					\
JS_PROTO type									\
js_load_##name(const char **data);						\
JS_IMPL type									\
js_load_##name(const char **data)						\
{										\
	struct JS_PACKED cast { type val; };					\
	type val = ((struct cast *) *data)->val;				\
	*data += sizeof(type);							\
	return val;								\
}										\
JS_PROTO char *									\
js_store_##name(char *data, type val);						\
JS_IMPL char *									\
js_store_##name(char *data, type val)						\
{										\
	struct JS_PACKED cast { type val; };					\
	((struct cast *) (data))->val = val;				\
	return data + sizeof(type);						\
}

JS_LOAD_STORE(u8, uint8_t);
JS_LOAD_STORE(u16, uint16_t);
JS_LOAD_STORE(u32, uint32_t);
JS_LOAD_STORE(u64, uint64_t);
JS_LOAD_STORE(float, float);
JS_LOAD_STORE(double, double);

#undef JS_LOAD_STORE

/** \endcond */

/*
 * }}}
 */

/*
 * {{{ API definition
 */

/**
 * \brief JSONPack data types
 */
enum js_type {
	JS_NIL = 0,
	JS_UINT,
	JS_INT,
	JS_STR,
	JS_BIN,
	JS_ARRAY,
	JS_MAP,
	JS_BOOL,
	JS_FLOAT,
	JS_DOUBLE,
	JS_EXT
};

/**
 * \brief Determine JSONPack type by a first byte \a c of encoded data.
 *
 * Example usage:
 * \code
 * assert(JS_ARRAY == js_typeof(0x90));
 * \endcode
 *
 * \param c - a first byte of encoded data
 * \return JSONPack type
 */
JS_PROTO __attribute__((pure)) enum js_type
js_typeof(const char c);

/**
 * \brief Calculate exact buffer size needed to store an array header of
 * \a size elements. Maximum return value is 5. For performance reasons you
 * can preallocate buffer for maximum size without calling the function.
 * \param size - a number of elements
 * \return buffer size in bytes (max is 5)
 */
JS_PROTO __attribute__((const)) uint32_t
js_sizeof_array(uint32_t size);

/**
 * \brief Encode an array header of \a size elements.
 *
 * All array members must be encoded after the header.
 *
 * Example usage:
 * \code
 * // Encode
 * char buf[1024];
 * char *w = buf;
 * w = js_encode_array(w, 2)
 * w = js_encode_uint(w, 10);
 * w = js_encode_uint(w, 15);
 *
 * // Decode
 * const char *r = buf;
 * uint32_t size = js_decode_array(&r);
 * for (uint32_t i = 0; i < size; i++) {
 *     uint64_t val = js_decode_uint(&r);
 * }
 * assert (r == w);
 * \endcode
 * It is your responsibility to ensure that \a data has enough space.
 * \param data - a buffer
 * \param size - a number of elements
 * \return \a data + \link js_sizeof_array() js_sizeof_array(size) \endlink
 * \sa js_sizeof_array
 */
JS_PROTO char *
js_encode_array(char *data, uint32_t size);

/**
 * \brief Check that \a cur buffer has enough bytes to decode an array header
 * \param cur buffer
 * \param end end of the buffer
 * \retval 0 - buffer has enough bytes
 * \retval > 0 - the number of remaining bytes to read
 * \pre cur < end
 * \pre js_typeof(*cur) == JS_ARRAY
 */
JS_PROTO __attribute__((pure)) ptrdiff_t
js_check_array(const char *cur, const char *end);

/**
 * \brief Decode an array header from JSONPack \a data.
 *
 * All array members must be decoded after the header.
 * \param data - the pointer to a buffer
 * \return the number of elements in an array
 * \post *data = *data + js_sizeof_array(retval)
 * \sa \link js_encode_array() An usage example \endlink
 */
JS_PROTO uint32_t
js_decode_array(const char **data);

/**
 * \brief Calculate exact buffer size needed to store a map header of
 * \a size elements. Maximum return value is 5. For performance reasons you
 * can preallocate buffer for maximum size without calling the function.
 * \param size - a number of elements
 * \return buffer size in bytes (max is 5)
 */
JS_PROTO __attribute__((const)) uint32_t
js_sizeof_map(uint32_t size);

/**
 * \brief Encode a map header of \a size elements.
 *
 * All map key-value pairs must be encoded after the header.
 *
 * Example usage:
 * \code
 * char buf[1024];
 *
 * // Encode
 * char *w = buf;
 * w = js_encode_map(b, 2);
 * w = js_encode_str(b, "key1", 4);
 * w = js_encode_str(b, "value1", 6);
 * w = js_encode_str(b, "key2", 4);
 * w = js_encode_str(b, "value2", 6);
 *
 * // Decode
 * const char *r = buf;
 * uint32_t size = js_decode_map(&r);
 * for (uint32_t i = 0; i < size; i++) {
 *      // Use switch(js_typeof(**r)) to support more types
 *     uint32_t key_len, val_len;
 *     const char *key = js_decode_str(&r, key_len);
 *     const char *val = js_decode_str(&r, val_len);
 * }
 * assert (r == w);
 * \endcode
 * It is your responsibility to ensure that \a data has enough space.
 * \param data - a buffer
 * \param size - a number of key/value pairs
 * \return \a data + \link js_sizeof_map() js_sizeof_map(size)\endlink
 * \sa js_sizeof_map
 */
JS_PROTO char *
js_encode_map(char *data, uint32_t size);

/**
 * \brief Check that \a cur buffer has enough bytes to decode a map header
 * \param cur buffer
 * \param end end of the buffer
 * \retval 0 - buffer has enough bytes
 * \retval > 0 - the number of remaining bytes to read
 * \pre cur < end
 * \pre js_typeof(*cur) == JS_MAP
 */
JS_PROTO __attribute__((pure)) ptrdiff_t
js_check_map(const char *cur, const char *end);

/**
 * \brief Decode a map header from JSONPack \a data.
 *
 * All map key-value pairs must be decoded after the header.
 * \param data - the pointer to a buffer
 * \return the number of key/value pairs in a map
 * \post *data = *data + js_sizeof_array(retval)
 * \sa \link js_encode_map() An usage example \endlink
 */
JS_PROTO uint32_t
js_decode_map(const char **data);

/**
 * \brief Calculate exact buffer size needed to store an integer \a num.
 * Maximum return value is 9. For performance reasons you can preallocate
 * buffer for maximum size without calling the function.
 * Example usage:
 * \code
 * char **data = ...;
 * char *end = *data;
 * my_buffer_ensure(js_sizeof_uint(x), &end);
 * // my_buffer_ensure(9, &end);
 * js_encode_uint(buffer, x);
 * \endcode
 * \param num - a number
 * \return buffer size in bytes (max is 9)
 */
JS_PROTO __attribute__((const)) uint32_t
js_sizeof_uint(uint64_t num);

/**
 * \brief Calculate exact buffer size needed to store an integer \a num.
 * Maximum return value is 9. For performance reasons you can preallocate
 * buffer for maximum size without calling the function.
 * \param num - a number
 * \return buffer size in bytes (max is 9)
 * \pre \a num < 0
 */
JS_PROTO __attribute__((const)) uint32_t
js_sizeof_int(int64_t num);

/**
 * \brief Encode an unsigned integer \a num.
 * It is your responsibility to ensure that \a data has enough space.
 * \param data - a buffer
 * \param num - a number
 * \return \a data + js_sizeof_uint(\a num)
 * \sa \link js_encode_array() An usage example \endlink
 * \sa js_sizeof_uint()
 */
JS_PROTO char *
js_encode_uint(char *data, uint64_t num);

/**
 * \brief Encode a signed integer \a num.
 * It is your responsibility to ensure that \a data has enough space.
 * \param data - a buffer
 * \param num - a number
 * \return \a data + js_sizeof_int(\a num)
 * \sa \link js_encode_array() An usage example \endlink
 * \sa js_sizeof_int()
 * \pre \a num < 0
 */
JS_PROTO char *
js_encode_int(char *data, int64_t num);

/**
 * \brief Check that \a cur buffer has enough bytes to decode an uint
 * \param cur buffer
 * \param end end of the buffer
 * \retval 0 - buffer has enough bytes
 * \retval > 0 - the number of remaining bytes to read
 * \pre cur < end
 * \pre js_typeof(*cur) == JS_UINT
 */
JS_PROTO __attribute__((pure)) ptrdiff_t
js_check_uint(const char *cur, const char *end);

/**
 * \brief Check that \a cur buffer has enough bytes to decode an int
 * \param cur buffer
 * \param end end of the buffer
 * \retval 0 - buffer has enough bytes
 * \retval > 0 - the number of remaining bytes to read
 * \pre cur < end
 * \pre js_typeof(*cur) == JS_INT
 */
JS_PROTO __attribute__((pure)) ptrdiff_t
js_check_int(const char *cur, const char *end);

/**
 * \brief Decode an unsigned integer from JSONPack \a data
 * \param data - the pointer to a buffer
 * \return an unsigned number
 * \post *data = *data + js_sizeof_uint(retval)
 */
JS_PROTO uint64_t
js_decode_uint(const char **data);

/**
 * \brief Decode a signed integer from JSONPack \a data
 * \param data - the pointer to a buffer
 * \return an unsigned number
 * \post *data = *data + js_sizeof_int(retval)
 */
JS_PROTO int64_t
js_decode_int(const char **data);

/**
 * \brief Compare two packed unsigned integers.
 *
 * The function is faster than two js_decode_uint() calls.
 * \param data_a unsigned int a
 * \param data_b unsigned int b
 * \retval < 0 when \a a < \a b
 * \retval   0 when \a a == \a b
 * \retval > 0 when \a a > \a b
 */
JS_PROTO __attribute__((pure)) int
js_compare_uint(const char *data_a, const char *data_b);

/**
 * \brief Calculate exact buffer size needed to store a float \a num.
 * The return value is always 5. The function was added to provide integrity of
 * the library.
 * \param num - a float
 * \return buffer size in bytes (always 5)
 */
JS_PROTO __attribute__((const)) uint32_t
js_sizeof_float(float num);

/**
 * \brief Calculate exact buffer size needed to store a double \a num.
 * The return value is either 5 or 9. The function was added to provide
 * integrity of the library. For performance reasons you can preallocate buffer
 * for maximum size without calling the function.
 * \param num - a double
 * \return buffer size in bytes (5 or 9)
 */
JS_PROTO __attribute__((const)) uint32_t
js_sizeof_double(double num);

/**
 * \brief Encode a float \a num.
 * It is your responsibility to ensure that \a data has enough space.
 * \param data - a buffer
 * \param num - a float
 * \return \a data + js_sizeof_float(\a num)
 * \sa js_sizeof_float()
 * \sa \link js_encode_array() An usage example \endlink
 */
JS_PROTO char *
js_encode_float(char *data, float num);

/**
 * \brief Encode a double \a num.
 * It is your responsibility to ensure that \a data has enough space.
 * \param data - a buffer
 * \param num - a float
 * \return \a data + js_sizeof_double(\a num)
 * \sa \link js_encode_array() An usage example \endlink
 * \sa js_sizeof_double()
 */
JS_PROTO char *
js_encode_double(char *data, double num);

/**
 * \brief Check that \a cur buffer has enough bytes to decode a float
 * \param cur buffer
 * \param end end of the buffer
 * \retval 0 - buffer has enough bytes
 * \retval > 0 - the number of remaining bytes to read
 * \pre cur < end
 * \pre js_typeof(*cur) == JS_FLOAT
 */
JS_PROTO __attribute__((pure)) ptrdiff_t
js_check_float(const char *cur, const char *end);

/**
 * \brief Check that \a cur buffer has enough bytes to decode a double
 * \param cur buffer
 * \param end end of the buffer
 * \retval 0 - buffer has enough bytes
 * \retval > 0 - the number of remaining bytes to read
 * \pre cur < end
 * \pre js_typeof(*cur) == JS_DOUBLE
 */
JS_PROTO __attribute__((pure)) ptrdiff_t
js_check_double(const char *cur, const char *end);

/**
 * \brief Decode a float from JSONPack \a data
 * \param data - the pointer to a buffer
 * \return a float
 * \post *data = *data + js_sizeof_float(retval)
 */
JS_PROTO float
js_decode_float(const char **data);

/**
 * \brief Decode a double from JSONPack \a data
 * \param data - the pointer to a buffer
 * \return a double
 * \post *data = *data + js_sizeof_double(retval)
 */
JS_PROTO double
js_decode_double(const char **data);

/**
 * \brief Calculate exact buffer size needed to store a string header of
 * length \a num. Maximum return value is 5. For performance reasons you can
 * preallocate buffer for maximum size without calling the function.
 * \param len - a string length
 * \return size in chars (max is 5)
 */
JS_PROTO __attribute__((const)) uint32_t
js_sizeof_strl(uint32_t len);

/**
 * \brief Equivalent to js_sizeof_strl(\a len) + \a len.
 * \param len - a string length
 * \return size in chars (max is 5 + \a len)
 */
JS_PROTO __attribute__((const)) uint32_t
js_sizeof_str(uint32_t len);

/**
 * \brief Calculate exact buffer size needed to store a binstring header of
 * length \a num. Maximum return value is 5. For performance reasons you can
 * preallocate buffer for maximum size without calling the function.
 * \param len - a string length
 * \return size in chars (max is 5)
 */
JS_PROTO __attribute__((const)) uint32_t
js_sizeof_binl(uint32_t len);

/**
 * \brief Equivalent to js_sizeof_binl(\a len) + \a len.
 * \param len - a string length
 * \return size in chars (max is 5 + \a len)
 */
JS_PROTO __attribute__((const)) uint32_t
js_sizeof_bin(uint32_t len);

/**
 * \brief Encode a string header of length \a len.
 *
 * The function encodes JSONPack header (\em only header) for a string of
 * length \a len. You should append actual string data to the buffer manually
 * after encoding the header (exactly \a len bytes without trailing '\0').
 *
 * This approach is very useful for cases when the total length of the string
 * is known in advance, but the string data is not stored in a single
 * continuous buffer (e.g. network packets).
 *
 * It is your responsibility to ensure that \a data has enough space.
 * Usage example:
 * \code
 * char buffer[1024];
 * char *b = buffer;
 * b = js_encode_strl(b, hdr.total_len);
 * char *s = b;
 * memcpy(b, pkt1.data, pkt1.len)
 * b += pkt1.len;
 * // get next packet
 * memcpy(b, pkt2.data, pkt2.len)
 * b += pkt2.len;
 * // get next packet
 * memcpy(b, pkt1.data, pkt3.len)
 * b += pkt3.len;
 *
 * // Check that all data was received
 * assert(hdr.total_len == (uint32_t) (b - s))
 * \endcode
 * Hint: you can dynamically reallocate the buffer during the process.
 * \param data - a buffer
 * \param len - a string length
 * \return \a data + js_sizeof_strl(len)
 * \sa js_sizeof_strl()
 */
JS_PROTO char *
js_encode_strl(char *data, uint32_t len);

/**
 * \brief Encode a string of length \a len.
 * The function is equivalent to js_encode_strl() + memcpy.
 * \param data - a buffer
 * \param str - a pointer to string data
 * \param len - a string length
 * \return \a data + js_sizeof_str(len) ==
 * data + js_sizeof_strl(len) + len
 * \sa js_encode_strl
 */
JS_PROTO char *
js_encode_str(char *data, const char *str, uint32_t len);

/**
 * \brief Encode a binstring header of length \a len.
 * See js_encode_strl() for more details.
 * \param data - a bufer
 * \param len - a string length
 * \return data + js_sizeof_binl(\a len)
 * \sa js_encode_strl
 */
JS_PROTO char *
js_encode_binl(char *data, uint32_t len);

/**
 * \brief Encode a binstring of length \a len.
 * The function is equivalent to js_encode_binl() + memcpy.
 * \param data - a buffer
 * \param str - a pointer to binstring data
 * \param len - a binstring length
 * \return \a data + js_sizeof_bin(\a len) ==
 * data + js_sizeof_binl(\a len) + \a len
 * \sa js_encode_strl
 */
JS_PROTO char *
js_encode_bin(char *data, const char *str, uint32_t len);

/**
 * \brief Encode a sequence of values according to format string.
 * Example: js_format(buf, sz, "[%d {%d%s%d%s}]", 42, 0, "false", 1, "true");
 * to get a jsonpack array of two items: number 42 and map (0->"false, 2->"true")
 * Does not write items that don't fit to data_size argument.
 *
 * \param data - a buffer
 * \param data_size - a buffer size
 * \param format - zero-end string, containing structure of resulting
 * jsonpack and types of next arguments.
 * Format can contain '[' and ']' pairs, defining arrays,
 * '{' and '}' pairs, defining maps, and format specifiers, described below:
 * %d, %i - int
 * %u - unsigned int
 * %ld, %li - long
 * %lu - unsigned long
 * %lld, %lli - long long
 * %llu - unsigned long long
 * %hd, %hi - short
 * %hu - unsigned short
 * %hhd, %hhi - char (as number)
 * %hhu - unsigned char (as number)
 * %f - float
 * %lf - double
 * %b - bool
 * %s - zero-end string
 * %.*s - string with specified length
 * %% is ignored
 * %smthelse assert and undefined behaviour
 * NIL - a nil value
 * all other symbols are ignored.
 *
 * \return the number of requred bytes.
 * \retval > data_size means that is not enough space
 * and whole jsonpack was not encoded.
 */
JS_PROTO size_t
js_format(char *data, size_t data_size, const char *format, ...);

/**
 * \brief js_format variation, taking variable argument list
 * Example:
 *  va_list args;
 *  va_start(args, fmt);
 *  js_vformat(data, data_size, fmt, args);
 *  va_end(args);
 * \sa \link js_format() js_format() \endlink
 */
JS_PROTO size_t
js_vformat(char *data, size_t data_size, const char *format, va_list args);

/**
 * \brief print to \a file jsonpacked data in JSON format.
 * JS_EXT is printed as "EXT" only
 * \param file - pointer to file (or NULL for stdout)
 * \param data - pointer to buffer containing jsonpack object
 * \retval 0 - success
 * \retval -1 - wrong jsonpack
 */
JS_PROTO int
js_fprint(FILE* file, const char *data);

/**
 * \brief Check that \a cur buffer has enough bytes to decode a string header
 * \param cur buffer
 * \param end end of the buffer
 * \retval 0 - buffer has enough bytes
 * \retval > 0 - the number of remaining bytes to read
 * \pre cur < end
 * \pre js_typeof(*cur) == JS_STR
 */
JS_PROTO __attribute__((pure)) ptrdiff_t
js_check_strl(const char *cur, const char *end);

/**
 * \brief Check that \a cur buffer has enough bytes to decode a binstring header
 * \param cur buffer
 * \param end end of the buffer
 * \retval 0 - buffer has enough bytes
 * \retval > 0 - the number of remaining bytes to read
 * \pre cur < end
 * \pre js_typeof(*cur) == JS_BIN
 */
JS_PROTO __attribute__((pure)) ptrdiff_t
js_check_binl(const char *cur, const char *end);

/**
 * \brief Decode a length of a string from JSONPack \a data
 * \param data - the pointer to a buffer
 * \return a length of astring
 * \post *data = *data + js_sizeof_strl(retval)
 * \sa js_encode_strl
 */
JS_PROTO uint32_t
js_decode_strl(const char **data);

/**
 * \brief Decode a string from JSONPack \a data
 * \param data - the pointer to a buffer
 * \param len - the pointer to save a string length
 * \return a pointer to a decoded string
 * \post *data = *data + js_sizeof_str(*len)
 * \sa js_encode_binl
 */
JS_PROTO const char *
js_decode_str(const char **data, uint32_t *len);

/**
 * \brief Decode a length of a binstring from JSONPack \a data
 * \param data - the pointer to a buffer
 * \return a length of a binstring
 * \post *data = *data + js_sizeof_binl(retval)
 * \sa js_encode_binl
 */
JS_PROTO uint32_t
js_decode_binl(const char **data);

/**
 * \brief Decode a binstring from JSONPack \a data
 * \param data - the pointer to a buffer
 * \param len - the pointer to save a binstring length
 * \return a pointer to a decoded binstring
 * \post *data = *data + js_sizeof_str(*len)
 * \sa js_encode_binl
 */
JS_PROTO const char *
js_decode_bin(const char **data, uint32_t *len);

/**
 * \brief Decode a length of a string or binstring from JSONPack \a data
 * \param data - the pointer to a buffer
 * \return a length of a string
 * \post *data = *data + js_sizeof_strbinl(retval)
 * \sa js_encode_binl
 */
JS_PROTO uint32_t
js_decode_strbinl(const char **data);

/**
 * \brief Decode a string or binstring from JSONPack \a data
 * \param data - the pointer to a buffer
 * \param len - the pointer to save a binstring length
 * \return a pointer to a decoded binstring
 * \post *data = *data + js_sizeof_strbinl(*len)
 * \sa js_encode_binl
 */
JS_PROTO const char *
js_decode_strbin(const char **data, uint32_t *len);

/**
 * \brief Calculate exact buffer size needed to store the nil value.
 * The return value is always 1. The function was added to provide integrity of
 * the library.
 * \return buffer size in bytes (always 1)
 */
JS_PROTO __attribute__((const)) uint32_t
js_sizeof_nil(void);

/**
 * \brief Encode the nil value.
 * It is your responsibility to ensure that \a data has enough space.
 * \param data - a buffer
 * \return \a data + js_sizeof_nil()
 * \sa \link js_encode_array() An usage example \endlink
 * \sa js_sizeof_nil()
 */
JS_PROTO char *
js_encode_nil(char *data);

/**
 * \brief Check that \a cur buffer has enough bytes to decode nil
 * \param cur buffer
 * \param end end of the buffer
 * \retval 0 - buffer has enough bytes
 * \retval > 0 - the number of remaining bytes to read
 * \pre cur < end
 * \pre js_typeof(*cur) == JS_NIL
 */
JS_PROTO __attribute__((pure)) ptrdiff_t
js_check_nil(const char *cur, const char *end);

/**
 * \brief Decode the nil value from JSONPack \a data
 * \param data - the pointer to a buffer
 * \post *data = *data + js_sizeof_nil()
 */
JS_PROTO void
js_decode_nil(const char **data);

/**
 * \brief Calculate exact buffer size needed to store a boolean value.
 * The return value is always 1. The function was added to provide integrity of
 * the library.
 * \return buffer size in bytes (always 1)
 */
JS_PROTO __attribute__((const)) uint32_t
js_sizeof_bool(bool val);

/**
 * \brief Encode a bool value \a val.
 * It is your responsibility to ensure that \a data has enough space.
 * \param data - a buffer
 * \param val - a bool
 * \return \a data + js_sizeof_bool(val)
 * \sa \link js_encode_array() An usage example \endlink
 * \sa js_sizeof_bool()
 */
JS_PROTO char *
js_encode_bool(char *data, bool val);

/**
 * \brief Check that \a cur buffer has enough bytes to decode a bool value
 * \param cur buffer
 * \param end end of the buffer
 * \retval 0 - buffer has enough bytes
 * \retval > 0 - the number of remaining bytes to read
 * \pre cur < end
 * \pre js_typeof(*cur) == JS_BOOL
 */
JS_PROTO __attribute__((pure)) ptrdiff_t
js_check_bool(const char *cur, const char *end);

/**
 * \brief Decode a bool value from JSONPack \a data
 * \param data - the pointer to a buffer
 * \return a decoded bool value
 * \post *data = *data + js_sizeof_bool(retval)
 */
JS_PROTO bool
js_decode_bool(const char **data);

/**
 * \brief Skip one element in a packed \a data.
 *
 * The function is faster than js_typeof + js_decode_XXX() combination.
 * For arrays and maps the function also skips all members.
 * For strings and binstrings the function also skips the string data.
 *
 * Usage example:
 * \code
 * char buf[1024];
 *
 * char *w = buf;
 * // First JSONPack object
 * w = js_encode_uint(w, 10);
 *
 * // Second JSONPack object
 * w = js_encode_array(w, 4);
 *    w = js_encode_array(w, 2);
 *         // Begin of an inner array
 *         w = js_encode_str(w, "second inner 1", 14);
 *         w = js_encode_str(w, "second inner 2", 14);
 *         // End of an inner array
 *    w = js_encode_str(w, "second", 6);
 *    w = js_encode_uint(w, 20);
 *    w = js_encode_bool(w, true);
 *
 * // Third JSONPack object
 * w = js_encode_str(w, "third", 5);
 * // EOF
 *
 * const char *r = buf;
 *
 * // First JSONPack object
 * assert(js_typeof(**r) == JS_UINT);
 * js_next(&r); // skip the first object
 *
 * // Second JSONPack object
 * assert(js_typeof(**r) == JS_ARRAY);
 * js_decode_array(&r);
 *     assert(js_typeof(**r) == JS_ARRAY); // inner array
 *     js_next(&r); // -->> skip the entire inner array (with all members)
 *     assert(js_typeof(**r) == JS_STR); // second
 *     js_next(&r);
 *     assert(js_typeof(**r) == JS_UINT); // 20
 *     js_next(&r);
 *     assert(js_typeof(**r) == JS_BOOL); // true
 *     js_next(&r);
 *
 * // Third JSONPack object
 * assert(js_typeof(**r) == JS_STR); // third
 * js_next(&r);
 *
 * assert(r == w); // EOF
 *
 * \endcode
 * \param data - the pointer to a buffer
 * \post *data = *data + js_sizeof_TYPE() where TYPE is js_typeof(**data)
 */
JS_PROTO void
js_next(const char **data);

/**
 * \brief Equivalent to js_next() but also validates JSONPack in \a data.
 * \param data - the pointer to a buffer
 * \param end - the end of a buffer
 * \retval 0 when JSONPack in \a data is valid.
 * \retval != 0 when JSONPack in \a data is not valid.
 * \post *data = *data + js_sizeof_TYPE() where TYPE is js_typeof(**data)
 * \post *data is not defined if JSONPack is not valid
 * \sa js_next()
 */
JS_PROTO int
js_check(const char **data, const char *end);

/*
 * }}}
 */

/*
 * {{{ Implementation
 */

/** \cond false */
extern const enum js_type js_type_hint[];
extern const int8_t js_parser_hint[];
extern const char *js_char2escape[];

JS_IMPL JS_ALWAYSINLINE enum js_type
js_typeof(const char c)
{
	return js_type_hint[(uint8_t) c];
}

JS_IMPL uint32_t
js_sizeof_array(uint32_t size)
{
	if (size <= 15) {
		return 1;
	} else if (size <= UINT16_MAX) {
		return 1 + sizeof(uint16_t);
	} else {
		return 1 + sizeof(uint32_t);
	}
}

JS_IMPL char *
js_encode_array(char *data, uint32_t size)
{
	if (size <= 15) {
		return js_store_u8(data, 0x90 | size);
	} else if (size <= UINT16_MAX) {
		data = js_store_u8(data, 0xdc);
		data = js_store_u16(data, size);
		return data;
	} else {
		data = js_store_u8(data, 0xdd);
		return js_store_u32(data, size);
	}
}

JS_IMPL ptrdiff_t
js_check_array(const char *cur, const char *end)
{
	assert(cur < end);
	assert(js_typeof(*cur) == JS_ARRAY);
	uint8_t c = js_load_u8(&cur);
	if (js_likely(!(c & 0x40)))
		return cur - end;

	assert(c >= 0xdc && c <= 0xdd); /* must be checked above by js_typeof */
	uint32_t hsize = 2U << (c & 0x1); /* 0xdc->2, 0xdd->4 */
	return hsize - (end - cur);
}

JS_PROTO uint32_t
js_decode_array_slowpath(uint8_t c, const char **data);

JS_IMPL uint32_t
js_decode_array_slowpath(uint8_t c, const char **data)
{
	uint32_t size;
	switch (c & 0x1) {
	case 0xdc & 0x1:
		size = js_load_u16(data);
		return size;
	case 0xdd & 0x1:
		size = js_load_u32(data);
		return size;
	default:
		js_unreachable();
	}
}

JS_IMPL JS_ALWAYSINLINE uint32_t
js_decode_array(const char **data)
{
	uint8_t c = js_load_u8(data);

	if (js_likely(!(c & 0x40)))
		return (c & 0xf);

	return js_decode_array_slowpath(c, data);
}

JS_IMPL uint32_t
js_sizeof_map(uint32_t size)
{
	if (size <= 15) {
		return 1;
	} else if (size <= UINT16_MAX) {
		return 1 + sizeof(uint16_t);
	} else {
		return 1 + sizeof(uint32_t);
	}
}

JS_IMPL char *
js_encode_map(char *data, uint32_t size)
{
	if (size <= 15) {
		return js_store_u8(data, 0x80 | size);
	} else if (size <= UINT16_MAX) {
		data = js_store_u8(data, 0xde);
		data = js_store_u16(data, size);
		return data;
	} else {
		data = js_store_u8(data, 0xdf);
		data = js_store_u32(data, size);
		return data;
	}
}

JS_IMPL ptrdiff_t
js_check_map(const char *cur, const char *end)
{
	assert(cur < end);
	assert(js_typeof(*cur) == JS_MAP);
	uint8_t c = js_load_u8(&cur);
	if (js_likely((c & ~0xfU) == 0x80))
		return cur - end;

	assert(c >= 0xde && c <= 0xdf); /* must be checked above by js_typeof */
	uint32_t hsize = 2U << (c & 0x1); /* 0xde->2, 0xdf->4 */
	return hsize - (end - cur);
}

JS_IMPL uint32_t
js_decode_map(const char **data)
{
	uint8_t c = js_load_u8(data);
	switch (c) {
	case 0xde:
		return js_load_u16(data);
	case 0xdf:
		return js_load_u32(data);
	default:
		if (js_unlikely(c < 0x80 || c > 0x8f))
			js_unreachable();
		return c & 0xf;
	}
}

JS_IMPL uint32_t
js_sizeof_uint(uint64_t num)
{
	if (num <= 0x7f) {
		return 1;
	} else if (num <= UINT8_MAX) {
		return 1 + sizeof(uint8_t);
	} else if (num <= UINT16_MAX) {
		return 1 + sizeof(uint16_t);
	} else if (num <= UINT32_MAX) {
		return 1 + sizeof(uint32_t);
	} else {
		return 1 + sizeof(uint64_t);
	}
}

JS_IMPL uint32_t
js_sizeof_int(int64_t num)
{
	assert(num < 0);
	if (num >= -0x20) {
		return 1;
	} else if (num >= INT8_MIN && num <= INT8_MAX) {
		return 1 + sizeof(int8_t);
	} else if (num >= INT16_MIN && num <= UINT16_MAX) {
		return 1 + sizeof(int16_t);
	} else if (num >= INT32_MIN && num <= UINT32_MAX) {
		return 1 + sizeof(int32_t);
	} else {
		return 1 + sizeof(int64_t);
	}
}

JS_IMPL ptrdiff_t
js_check_uint(const char *cur, const char *end)
{
	assert(cur < end);
	assert(js_typeof(*cur) == JS_UINT);
	uint8_t c = js_load_u8(&cur);
	return js_parser_hint[c] - (end - cur);
}

JS_IMPL ptrdiff_t
js_check_int(const char *cur, const char *end)
{
	assert(cur < end);
	assert(js_typeof(*cur) == JS_INT);
	uint8_t c = js_load_u8(&cur);
	return js_parser_hint[c] - (end - cur);
}

JS_IMPL char *
js_encode_uint(char *data, uint64_t num)
{
	if (num <= 0x7f) {
		return js_store_u8(data, num);
	} else if (num <= UINT8_MAX) {
		data = js_store_u8(data, 0xcc);
		return js_store_u8(data, num);
	} else if (num <= UINT16_MAX) {
		data = js_store_u8(data, 0xcd);
		return js_store_u16(data, num);
	} else if (num <= UINT32_MAX) {
		data = js_store_u8(data, 0xce);
		return js_store_u32(data, num);
	} else {
		data = js_store_u8(data, 0xcf);
		return js_store_u64(data, num);
	}
}

JS_IMPL char *
js_encode_int(char *data, int64_t num)
{
	assert(num < 0);
	if (num >= -0x20) {
		return js_store_u8(data, 0xe0 | num);
	} else if (num >= INT8_MIN) {
		data = js_store_u8(data, 0xd0);
		return js_store_u8(data, num);
	} else if (num >= INT16_MIN) {
		data = js_store_u8(data, 0xd1);
		return js_store_u16(data, num);
	} else if (num >= INT32_MIN) {
		data = js_store_u8(data, 0xd2);
		return js_store_u32(data, num);
	} else {
		data = js_store_u8(data, 0xd3);
		return js_store_u64(data, num);
	}
}

JS_IMPL uint64_t
js_decode_uint(const char **data)
{
	uint8_t c = js_load_u8(data);
	switch (c) {
	case 0xcc:
		return js_load_u8(data);
	case 0xcd:
		return js_load_u16(data);
	case 0xce:
		return js_load_u32(data);
	case 0xcf:
		return js_load_u64(data);
	default:
		if (js_unlikely(c > 0x7f))
			js_unreachable();
		return c;
	}
}

JS_IMPL int
js_compare_uint(const char *data_a, const char *data_b)
{
	uint8_t ca = js_load_u8(&data_a);
	uint8_t cb = js_load_u8(&data_b);

	int r = ca - cb;
	if (r != 0)
		return r;

	if (ca <= 0x7f)
		return 0;

	uint64_t a, b;
	switch (ca & 0x3) {
	case 0xcc & 0x3:
		a = js_load_u8(&data_a);
		b = js_load_u8(&data_b);
		break;
	case 0xcd & 0x3:
		a = js_load_u16(&data_a);
		b = js_load_u16(&data_b);
		break;
	case 0xce & 0x3:
		a = js_load_u32(&data_a);
		b = js_load_u32(&data_b);
		break;
	case 0xcf & 0x3:
		a = js_load_u64(&data_a);
		b = js_load_u64(&data_b);
		return a < b ? -1 : a > b;
		break;
	default:
		js_unreachable();
	}

	int64_t v = (a - b);
	return (v > 0) - (v < 0);
}

JS_IMPL int64_t
js_decode_int(const char **data)
{
	uint8_t c = js_load_u8(data);
	switch (c) {
	case 0xd0:
		return (int8_t) js_load_u8(data);
	case 0xd1:
		return (int16_t) js_load_u16(data);
	case 0xd2:
		return (int32_t) js_load_u32(data);
	case 0xd3:
		return (int64_t) js_load_u64(data);
	default:
		if (js_unlikely(c < 0xe0))
			js_unreachable();
		return (int8_t) (c);
	}
}

JS_IMPL uint32_t
js_sizeof_float(float num)
{
	(void) num;
	return 1 + sizeof(float);
}

JS_IMPL uint32_t
js_sizeof_double(double num)
{
	(void) num;
	return 1 + sizeof(double);
}

JS_IMPL ptrdiff_t
js_check_float(const char *cur, const char *end)
{
	assert(cur < end);
	assert(js_typeof(*cur) == JS_FLOAT);
	return 1 + sizeof(float) - (end - cur);
}

JS_IMPL ptrdiff_t
js_check_double(const char *cur, const char *end)
{
	assert(cur < end);
	assert(js_typeof(*cur) == JS_DOUBLE);
	return 1 + sizeof(double) - (end - cur);
}

JS_IMPL char *
js_encode_float(char *data, float num)
{
	data = js_store_u8(data, 0xca);
	return js_store_float(data, num);
}

JS_IMPL char *
js_encode_double(char *data, double num)
{
	data = js_store_u8(data, 0xcb);
	return js_store_double(data, num);
}

JS_IMPL float
js_decode_float(const char **data)
{
	uint8_t c = js_load_u8(data);
	assert(c == 0xca);
	(void) c;
	return js_load_float(data);
}

JS_IMPL double
js_decode_double(const char **data)
{
	uint8_t c = js_load_u8(data);
	assert(c == 0xcb);
	(void) c;
	return js_load_double(data);
}

JS_IMPL uint32_t
js_sizeof_strl(uint32_t len)
{
	if (len <= 31) {
		return 1;
	} else if (len <= UINT8_MAX) {
		return 1 + sizeof(uint8_t);
	} else if (len <= UINT16_MAX) {
		return 1 + sizeof(uint16_t);
	} else {
		return 1 + sizeof(uint32_t);
	}
}

JS_IMPL uint32_t
js_sizeof_str(uint32_t len)
{
	return js_sizeof_strl(len) + len;
}

JS_IMPL uint32_t
js_sizeof_binl(uint32_t len)
{
	if (len <= UINT8_MAX) {
		return 1 + sizeof(uint8_t);
	} else if (len <= UINT16_MAX) {
		return 1 + sizeof(uint16_t);
	} else {
		return 1 + sizeof(uint32_t);
	}
}

JS_IMPL uint32_t
js_sizeof_bin(uint32_t len)
{
	return js_sizeof_binl(len) + len;
}

JS_IMPL char *
js_encode_strl(char *data, uint32_t len)
{
	if (len <= 31) {
		return js_store_u8(data, 0xa0 | (uint8_t) len);
	} else if (len <= UINT8_MAX) {
		data = js_store_u8(data, 0xd9);
		return js_store_u8(data, len);
	} else if (len <= UINT16_MAX) {
		data = js_store_u8(data, 0xda);
		return js_store_u16(data, len);
	} else {
		data = js_store_u8(data, 0xdb);
		return js_store_u32(data, len);
	}
}

JS_IMPL char *
js_encode_str(char *data, const char *str, uint32_t len)
{
	data = js_encode_strl(data, len);
	memcpy(data, str, len);
	return data + len;
}

JS_IMPL char *
js_encode_binl(char *data, uint32_t len)
{
	if (len <= UINT8_MAX) {
		data = js_store_u8(data, 0xc4);
		return js_store_u8(data, len);
	} else if (len <= UINT16_MAX) {
		data = js_store_u8(data, 0xc5);
		return js_store_u16(data, len);
	} else {
		data = js_store_u8(data, 0xc6);
		return js_store_u32(data, len);
	}
}

JS_IMPL char *
js_encode_bin(char *data, const char *str, uint32_t len)
{
	data = js_encode_binl(data, len);
	memcpy(data, str, len);
	return data + len;
}

JS_IMPL ptrdiff_t
js_check_strl(const char *cur, const char *end)
{
	assert(cur < end);
	assert(js_typeof(*cur) == JS_STR);

	uint8_t c = js_load_u8(&cur);
	if (js_likely(c & ~0x1f) == 0xa0)
		return cur - end;

	assert(c >= 0xd9 && c <= 0xdb); /* must be checked above by js_typeof */
	uint32_t hsize = 1U << (c & 0x3) >> 1; /* 0xd9->1, 0xda->2, 0xdb->4 */
	return hsize - (end - cur);
}

JS_IMPL ptrdiff_t
js_check_binl(const char *cur, const char *end)
{
	uint8_t c = js_load_u8(&cur);
	assert(cur < end);
	assert(js_typeof(c) == JS_BIN);

	assert(c >= 0xc4 && c <= 0xc6); /* must be checked above by js_typeof */
	uint32_t hsize = 1U << (c & 0x3); /* 0xc4->1, 0xc5->2, 0xc6->4 */
	return hsize - (end - cur);
}

JS_IMPL uint32_t
js_decode_strl(const char **data)
{
	uint8_t c = js_load_u8(data);
	switch (c) {
	case 0xd9:
		return js_load_u8(data);
	case 0xda:
		return js_load_u16(data);
	case 0xdb:
		return js_load_u32(data);
	default:
		if (js_unlikely(c < 0xa0 || c > 0xbf))
			js_unreachable();
		return c & 0x1f;
	}
}

JS_IMPL const char *
js_decode_str(const char **data, uint32_t *len)
{
	assert(len != NULL);

	*len = js_decode_strl(data);
	const char *str = *data;
	*data += *len;
	return str;
}

JS_IMPL uint32_t
js_decode_binl(const char **data)
{
	uint8_t c = js_load_u8(data);

	switch (c) {
	case 0xc4:
		return js_load_u8(data);
	case 0xc5:
		return js_load_u16(data);
	case 0xc6:
		return js_load_u32(data);
	default:
		js_unreachable();
	}
}

JS_IMPL const char *
js_decode_bin(const char **data, uint32_t *len)
{
	assert(len != NULL);

	*len = js_decode_binl(data);
	const char *str = *data;
	*data += *len;
	return str;
}

JS_IMPL uint32_t
js_decode_strbinl(const char **data)
{
	uint8_t c = js_load_u8(data);

	switch (c) {
	case 0xd9:
		return js_load_u8(data);
	case 0xda:
		return js_load_u16(data);
	case 0xdb:
		return js_load_u32(data);
	case 0xc4:
		return js_load_u8(data);
	case 0xc5:
		return js_load_u16(data);
	case 0xc6:
		return js_load_u32(data);
	default:
		if (js_unlikely(c < 0xa0 || c > 0xbf))
			js_unreachable();
		return c & 0x1f;
	}
}

JS_IMPL const char *
js_decode_strbin(const char **data, uint32_t *len)
{
	assert(len != NULL);

	*len = js_decode_strbinl(data);
	const char *str = *data;
	*data += *len;
	return str;
}

JS_IMPL uint32_t
js_sizeof_nil()
{
	return 1;
}

JS_IMPL char *
js_encode_nil(char *data)
{
	return js_store_u8(data, 0xc0);
}

JS_IMPL ptrdiff_t
js_check_nil(const char *cur, const char *end)
{
	assert(cur < end);
	assert(js_typeof(*cur) == JS_NIL);
	return 1 - (end - cur);
}

JS_IMPL void
js_decode_nil(const char **data)
{
	uint8_t c = js_load_u8(data);
	assert(c == 0xc0);
	(void) c;
}

JS_IMPL uint32_t
js_sizeof_bool(bool val)
{
	(void) val;
	return 1;
}

JS_IMPL char *
js_encode_bool(char *data, bool val)
{
	return js_store_u8(data, 0xc2 | (val & 1));
}

JS_IMPL ptrdiff_t
js_check_bool(const char *cur, const char *end)
{
	assert(cur < end);
	assert(js_typeof(*cur) == JS_BOOL);
	return 1 - (end - cur);
}

JS_IMPL bool
js_decode_bool(const char **data)
{
	uint8_t c = js_load_u8(data);
	switch (c) {
	case 0xc3:
		return true;
	case 0xc2:
		return false;
	default:
		js_unreachable();
	}
}

/** See js_parser_hint */
enum {
	JS_HINT = -32,
	JS_HINT_STR_8 = JS_HINT,
	JS_HINT_STR_16 = JS_HINT - 1,
	JS_HINT_STR_32 = JS_HINT - 2,
	JS_HINT_ARRAY_16 = JS_HINT - 3,
	JS_HINT_ARRAY_32 = JS_HINT - 4,
	JS_HINT_MAP_16 = JS_HINT - 5,
	JS_HINT_MAP_32 = JS_HINT - 6,
	JS_HINT_EXT_8 = JS_HINT - 7,
	JS_HINT_EXT_16 = JS_HINT - 8,
	JS_HINT_EXT_32 = JS_HINT - 9
};

JS_PROTO void
js_next_slowpath(const char **data, int k);

JS_IMPL void
js_next_slowpath(const char **data, int k)
{
	for (; k > 0; k--) {
		uint8_t c = js_load_u8(data);
		int l = js_parser_hint[c];
		if (js_likely(l >= 0)) {
			*data += l;
			continue;
		} else if (js_likely(l > JS_HINT)) {
			k -= l;
			continue;
		}

		uint32_t len;
		switch (l) {
		case JS_HINT_STR_8:
			/* JS_STR (8) */
			len = js_load_u8(data);
			*data += len;
			break;
		case JS_HINT_STR_16:
			/* JS_STR (16) */
			len = js_load_u16(data);
			*data += len;
			break;
		case JS_HINT_STR_32:
			/* JS_STR (32) */
			len = js_load_u32(data);
			*data += len;
			break;
		case JS_HINT_ARRAY_16:
			/* JS_ARRAY (16) */
			k += js_load_u16(data);
			break;
		case JS_HINT_ARRAY_32:
			/* JS_ARRAY (32) */
			k += js_load_u32(data);
			break;
		case JS_HINT_MAP_16:
			/* JS_MAP (16) */
			k += 2 * js_load_u16(data);
			break;
		case JS_HINT_MAP_32:
			/* JS_MAP (32) */
			k += 2 * js_load_u32(data);
			break;
		case JS_HINT_EXT_8:
			/* JS_EXT (8) */
			len = js_load_u8(data);
			js_load_u8(data);
			*data += len;
			break;
		case JS_HINT_EXT_16:
			/* JS_EXT (16) */
			len = js_load_u16(data);
			js_load_u8(data);
			*data += len;
			break;
		case JS_HINT_EXT_32:
			/* JS_EXT (32) */
			len = js_load_u32(data);
			js_load_u8(data);
			*data += len;
			break;
		default:
			js_unreachable();
		}
	}
}

JS_IMPL void
js_next(const char **data)
{
	int k = 1;
	for (; k > 0; k--) {
		uint8_t c = js_load_u8(data);
		int l = js_parser_hint[c];
		if (js_likely(l >= 0)) {
			*data += l;
			continue;
		} else if (js_likely(c == 0xd9)){
			/* JS_STR (8) */
			uint8_t len = js_load_u8(data);
			*data += len;
			continue;
		} else if (l > JS_HINT) {
			k -= l;
			continue;
		} else {
			*data -= sizeof(uint8_t);
			return js_next_slowpath(data, k);
		}
	}
}

JS_IMPL int
js_check(const char **data, const char *end)
{
	int k;
	for (k = 1; k > 0; k--) {
		if (js_unlikely(*data >= end))
			return 1;

		uint8_t c = js_load_u8(data);
		int l = js_parser_hint[c];
		if (js_likely(l >= 0)) {
			*data += l;
			continue;
		} else if (js_likely(l > JS_HINT)) {
			k -= l;
			continue;
		}

		uint32_t len;
		switch (l) {
		case JS_HINT_STR_8:
			/* JS_STR (8) */
			if (js_unlikely(*data + sizeof(uint8_t) > end))
				return 1;
			len = js_load_u8(data);
			*data += len;
			break;
		case JS_HINT_STR_16:
			/* JS_STR (16) */
			if (js_unlikely(*data + sizeof(uint16_t) > end))
				return 1;
			len = js_load_u16(data);
			*data += len;
			break;
		case JS_HINT_STR_32:
			/* JS_STR (32) */
			if (js_unlikely(*data + sizeof(uint32_t) > end))
				return 1;
			len = js_load_u32(data);
			*data += len;
			break;
		case JS_HINT_ARRAY_16:
			/* JS_ARRAY (16) */
			if (js_unlikely(*data + sizeof(uint16_t) > end))
				return 1;
			k += js_load_u16(data);
			break;
		case JS_HINT_ARRAY_32:
			/* JS_ARRAY (32) */
			if (js_unlikely(*data + sizeof(uint32_t) > end))
				return 1;
			k += js_load_u32(data);
			break;
		case JS_HINT_MAP_16:
			/* JS_MAP (16) */
			if (js_unlikely(*data + sizeof(uint16_t) > end))
				return false;
			k += 2 * js_load_u16(data);
			break;
		case JS_HINT_MAP_32:
			/* JS_MAP (32) */
			if (js_unlikely(*data + sizeof(uint32_t) > end))
				return 1;
			k += 2 * js_load_u32(data);
			break;
		case JS_HINT_EXT_8:
			/* JS_EXT (8) */
			if (js_unlikely(*data + sizeof(uint8_t) + 1 > end))
				return 1;
			len = js_load_u8(data);
			js_load_u8(data);
			*data += len;
			break;
		case JS_HINT_EXT_16:
			/* JS_EXT (16) */
			if (js_unlikely(*data + sizeof(uint16_t) + 1 > end))
				return 1;
			len = js_load_u16(data);
			js_load_u8(data);
			*data += len;
			break;
		case JS_HINT_EXT_32:
			/* JS_EXT (32) */
			if (js_unlikely(*data + sizeof(uint32_t) + 1 > end))
				return 1;
		        len = js_load_u32(data);
			js_load_u8(data);
			*data += len;
			break;
		default:
			js_unreachable();
		}
	}

	if (js_unlikely(*data > end))
		return 1;

	return 0;
}

JS_IMPL size_t
js_vformat(char *data, size_t data_size, const char *format, va_list vl)
{
	size_t result = 0;
	const char *f = NULL;

	for (f = format; *f; f++) {
		if (f[0] == '[') {
			uint32_t size = 0;
			int level = 1;
			const char *e = NULL;

			for (e = f + 1; level && *e; e++) {
				if (*e == '[' || *e == '{') {
					if (level == 1)
						size++;
					level++;
				} else if (*e == ']' || *e == '}') {
					level--;
					/* opened '[' must be closed by ']' */
					assert(level || *e == ']');
				} else if (*e == '%') {
					if (e[1] == '%')
						e++;
					else if (level == 1)
						size++;
				} else if (*e == 'N' && e[1] == 'I'
					   && e[2] == 'L' && level == 1) {
					size++;
				}
			}
			/* opened '[' must be closed */
			assert(level == 0);
			result += js_sizeof_array(size);
			if (result <= data_size)
				data = js_encode_array(data, size);
		} else if (f[0] == '{') {
			uint32_t count = 0;
			int level = 1;
			const char *e = NULL;

			for (e = f + 1; level && *e; e++) {
				if (*e == '[' || *e == '{') {
					if (level == 1)
						count++;
					level++;
				} else if (*e == ']' || *e == '}') {
					level--;
					/* opened '{' must be closed by '}' */
					assert(level || *e == '}');
				} else if (*e == '%') {
					if (e[1] == '%')
						e++;
					else if (level == 1)
						count++;
				} else if (*e == 'N' && e[1] == 'I'
					   && e[2] == 'L' && level == 1) {
					count++;
				}
			}
			/* opened '{' must be closed */
			assert(level == 0);
			/* since map is a pair list, count must be even */
			assert(count % 2 == 0);
			uint32_t size = count / 2;
			result += js_sizeof_map(size);
			if (result <= data_size)
				data = js_encode_map(data, size);
		} else if (f[0] == '%') {
			f++;
			assert(f[0]);
			int64_t int_value = 0;
			int int_status = 0; /* 1 - signed, 2 - unsigned */

			if (f[0] == 'd' || f[0] == 'i') {
				int_value = va_arg(vl, int);
				int_status = 1;
			} else if (f[0] == 'u') {
				int_value = va_arg(vl, unsigned int);
				int_status = 2;
			} else if (f[0] == 's') {
				const char *str = va_arg(vl, const char *);
				uint32_t len = (uint32_t)strlen(str);
				result += js_sizeof_str(len);
				if (result <= data_size)
					data = js_encode_str(data, str, len);
			} else if (f[0] == '.' && f[1] == '*' && f[2] == 's') {
				uint32_t len = va_arg(vl, uint32_t);
				const char *str = va_arg(vl, const char *);
				result += js_sizeof_str(len);
				if (result <= data_size)
					data = js_encode_str(data, str, len);
				f += 2;
			} else if(f[0] == 'f') {
				float v = (float)va_arg(vl, double);
				result += js_sizeof_float(v);
				if (result <= data_size)
					data = js_encode_float(data, v);
			} else if(f[0] == 'l' && f[1] == 'f') {
				double v = va_arg(vl, double);
				result += js_sizeof_double(v);
				if (result <= data_size)
					data = js_encode_double(data, v);
				f++;
			} else if(f[0] == 'b') {
				bool v = (bool)va_arg(vl, int);
				result += js_sizeof_bool(v);
				if (result <= data_size)
					data = js_encode_bool(data, v);
			} else if (f[0] == 'l'
				   && (f[1] == 'd' || f[1] == 'i')) {
				int_value = va_arg(vl, long);
				int_status = 1;
				f++;
			} else if (f[0] == 'l' && f[1] == 'u') {
				int_value = va_arg(vl, unsigned long);
				int_status = 2;
				f++;
			} else if (f[0] == 'l' && f[1] == 'l'
				   && (f[2] == 'd' || f[2] == 'i')) {
				int_value = va_arg(vl, long long);
				int_status = 1;
				f += 2;
			} else if (f[0] == 'l' && f[1] == 'l' && f[2] == 'u') {
				int_value = va_arg(vl, unsigned long long);
				int_status = 2;
				f += 2;
			} else if (f[0] == 'h'
				   && (f[1] == 'd' || f[1] == 'i')) {
				int_value = va_arg(vl, int);
				int_status = 1;
				f++;
			} else if (f[0] == 'h' && f[1] == 'u') {
				int_value = va_arg(vl, unsigned int);
				int_status = 2;
				f++;
			} else if (f[0] == 'h' && f[1] == 'h'
				   && (f[2] == 'd' || f[2] == 'i')) {
				int_value = va_arg(vl, int);
				int_status = 1;
				f += 2;
			} else if (f[0] == 'h' && f[1] == 'h' && f[2] == 'u') {
				int_value = va_arg(vl, unsigned int);
				int_status = 2;
				f += 2;
			} else if (f[0] != '%') {
				/* unexpected format specifier */
				assert(false);
			}

			if (int_status == 1 && int_value < 0) {
				result += js_sizeof_int(int_value);
				if (result <= data_size)
					data = js_encode_int(data, int_value);
			} else if(int_status) {
				result += js_sizeof_uint(int_value);
				if (result <= data_size)
					data = js_encode_uint(data, int_value);
			}
		} else if (f[0] == 'N' && f[1] == 'I' && f[2] == 'L') {
			result += js_sizeof_nil();
			if (result <= data_size)
				data = js_encode_nil(data);
			f += 2;
		}
	}
	return result;
}

JS_IMPL size_t
js_format(char *data, size_t data_size, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	size_t res = js_vformat(data, data_size, format, args);
	va_end(args);
	return res;
}

JS_PROTO int
js_fprint_internal(FILE *file, const char **data);

JS_IMPL int
js_fprint_internal(FILE *file, const char **data)
{
#define _CHECK_RC(exp) do { if (js_unlikely((exp) < 0)) return -1; } while(0)
	switch (js_typeof(**data)) {
	case JS_NIL:
		js_decode_nil(data);
		_CHECK_RC(fputs("null", file));
		break;
	case JS_UINT:
		_CHECK_RC(fprintf(file, "%llu", (unsigned long long)
			      js_decode_uint(data)));
		break;
	case JS_INT:
		_CHECK_RC(fprintf(file, "%lld", (long long)
			      js_decode_int(data)));
		break;
	case JS_STR:
	case JS_BIN:
	{
		uint32_t len = js_typeof(**data) == JS_STR ?
			js_decode_strl(data) : js_decode_binl(data);
		_CHECK_RC(fputc('"', file));
		const char *s;
		for (s = *data; s < *data + len; s++) {
			unsigned char c = (unsigned char ) *s;
			if (c < 128 && js_char2escape[c] != NULL) {
				/* Escape character */
				_CHECK_RC(fputs(js_char2escape[c], file));
			} else {
				_CHECK_RC(fputc(c, file));
			}
		}
		_CHECK_RC(fputc('"', file));
		*data += len;
		break;
	}
	case JS_ARRAY:
	{
		uint32_t size = js_decode_array(data);
		_CHECK_RC(fputc('[', file));
		uint32_t i;
		for (i = 0; i < size; i++) {
			if (i)
				_CHECK_RC(fputs(", ", file));
			_CHECK_RC(js_fprint_internal(file, data));
		}
		_CHECK_RC(fputc(']', file));
		break;
	}
	case JS_MAP:
	{
		uint32_t size = js_decode_map(data);
		_CHECK_RC(fputc('{', file));
		uint32_t i;
		for (i = 0; i < size; i++) {
			if (i)
				_CHECK_RC(fprintf(file, ", "));
			_CHECK_RC(js_fprint_internal(file, data));
			_CHECK_RC(fputs(": ", file));
			_CHECK_RC(js_fprint_internal(file, data));
		}
		_CHECK_RC(fputc('}', file));
		break;
	}
	case JS_BOOL:
		_CHECK_RC(fputs(js_decode_bool(data) ? "true" : "false", file));
		break;
	case JS_FLOAT:
		_CHECK_RC(fprintf(file, "%g", js_decode_float(data)));
		break;
	case JS_DOUBLE:
		_CHECK_RC(fprintf(file, "%lg", js_decode_double(data)));
		break;
	case JS_EXT:
		js_next(data);
		_CHECK_RC(fputs("undefined", file));
		break;
	default:
		js_unreachable();
		return -1;
	}
	return 0;
#undef _CHECK_RC
}

JS_IMPL int
js_fprint(FILE *file, const char *data)
{
	if (!file)
		file = stdout;
	int res = js_fprint_internal(file, &data);
	return res;
}

/** \endcond */

/*
 * }}}
 */

/*
 * {{{ Implementation: parser tables
 */

/** \cond false */

#if defined(JS_SOURCE)

/**
 * This lookup table used by js_sizeof() to determine enum js_type by the first
 * byte of JSONPack element.
 */
const enum js_type js_type_hint[256]= {
	/* {{{ JS_UINT (fixed) */
	/* 0x00 */ JS_UINT,
	/* 0x01 */ JS_UINT,
	/* 0x02 */ JS_UINT,
	/* 0x03 */ JS_UINT,
	/* 0x04 */ JS_UINT,
	/* 0x05 */ JS_UINT,
	/* 0x06 */ JS_UINT,
	/* 0x07 */ JS_UINT,
	/* 0x08 */ JS_UINT,
	/* 0x09 */ JS_UINT,
	/* 0x0a */ JS_UINT,
	/* 0x0b */ JS_UINT,
	/* 0x0c */ JS_UINT,
	/* 0x0d */ JS_UINT,
	/* 0x0e */ JS_UINT,
	/* 0x0f */ JS_UINT,
	/* 0x10 */ JS_UINT,
	/* 0x11 */ JS_UINT,
	/* 0x12 */ JS_UINT,
	/* 0x13 */ JS_UINT,
	/* 0x14 */ JS_UINT,
	/* 0x15 */ JS_UINT,
	/* 0x16 */ JS_UINT,
	/* 0x17 */ JS_UINT,
	/* 0x18 */ JS_UINT,
	/* 0x19 */ JS_UINT,
	/* 0x1a */ JS_UINT,
	/* 0x1b */ JS_UINT,
	/* 0x1c */ JS_UINT,
	/* 0x1d */ JS_UINT,
	/* 0x1e */ JS_UINT,
	/* 0x1f */ JS_UINT,
	/* 0x20 */ JS_UINT,
	/* 0x21 */ JS_UINT,
	/* 0x22 */ JS_UINT,
	/* 0x23 */ JS_UINT,
	/* 0x24 */ JS_UINT,
	/* 0x25 */ JS_UINT,
	/* 0x26 */ JS_UINT,
	/* 0x27 */ JS_UINT,
	/* 0x28 */ JS_UINT,
	/* 0x29 */ JS_UINT,
	/* 0x2a */ JS_UINT,
	/* 0x2b */ JS_UINT,
	/* 0x2c */ JS_UINT,
	/* 0x2d */ JS_UINT,
	/* 0x2e */ JS_UINT,
	/* 0x2f */ JS_UINT,
	/* 0x30 */ JS_UINT,
	/* 0x31 */ JS_UINT,
	/* 0x32 */ JS_UINT,
	/* 0x33 */ JS_UINT,
	/* 0x34 */ JS_UINT,
	/* 0x35 */ JS_UINT,
	/* 0x36 */ JS_UINT,
	/* 0x37 */ JS_UINT,
	/* 0x38 */ JS_UINT,
	/* 0x39 */ JS_UINT,
	/* 0x3a */ JS_UINT,
	/* 0x3b */ JS_UINT,
	/* 0x3c */ JS_UINT,
	/* 0x3d */ JS_UINT,
	/* 0x3e */ JS_UINT,
	/* 0x3f */ JS_UINT,
	/* 0x40 */ JS_UINT,
	/* 0x41 */ JS_UINT,
	/* 0x42 */ JS_UINT,
	/* 0x43 */ JS_UINT,
	/* 0x44 */ JS_UINT,
	/* 0x45 */ JS_UINT,
	/* 0x46 */ JS_UINT,
	/* 0x47 */ JS_UINT,
	/* 0x48 */ JS_UINT,
	/* 0x49 */ JS_UINT,
	/* 0x4a */ JS_UINT,
	/* 0x4b */ JS_UINT,
	/* 0x4c */ JS_UINT,
	/* 0x4d */ JS_UINT,
	/* 0x4e */ JS_UINT,
	/* 0x4f */ JS_UINT,
	/* 0x50 */ JS_UINT,
	/* 0x51 */ JS_UINT,
	/* 0x52 */ JS_UINT,
	/* 0x53 */ JS_UINT,
	/* 0x54 */ JS_UINT,
	/* 0x55 */ JS_UINT,
	/* 0x56 */ JS_UINT,
	/* 0x57 */ JS_UINT,
	/* 0x58 */ JS_UINT,
	/* 0x59 */ JS_UINT,
	/* 0x5a */ JS_UINT,
	/* 0x5b */ JS_UINT,
	/* 0x5c */ JS_UINT,
	/* 0x5d */ JS_UINT,
	/* 0x5e */ JS_UINT,
	/* 0x5f */ JS_UINT,
	/* 0x60 */ JS_UINT,
	/* 0x61 */ JS_UINT,
	/* 0x62 */ JS_UINT,
	/* 0x63 */ JS_UINT,
	/* 0x64 */ JS_UINT,
	/* 0x65 */ JS_UINT,
	/* 0x66 */ JS_UINT,
	/* 0x67 */ JS_UINT,
	/* 0x68 */ JS_UINT,
	/* 0x69 */ JS_UINT,
	/* 0x6a */ JS_UINT,
	/* 0x6b */ JS_UINT,
	/* 0x6c */ JS_UINT,
	/* 0x6d */ JS_UINT,
	/* 0x6e */ JS_UINT,
	/* 0x6f */ JS_UINT,
	/* 0x70 */ JS_UINT,
	/* 0x71 */ JS_UINT,
	/* 0x72 */ JS_UINT,
	/* 0x73 */ JS_UINT,
	/* 0x74 */ JS_UINT,
	/* 0x75 */ JS_UINT,
	/* 0x76 */ JS_UINT,
	/* 0x77 */ JS_UINT,
	/* 0x78 */ JS_UINT,
	/* 0x79 */ JS_UINT,
	/* 0x7a */ JS_UINT,
	/* 0x7b */ JS_UINT,
	/* 0x7c */ JS_UINT,
	/* 0x7d */ JS_UINT,
	/* 0x7e */ JS_UINT,
	/* 0x7f */ JS_UINT,
	/* }}} */

	/* {{{ JS_MAP (fixed) */
	/* 0x80 */ JS_MAP,
	/* 0x81 */ JS_MAP,
	/* 0x82 */ JS_MAP,
	/* 0x83 */ JS_MAP,
	/* 0x84 */ JS_MAP,
	/* 0x85 */ JS_MAP,
	/* 0x86 */ JS_MAP,
	/* 0x87 */ JS_MAP,
	/* 0x88 */ JS_MAP,
	/* 0x89 */ JS_MAP,
	/* 0x8a */ JS_MAP,
	/* 0x8b */ JS_MAP,
	/* 0x8c */ JS_MAP,
	/* 0x8d */ JS_MAP,
	/* 0x8e */ JS_MAP,
	/* 0x8f */ JS_MAP,
	/* }}} */

	/* {{{ JS_ARRAY (fixed) */
	/* 0x90 */ JS_ARRAY,
	/* 0x91 */ JS_ARRAY,
	/* 0x92 */ JS_ARRAY,
	/* 0x93 */ JS_ARRAY,
	/* 0x94 */ JS_ARRAY,
	/* 0x95 */ JS_ARRAY,
	/* 0x96 */ JS_ARRAY,
	/* 0x97 */ JS_ARRAY,
	/* 0x98 */ JS_ARRAY,
	/* 0x99 */ JS_ARRAY,
	/* 0x9a */ JS_ARRAY,
	/* 0x9b */ JS_ARRAY,
	/* 0x9c */ JS_ARRAY,
	/* 0x9d */ JS_ARRAY,
	/* 0x9e */ JS_ARRAY,
	/* 0x9f */ JS_ARRAY,
	/* }}} */

	/* {{{ JS_STR (fixed) */
	/* 0xa0 */ JS_STR,
	/* 0xa1 */ JS_STR,
	/* 0xa2 */ JS_STR,
	/* 0xa3 */ JS_STR,
	/* 0xa4 */ JS_STR,
	/* 0xa5 */ JS_STR,
	/* 0xa6 */ JS_STR,
	/* 0xa7 */ JS_STR,
	/* 0xa8 */ JS_STR,
	/* 0xa9 */ JS_STR,
	/* 0xaa */ JS_STR,
	/* 0xab */ JS_STR,
	/* 0xac */ JS_STR,
	/* 0xad */ JS_STR,
	/* 0xae */ JS_STR,
	/* 0xaf */ JS_STR,
	/* 0xb0 */ JS_STR,
	/* 0xb1 */ JS_STR,
	/* 0xb2 */ JS_STR,
	/* 0xb3 */ JS_STR,
	/* 0xb4 */ JS_STR,
	/* 0xb5 */ JS_STR,
	/* 0xb6 */ JS_STR,
	/* 0xb7 */ JS_STR,
	/* 0xb8 */ JS_STR,
	/* 0xb9 */ JS_STR,
	/* 0xba */ JS_STR,
	/* 0xbb */ JS_STR,
	/* 0xbc */ JS_STR,
	/* 0xbd */ JS_STR,
	/* 0xbe */ JS_STR,
	/* 0xbf */ JS_STR,
	/* }}} */

	/* {{{ JS_NIL, JS_BOOL */
	/* 0xc0 */ JS_NIL,
	/* 0xc1 */ JS_EXT, /* never used */
	/* 0xc2 */ JS_BOOL,
	/* 0xc3 */ JS_BOOL,
	/* }}} */

	/* {{{ JS_BIN */
	/* 0xc4 */ JS_BIN,   /* JS_BIN(8)  */
	/* 0xc5 */ JS_BIN,   /* JS_BIN(16) */
	/* 0xc6 */ JS_BIN,   /* JS_BIN(32) */
	/* }}} */

	/* {{{ JS_EXT */
	/* 0xc7 */ JS_EXT,
	/* 0xc8 */ JS_EXT,
	/* 0xc9 */ JS_EXT,
	/* }}} */

	/* {{{ JS_FLOAT, JS_DOUBLE */
	/* 0xca */ JS_FLOAT,
	/* 0xcb */ JS_DOUBLE,
	/* }}} */

	/* {{{ JS_UINT */
	/* 0xcc */ JS_UINT,
	/* 0xcd */ JS_UINT,
	/* 0xce */ JS_UINT,
	/* 0xcf */ JS_UINT,
	/* }}} */

	/* {{{ JS_INT */
	/* 0xd0 */ JS_INT,   /* JS_INT (8)  */
	/* 0xd1 */ JS_INT,   /* JS_INT (16) */
	/* 0xd2 */ JS_INT,   /* JS_INT (32) */
	/* 0xd3 */ JS_INT,   /* JS_INT (64) */
	/* }}} */

	/* {{{ JS_EXT */
	/* 0xd4 */ JS_EXT,   /* JS_INT (8)    */
	/* 0xd5 */ JS_EXT,   /* JS_INT (16)   */
	/* 0xd6 */ JS_EXT,   /* JS_INT (32)   */
	/* 0xd7 */ JS_EXT,   /* JS_INT (64)   */
	/* 0xd8 */ JS_EXT,   /* JS_INT (127)  */
	/* }}} */

	/* {{{ JS_STR */
	/* 0xd9 */ JS_STR,   /* JS_STR(8)  */
	/* 0xda */ JS_STR,   /* JS_STR(16) */
	/* 0xdb */ JS_STR,   /* JS_STR(32) */
	/* }}} */

	/* {{{ JS_ARRAY */
	/* 0xdc */ JS_ARRAY, /* JS_ARRAY(16)  */
	/* 0xdd */ JS_ARRAY, /* JS_ARRAY(32)  */
	/* }}} */

	/* {{{ JS_MAP */
	/* 0xde */ JS_MAP,   /* JS_MAP (16) */
	/* 0xdf */ JS_MAP,   /* JS_MAP (32) */
	/* }}} */

	/* {{{ JS_INT */
	/* 0xe0 */ JS_INT,
	/* 0xe1 */ JS_INT,
	/* 0xe2 */ JS_INT,
	/* 0xe3 */ JS_INT,
	/* 0xe4 */ JS_INT,
	/* 0xe5 */ JS_INT,
	/* 0xe6 */ JS_INT,
	/* 0xe7 */ JS_INT,
	/* 0xe8 */ JS_INT,
	/* 0xe9 */ JS_INT,
	/* 0xea */ JS_INT,
	/* 0xeb */ JS_INT,
	/* 0xec */ JS_INT,
	/* 0xed */ JS_INT,
	/* 0xee */ JS_INT,
	/* 0xef */ JS_INT,
	/* 0xf0 */ JS_INT,
	/* 0xf1 */ JS_INT,
	/* 0xf2 */ JS_INT,
	/* 0xf3 */ JS_INT,
	/* 0xf4 */ JS_INT,
	/* 0xf5 */ JS_INT,
	/* 0xf6 */ JS_INT,
	/* 0xf7 */ JS_INT,
	/* 0xf8 */ JS_INT,
	/* 0xf9 */ JS_INT,
	/* 0xfa */ JS_INT,
	/* 0xfb */ JS_INT,
	/* 0xfc */ JS_INT,
	/* 0xfd */ JS_INT,
	/* 0xfe */ JS_INT,
	/* 0xff */ JS_INT
	/* }}} */
};

/**
 * This lookup table used by js_next() and js_check() to determine
 * size of JSONPack element by its first byte.
 * A positive value contains size of the element (excluding the first byte).
 * A negative value means the element is compound (e.g. array or map)
 * of size (-n).
 * JS_HINT_* values used for special cases handled by switch() statement.
 */
const int8_t js_parser_hint[256] = {
	/* {{{ JS_UINT(fixed) **/
	/* 0x00 */ 0,
	/* 0x01 */ 0,
	/* 0x02 */ 0,
	/* 0x03 */ 0,
	/* 0x04 */ 0,
	/* 0x05 */ 0,
	/* 0x06 */ 0,
	/* 0x07 */ 0,
	/* 0x08 */ 0,
	/* 0x09 */ 0,
	/* 0x0a */ 0,
	/* 0x0b */ 0,
	/* 0x0c */ 0,
	/* 0x0d */ 0,
	/* 0x0e */ 0,
	/* 0x0f */ 0,
	/* 0x10 */ 0,
	/* 0x11 */ 0,
	/* 0x12 */ 0,
	/* 0x13 */ 0,
	/* 0x14 */ 0,
	/* 0x15 */ 0,
	/* 0x16 */ 0,
	/* 0x17 */ 0,
	/* 0x18 */ 0,
	/* 0x19 */ 0,
	/* 0x1a */ 0,
	/* 0x1b */ 0,
	/* 0x1c */ 0,
	/* 0x1d */ 0,
	/* 0x1e */ 0,
	/* 0x1f */ 0,
	/* 0x20 */ 0,
	/* 0x21 */ 0,
	/* 0x22 */ 0,
	/* 0x23 */ 0,
	/* 0x24 */ 0,
	/* 0x25 */ 0,
	/* 0x26 */ 0,
	/* 0x27 */ 0,
	/* 0x28 */ 0,
	/* 0x29 */ 0,
	/* 0x2a */ 0,
	/* 0x2b */ 0,
	/* 0x2c */ 0,
	/* 0x2d */ 0,
	/* 0x2e */ 0,
	/* 0x2f */ 0,
	/* 0x30 */ 0,
	/* 0x31 */ 0,
	/* 0x32 */ 0,
	/* 0x33 */ 0,
	/* 0x34 */ 0,
	/* 0x35 */ 0,
	/* 0x36 */ 0,
	/* 0x37 */ 0,
	/* 0x38 */ 0,
	/* 0x39 */ 0,
	/* 0x3a */ 0,
	/* 0x3b */ 0,
	/* 0x3c */ 0,
	/* 0x3d */ 0,
	/* 0x3e */ 0,
	/* 0x3f */ 0,
	/* 0x40 */ 0,
	/* 0x41 */ 0,
	/* 0x42 */ 0,
	/* 0x43 */ 0,
	/* 0x44 */ 0,
	/* 0x45 */ 0,
	/* 0x46 */ 0,
	/* 0x47 */ 0,
	/* 0x48 */ 0,
	/* 0x49 */ 0,
	/* 0x4a */ 0,
	/* 0x4b */ 0,
	/* 0x4c */ 0,
	/* 0x4d */ 0,
	/* 0x4e */ 0,
	/* 0x4f */ 0,
	/* 0x50 */ 0,
	/* 0x51 */ 0,
	/* 0x52 */ 0,
	/* 0x53 */ 0,
	/* 0x54 */ 0,
	/* 0x55 */ 0,
	/* 0x56 */ 0,
	/* 0x57 */ 0,
	/* 0x58 */ 0,
	/* 0x59 */ 0,
	/* 0x5a */ 0,
	/* 0x5b */ 0,
	/* 0x5c */ 0,
	/* 0x5d */ 0,
	/* 0x5e */ 0,
	/* 0x5f */ 0,
	/* 0x60 */ 0,
	/* 0x61 */ 0,
	/* 0x62 */ 0,
	/* 0x63 */ 0,
	/* 0x64 */ 0,
	/* 0x65 */ 0,
	/* 0x66 */ 0,
	/* 0x67 */ 0,
	/* 0x68 */ 0,
	/* 0x69 */ 0,
	/* 0x6a */ 0,
	/* 0x6b */ 0,
	/* 0x6c */ 0,
	/* 0x6d */ 0,
	/* 0x6e */ 0,
	/* 0x6f */ 0,
	/* 0x70 */ 0,
	/* 0x71 */ 0,
	/* 0x72 */ 0,
	/* 0x73 */ 0,
	/* 0x74 */ 0,
	/* 0x75 */ 0,
	/* 0x76 */ 0,
	/* 0x77 */ 0,
	/* 0x78 */ 0,
	/* 0x79 */ 0,
	/* 0x7a */ 0,
	/* 0x7b */ 0,
	/* 0x7c */ 0,
	/* 0x7d */ 0,
	/* 0x7e */ 0,
	/* 0x7f */ 0,
	/* }}} */

	/* {{{ JS_MAP (fixed) */
	/* 0x80 */  0, /* empty map - just skip one byte */
	/* 0x81 */ -2, /* 2 elements follow */
	/* 0x82 */ -4,
	/* 0x83 */ -6,
	/* 0x84 */ -8,
	/* 0x85 */ -10,
	/* 0x86 */ -12,
	/* 0x87 */ -14,
	/* 0x88 */ -16,
	/* 0x89 */ -18,
	/* 0x8a */ -20,
	/* 0x8b */ -22,
	/* 0x8c */ -24,
	/* 0x8d */ -26,
	/* 0x8e */ -28,
	/* 0x8f */ -30,
	/* }}} */

	/* {{{ JS_ARRAY (fixed) */
	/* 0x90 */  0,  /* empty array - just skip one byte */
	/* 0x91 */ -1,  /* 1 element follows */
	/* 0x92 */ -2,
	/* 0x93 */ -3,
	/* 0x94 */ -4,
	/* 0x95 */ -5,
	/* 0x96 */ -6,
	/* 0x97 */ -7,
	/* 0x98 */ -8,
	/* 0x99 */ -9,
	/* 0x9a */ -10,
	/* 0x9b */ -11,
	/* 0x9c */ -12,
	/* 0x9d */ -13,
	/* 0x9e */ -14,
	/* 0x9f */ -15,
	/* }}} */

	/* {{{ JS_STR (fixed) */
	/* 0xa0 */ 0,
	/* 0xa1 */ 1,
	/* 0xa2 */ 2,
	/* 0xa3 */ 3,
	/* 0xa4 */ 4,
	/* 0xa5 */ 5,
	/* 0xa6 */ 6,
	/* 0xa7 */ 7,
	/* 0xa8 */ 8,
	/* 0xa9 */ 9,
	/* 0xaa */ 10,
	/* 0xab */ 11,
	/* 0xac */ 12,
	/* 0xad */ 13,
	/* 0xae */ 14,
	/* 0xaf */ 15,
	/* 0xb0 */ 16,
	/* 0xb1 */ 17,
	/* 0xb2 */ 18,
	/* 0xb3 */ 19,
	/* 0xb4 */ 20,
	/* 0xb5 */ 21,
	/* 0xb6 */ 22,
	/* 0xb7 */ 23,
	/* 0xb8 */ 24,
	/* 0xb9 */ 25,
	/* 0xba */ 26,
	/* 0xbb */ 27,
	/* 0xbc */ 28,
	/* 0xbd */ 29,
	/* 0xbe */ 30,
	/* 0xbf */ 31,
	/* }}} */

	/* {{{ JS_NIL, JS_BOOL */
	/* 0xc0 */ 0, /* JS_NIL */
	/* 0xc1 */ 0, /* never used */
	/* 0xc2 */ 0, /* JS_BOOL*/
	/* 0xc3 */ 0, /* JS_BOOL*/
	/* }}} */

	/* {{{ JS_BIN */
	/* 0xc4 */ JS_HINT_STR_8,  /* JS_BIN (8)  */
	/* 0xc5 */ JS_HINT_STR_16, /* JS_BIN (16) */
	/* 0xc6 */ JS_HINT_STR_32, /* JS_BIN (32) */
	/* }}} */

	/* {{{ JS_EXT */
	/* 0xc7 */ JS_HINT_EXT_8,    /* JS_EXT (8)  */
	/* 0xc8 */ JS_HINT_EXT_16,   /* JS_EXT (16) */
	/* 0xc9 */ JS_HINT_EXT_32,   /* JS_EXT (32) */
	/* }}} */

	/* {{{ JS_FLOAT, JS_DOUBLE */
	/* 0xca */ sizeof(float),    /* JS_FLOAT */
	/* 0xcb */ sizeof(double),   /* JS_DOUBLE */
	/* }}} */

	/* {{{ JS_UINT */
	/* 0xcc */ sizeof(uint8_t),  /* JS_UINT (8)  */
	/* 0xcd */ sizeof(uint16_t), /* JS_UINT (16) */
	/* 0xce */ sizeof(uint32_t), /* JS_UINT (32) */
	/* 0xcf */ sizeof(uint64_t), /* JS_UINT (64) */
	/* }}} */

	/* {{{ JS_INT */
	/* 0xd0 */ sizeof(uint8_t),  /* JS_INT (8)  */
	/* 0xd1 */ sizeof(uint16_t), /* JS_INT (8)  */
	/* 0xd2 */ sizeof(uint32_t), /* JS_INT (8)  */
	/* 0xd3 */ sizeof(uint64_t), /* JS_INT (8)  */
	/* }}} */

	/* {{{ JS_EXT (fixext) */
	/* 0xd4 */ 2,  /* JS_EXT (fixext 8)   */
	/* 0xd5 */ 3,  /* JS_EXT (fixext 16)  */
	/* 0xd6 */ 5,  /* JS_EXT (fixext 32)  */
	/* 0xd7 */ 9,  /* JS_EXT (fixext 64)  */
	/* 0xd8 */ 17, /* JS_EXT (fixext 128) */
	/* }}} */

	/* {{{ JS_STR */
	/* 0xd9 */ JS_HINT_STR_8,      /* JS_STR (8) */
	/* 0xda */ JS_HINT_STR_16,     /* JS_STR (16) */
	/* 0xdb */ JS_HINT_STR_32,     /* JS_STR (32) */
	/* }}} */

	/* {{{ JS_ARRAY */
	/* 0xdc */ JS_HINT_ARRAY_16,   /* JS_ARRAY (16) */
	/* 0xdd */ JS_HINT_ARRAY_32,   /* JS_ARRAY (32) */
	/* }}} */

	/* {{{ JS_MAP */
	/* 0xde */ JS_HINT_MAP_16,     /* JS_MAP (16) */
	/* 0xdf */ JS_HINT_MAP_32,     /* JS_MAP (32) */
	/* }}} */

	/* {{{ JS_INT (fixed) */
	/* 0xe0 */ 0,
	/* 0xe1 */ 0,
	/* 0xe2 */ 0,
	/* 0xe3 */ 0,
	/* 0xe4 */ 0,
	/* 0xe5 */ 0,
	/* 0xe6 */ 0,
	/* 0xe7 */ 0,
	/* 0xe8 */ 0,
	/* 0xe9 */ 0,
	/* 0xea */ 0,
	/* 0xeb */ 0,
	/* 0xec */ 0,
	/* 0xed */ 0,
	/* 0xee */ 0,
	/* 0xef */ 0,
	/* 0xf0 */ 0,
	/* 0xf1 */ 0,
	/* 0xf2 */ 0,
	/* 0xf3 */ 0,
	/* 0xf4 */ 0,
	/* 0xf5 */ 0,
	/* 0xf6 */ 0,
	/* 0xf7 */ 0,
	/* 0xf8 */ 0,
	/* 0xf9 */ 0,
	/* 0xfa */ 0,
	/* 0xfb */ 0,
	/* 0xfc */ 0,
	/* 0xfd */ 0,
	/* 0xfe */ 0,
	/* 0xff */ 0
	/* }}} */
};

const char *js_char2escape[128] = {
	"\\u0000", "\\u0001", "\\u0002", "\\u0003",
	"\\u0004", "\\u0005", "\\u0006", "\\u0007",
	"\\b", "\\t", "\\n", "\\u000b",
	"\\f", "\\r", "\\u000e", "\\u000f",
	"\\u0010", "\\u0011", "\\u0012", "\\u0013",
	"\\u0014", "\\u0015", "\\u0016", "\\u0017",
	"\\u0018", "\\u0019", "\\u001a", "\\u001b",
	"\\u001c", "\\u001d", "\\u001e", "\\u001f",
	NULL, NULL, "\\\"", NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, "\\/",
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, "\\\\", NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, "\\u007f"
};

#endif /* defined(JS_SOURCE) */

/** \endcond */

/*
 * }}}
 */

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */

#undef JS_SOURCE
#undef JS_PROTO
#undef JS_IMPL
#undef JS_ALWAYSINLINE
#undef JS_GCC_VERSION

#endif /* JSONPUCK_H_INCLUDED */
