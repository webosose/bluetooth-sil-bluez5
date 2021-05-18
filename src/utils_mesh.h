// Copyright (c) 2021 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef UTILS_MESH_H
#define UTILS_MESH_H

#ifdef __cplusplus
  extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <alloca.h>
#include <byteswap.h>
#include <string.h>

#define OP_UNRELIABLE			0x0100

#define MESH_AD_TYPE_PROVISION	0x29
#define MESH_AD_TYPE_NETWORK	0x2A
#define MESH_AD_TYPE_BEACON	0x2B

#define FEATURE_RELAY	1
#define FEATURE_PROXY	2
#define FEATURE_FRIEND	4
#define FEATURE_LPN	8

#define MESH_MODE_DISABLED	0
#define MESH_MODE_ENABLED	1
#define MESH_MODE_UNSUPPORTED	2

#define KEY_REFRESH_PHASE_NONE	0x00
#define KEY_REFRESH_PHASE_ONE	0x01
#define KEY_REFRESH_PHASE_TWO	0x02
#define KEY_REFRESH_PHASE_THREE	0x03

#define DEFAULT_TTL		0xff
#define TTL_MASK		0x7f

/* Supported algorithms for provisioning */
#define ALG_FIPS_256_ECC	0x0001

/* Input OOB action bit flags */
#define OOB_IN_PUSH	0x0001
#define OOB_IN_TWIST	0x0002
#define OOB_IN_NUMBER	0x0004
#define OOB_IN_ALPHA	0x0008

/* Output OOB action bit flags */
#define OOB_OUT_BLINK	0x0001
#define OOB_OUT_BEEP	0x0002
#define OOB_OUT_VIBRATE	0x0004
#define OOB_OUT_NUMBER	0x0008
#define OOB_OUT_ALPHA	0x0010

/* Status codes */
#define MESH_STATUS_SUCCESS		0x00
#define MESH_STATUS_INVALID_ADDRESS	0x01
#define MESH_STATUS_INVALID_MODEL	0x02
#define MESH_STATUS_INVALID_APPKEY	0x03
#define MESH_STATUS_INVALID_NETKEY	0x04
#define MESH_STATUS_INSUFF_RESOURCES	0x05
#define MESH_STATUS_IDX_ALREADY_STORED	0x06
#define MESH_STATUS_INVALID_PUB_PARAM	0x07
#define MESH_STATUS_NOT_SUB_MOD		0x08
#define MESH_STATUS_STORAGE_FAIL	0x09
#define MESH_STATUS_FEATURE_NO_SUPPORT	0x0a
#define MESH_STATUS_CANNOT_UPDATE	0x0b
#define MESH_STATUS_CANNOT_REMOVE	0x0c
#define MESH_STATUS_CANNOT_BIND		0x0d
#define MESH_STATUS_UNABLE_CHANGE_STATE	0x0e
#define MESH_STATUS_CANNOT_SET		0x0f
#define MESH_STATUS_UNSPECIFIED_ERROR	0x10
#define MESH_STATUS_INVALID_BINDING	0x11

#define UNASSIGNED_ADDRESS	0x0000
#define PROXIES_ADDRESS	0xfffc
#define FRIENDS_ADDRESS	0xfffd
#define RELAYS_ADDRESS		0xfffe
#define ALL_NODES_ADDRESS	0xffff
#define VIRTUAL_ADDRESS_LOW	0x8000
#define VIRTUAL_ADDRESS_HIGH	0xbfff
#define GROUP_ADDRESS_LOW	0xc000
#define GROUP_ADDRESS_HIGH	0xfeff
#define FIXED_GROUP_LOW		0xff00
#define FIXED_GROUP_HIGH	0xffff

#define NODE_IDENTITY_STOPPED		0x00
#define NODE_IDENTITY_RUNNING		0x01
#define NODE_IDENTITY_NOT_SUPPORTED	0x02

#define PRIMARY_ELE_IDX		0x00

#define PRIMARY_NET_IDX		0x0000
#define MAX_KEY_IDX		0x0fff
#define MAX_MODEL_COUNT		0xff
#define MAX_ELE_COUNT		0xff

#define MAX_MSG_LEN		380

#define VENDOR_ID_MASK		0xffff0000

#define NET_IDX_INVALID	0xffff
#define NET_NID_INVALID	0xff

#define APP_IDX_MASK		0x0fff
#define APP_IDX_DEV_REMOTE	0x6fff
#define APP_IDX_DEV_LOCAL	0x7fff

#define DEFAULT_SEQUENCE_NUMBER 0x000000

#define IS_UNASSIGNED(x)	((x) == UNASSIGNED_ADDRESS)
#define IS_UNICAST(x)		(((x) > UNASSIGNED_ADDRESS) && \
					((x) < VIRTUAL_ADDRESS_LOW))
