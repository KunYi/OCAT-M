/**
 * @file endian.h
 * @brief Endianness conversion utilities
 * @version 1.0.0
 * @date 2026-01-03
 *
 * Provides endianness detection and conversion functions.
 * Optimized to avoid unnecessary conversions on little-endian systems.
 */

#ifndef ETHERCAT_ENDIAN_H
#define ETHERCAT_ENDIAN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Endianness Detection                                                       */
/* ========================================================================== */

/* Detect endianness at compile time */
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define ECAT_LITTLE_ENDIAN 1
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define ECAT_BIG_ENDIAN 1
#elif defined(__LITTLE_ENDIAN__) || defined(_LITTLE_ENDIAN) || \
      defined(__ARMEL__) || defined(__THUMBEL__) || defined(__AARCH64EL__) || \
      defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__) || \
      defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || \
      defined(_M_X64) || defined(_M_AMD64)
    #define ECAT_LITTLE_ENDIAN 1
#elif defined(__BIG_ENDIAN__) || defined(_BIG_ENDIAN) || \
      defined(__ARMEB__) || defined(__THUMBEB__) || defined(__AARCH64EB__) || \
      defined(_MIPSEB) || defined(__MIPSEB) || defined(__MIPSEB__)
    #define ECAT_BIG_ENDIAN 1
#else
    /* Runtime detection fallback */
    #define ECAT_RUNTIME_ENDIAN 1
#endif

/* ========================================================================== */
/* Byte Swap Functions                                                        */
/* ========================================================================== */

/**
 * @brief Swap bytes in 16-bit value
 */
static inline uint16_t ecat_bswap16(uint16_t x)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap16(x);
#else
    return (x >> 8) | (x << 8);
#endif
}

/**
 * @brief Swap bytes in 32-bit value
 */
static inline uint32_t ecat_bswap32(uint32_t x)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(x);
#else
    return ((x >> 24) & 0x000000FF) |
           ((x >>  8) & 0x0000FF00) |
           ((x <<  8) & 0x00FF0000) |
           ((x << 24) & 0xFF000000);
#endif
}

/**
 * @brief Swap bytes in 64-bit value
 */
static inline uint64_t ecat_bswap64(uint64_t x)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(x);
#else
    return ((x >> 56) & 0x00000000000000FFULL) |
           ((x >> 40) & 0x000000000000FF00ULL) |
           ((x >> 24) & 0x0000000000FF0000ULL) |
           ((x >>  8) & 0x00000000FF000000ULL) |
           ((x <<  8) & 0x000000FF00000000ULL) |
           ((x << 24) & 0x0000FF0000000000ULL) |
           ((x << 40) & 0x00FF000000000000ULL) |
           ((x << 56) & 0xFF00000000000000ULL);
#endif
}

/* ========================================================================== */
/* Host to Little-Endian Conversion                                           */
/* ========================================================================== */

#if defined(ECAT_LITTLE_ENDIAN)
    /* On little-endian systems, no conversion needed */
    #define ecat_htole16(x) (x)
    #define ecat_htole32(x) (x)
    #define ecat_htole64(x) (x)
    #define ecat_le16toh(x) (x)
    #define ecat_le32toh(x) (x)
    #define ecat_le64toh(x) (x)
#elif defined(ECAT_BIG_ENDIAN)
    /* On big-endian systems, swap bytes */
    #define ecat_htole16(x) ecat_bswap16(x)
    #define ecat_htole32(x) ecat_bswap32(x)
    #define ecat_htole64(x) ecat_bswap64(x)
    #define ecat_le16toh(x) ecat_bswap16(x)
    #define ecat_le32toh(x) ecat_bswap32(x)
    #define ecat_le64toh(x) ecat_bswap64(x)
#else
    /* Runtime detection */
    static inline int ecat_is_little_endian(void)
    {
        union {
            uint32_t i;
            uint8_t c[4];
        } test = { 0x01020304 };
        return test.c[0] == 4;
    }

    static inline uint16_t ecat_htole16(uint16_t x)
    {
        return ecat_is_little_endian() ? x : ecat_bswap16(x);
    }

    static inline uint32_t ecat_htole32(uint32_t x)
    {
        return ecat_is_little_endian() ? x : ecat_bswap32(x);
    }

    static inline uint64_t ecat_htole64(uint64_t x)
    {
        return ecat_is_little_endian() ? x : ecat_bswap64(x);
    }

    static inline uint16_t ecat_le16toh(uint16_t x)
    {
        return ecat_is_little_endian() ? x : ecat_bswap16(x);
    }

    static inline uint32_t ecat_le32toh(uint32_t x)
    {
        return ecat_is_little_endian() ? x : ecat_bswap32(x);
    }

    static inline uint64_t ecat_le64toh(uint64_t x)
    {
        return ecat_is_little_endian() ? x : ecat_bswap64(x);
    }
#endif

/* ========================================================================== */
/* Buffer Read/Write Functions                                                */
/* ========================================================================== */

/**
 * @brief Read uint16_t from buffer in little-endian format
 */
static inline uint16_t ecat_read_u16_le(const uint8_t* buffer)
{
    uint16_t value = (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
    return value;
}

/**
 * @brief Read uint32_t from buffer in little-endian format
 */
static inline uint32_t ecat_read_u32_le(const uint8_t* buffer)
{
    uint32_t value = (uint32_t)buffer[0] |
                     ((uint32_t)buffer[1] << 8) |
                     ((uint32_t)buffer[2] << 16) |
                     ((uint32_t)buffer[3] << 24);
    return value;
}

/**
 * @brief Write uint16_t to buffer in little-endian format
 */
static inline void ecat_write_u16_le(uint8_t* buffer, uint16_t value)
{
    buffer[0] = value & 0xFF;
    buffer[1] = (value >> 8) & 0xFF;
}

/**
 * @brief Write uint32_t to buffer in little-endian format
 */
static inline void ecat_write_u32_le(uint8_t* buffer, uint32_t value)
{
    buffer[0] = value & 0xFF;
    buffer[1] = (value >> 8) & 0xFF;
    buffer[2] = (value >> 16) & 0xFF;
    buffer[3] = (value >> 24) & 0xFF;
}

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_ENDIAN_H */