#define IS_UNICAST_RANGE(x, c)	(IS_UNICAST(x) && IS_UNICAST(x + c - 1))
#define IS_VIRTUAL(x)		(((x) >= VIRTUAL_ADDRESS_LOW) && \
					((x) <= VIRTUAL_ADDRESS_HIGH))
#define IS_GROUP(x)		((((x) >= GROUP_ADDRESS_LOW) && \
					((x) < FIXED_GROUP_HIGH)) || \
					((x) == ALL_NODES_ADDRESS))

#define IS_FIXED_GROUP_ADDRESS(x)	((x) >= PROXIES_ADDRESS)
#define IS_ALL_NODES(x)	((x) == ALL_NODES_ADDRESS)

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define le16_to_cpu(val) (val)
#define le32_to_cpu(val) (val)
#define le64_to_cpu(val) (val)
#define cpu_to_le16(val) (val)
#define cpu_to_le32(val) (val)
#define cpu_to_le64(val) (val)
#define be16_to_cpu(val) bswap_16(val)
#define be32_to_cpu(val) bswap_32(val)
#define be64_to_cpu(val) bswap_64(val)
#define cpu_to_be16(val) bswap_16(val)
#define cpu_to_be32(val) bswap_32(val)
#define cpu_to_be64(val) bswap_64(val)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define le16_to_cpu(val) bswap_16(val)
#define le32_to_cpu(val) bswap_32(val)
#define le64_to_cpu(val) bswap_64(val)
#define cpu_to_le16(val) bswap_16(val)
#define cpu_to_le32(val) bswap_32(val)
#define cpu_to_le64(val) bswap_64(val)
#define be16_to_cpu(val) (val)
#define be32_to_cpu(val) (val)
#define be64_to_cpu(val) (val)
#define cpu_to_be16(val) (val)
#define cpu_to_be32(val) (val)
#define cpu_to_be64(val) (val)
#else
#error "Unknown byte order"
#endif

#define get_unaligned(ptr)			\
__extension__ ({				\
	struct __attribute__((packed)) {	\
		__typeof__(*(ptr)) __v;		\
	} *__p = (__typeof__(__p)) (ptr);	\
	__p->__v;				\
})

#define put_unaligned(val, ptr)			\
do {						\
	struct __attribute__((packed)) {	\
		__typeof__(*(ptr)) __v;		\
	} *__p = (__typeof__(__p)) (ptr);	\
	__p->__v = (val);			\
} while (0)

static inline int8_t get_s8(const void *ptr)
{
	return *((int8_t *) ptr);
}

static inline uint8_t get_u8(const void *ptr)
{
	return *((uint8_t *) ptr);
}

static inline uint16_t get_le16(const void *ptr)
{
	return le16_to_cpu(get_unaligned((const uint16_t *) ptr));
}

static inline uint16_t get_be16(const void *ptr)
{
	return be16_to_cpu(get_unaligned((const uint16_t *) ptr));
}

static inline uint32_t get_le32(const void *ptr)
{
	return le32_to_cpu(get_unaligned((const uint32_t *) ptr));
}

static inline uint32_t get_be32(const void *ptr)
{
	return be32_to_cpu(get_unaligned((const uint32_t *) ptr));
}

static inline uint64_t get_le64(const void *ptr)
{
	return le64_to_cpu(get_unaligned((const uint64_t *) ptr));
}

static inline uint64_t get_be64(const void *ptr)
{
	return be64_to_cpu(get_unaligned((const uint64_t *) ptr));
}

static inline void put_le16(uint16_t val, void *dst)
{
	put_unaligned(cpu_to_le16(val), (uint16_t *) dst);
}

static inline void put_be16(uint16_t val, const void *ptr)
{
	put_unaligned(cpu_to_be16(val), (uint16_t *) ptr);
}

static inline void put_le32(uint32_t val, void *dst)
{
	put_unaligned(cpu_to_le32(val), (uint32_t *) dst);
}

static inline void put_be32(uint32_t val, void *dst)
{
	put_unaligned(cpu_to_be32(val), (uint32_t *) dst);
}

static inline void put_le64(uint64_t val, void *dst)
{
	put_unaligned(cpu_to_le64(val), (uint64_t *) dst);
}

static inline void put_be64(uint64_t val, void *dst)
{
	put_unaligned(cpu_to_be64(val), (uint64_t *) dst);
}
bool meshOpcodeGet(const uint8_t *buf, uint16_t sz, uint32_t *opcode, int *n);
uint16_t meshOpcodeSet(uint32_t opcode, uint8_t *buf);
#ifdef __cplusplus
  }
#endif
#endif