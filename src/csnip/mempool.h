#ifndef CSNIP_MEMPOOL_H
#define CSNIP_MEMPOOL_H

/**	@file mempool.h
 *	@brief			Memory pools
 *	@defgroup mempool	Memory pools
 *	@{
 *
 *	@brief Memory pools.
 *
 *	Memory pools provide memory areas for allocating fixed size
 *	structs, i.e., they are sort of a specialized malloc for
 *	comparatively small memory blocks of small size.
 *
 *	The advantage they have over malloc() is O(1) operations with
 *	better constants, less memory fragmentation, and better spatial
 *	locality.
 *
 *	If the required capacity is known ahead of time, pool
 *	allocations and deallocations are O(1) time.  When the capacity
 *	is not known, they become amortized O(1) time.
 */

#include <assert.h>
#include <stddef.h>

#include <csnip/arr.h>
#include <csnip/mem.h>

/**
* @brief Defines a memory pool type structure.
*
* This macro generates a structure definition for a memory pool that manages
* fixed-size items. The pool uses a slab allocator with an intrusive free list.
*
* @param struct_pooltype  Name for the generated pool structure type
* @param itemtype         Type of items to be pooled (must be at least
*                         sizeof(void*) bytes and properly aligned)
*
* @details The generated structure contains:
* - **slabs**: Dynamic array of pointers to memory slabs (contiguous blocks
*              of items)
* - **n_slabs**: Current number of allocated slabs
* - **cap_slabs**: Capacity of the slabs pointer array before reallocation
* - **n_items**: Total number of items currently allocated to users
* - **first_free**: Head of the intrusive free list linking available items
*
* @note The itemtype must satisfy:
*       - sizeof(itemtype) >= sizeof(itemtype*)
*       - alignof(itemtype) >= alignof(itemtype*)
*       These requirements allow free items to store "next" pointers in their
*       own memory without additional overhead.
*
* @par Example:
* @code
* // Define a pool type for MyStruct
* CSNIP_MEMPOOL_DEF_TYPE(MyStructPool, struct MyStruct)
*
* // Now MyStructPool is available as a type
* struct MyStructPool pool;
* @endcode
*
* @see CSNIP_MEMPOOL_DECL_FUNCS For declaring pool operation functions
* @see CSNIP_MEMPOOL_DEF_FUNCS For defining pool operation functions
*/
#define CSNIP_MEMPOOL_DEF_TYPE(struct_pooltype, itemtype) \
	struct struct_pooltype { \
		itemtype** slabs; \
		size_t n_slabs; \
		size_t cap_slabs; \
		\
		size_t n_items; /* Number of items allocated */ \
		itemtype* first_free; \
	}; \

/**
* @brief Declares function prototypes for memory pool operations.
*
* This macro generates forward declarations for all functions needed to
* operate a memory pool. Use this in header files to expose the pool API.
*
* @param scope      Storage class specifier (e.g., `static` for internal
*                   linkage, or empty for external linkage)
* @param prefix     Prefix for generated function names (e.g., `mypool_`
*                   generates `mypool_init_empty`, `mypool_alloc_item`, etc.)
* @param itemtype   Type of items managed by the pool
* @param pooltype   The pool structure type (must be defined first with
*                   CSNIP_MEMPOOL_DEF_TYPE)
*
* @details Generated function declarations:
* - **prefix##init_empty()**: Creates an empty pool with no initial capacity
* - **prefix##init_with_cap(cap, err)**: Creates a pool pre-allocated with
*                                        `cap` items
* - **prefix##deinit(pool)**: Destroys the pool and frees all memory
* - **prefix##alloc_item(pool, err)**: Allocates one item from the pool
* - **prefix##free_item(pool, item)**: Returns an item to the pool
*
* @note This macro only declares functions. You must also use
*       CSNIP_MEMPOOL_DEF_FUNCS in exactly one translation unit to provide
*       the implementations.
*
 * @par Example:
 * @code
 * // In mypool.h
 * CSNIP_MEMPOOL_DEF_TYPE(MyPool, struct MyItem)
 * CSNIP_MEMPOOL_DECL_FUNCS(, mypool_, struct MyItem, struct MyPool)
 *
 * // Now these functions are declared:
 * // struct MyPool mypool_init_empty(void);
 * // struct MyItem* mypool_alloc_item(struct MyPool* pool, int* err);
 * // void mypool_free_item(struct MyPool* pool, struct MyItem* e);
 * // ...
 * @endcode
 *
 * @see CSNIP_MEMPOOL_DEF_TYPE For defining the pool structure
 * @see CSNIP_MEMPOOL_DEF_FUNCS For defining function implementations
 */
#define CSNIP_MEMPOOL_DECL_FUNCS(scope, \
		prefix, \
		itemtype, \
		pooltype) \
	/* Pool allocation and release */ \
	scope pooltype prefix##init_empty(void); \
	scope pooltype prefix##init_with_cap(size_t cap, int* err); \
	scope void prefix##deinit(pooltype* pool); \
	\
	/* Item allocation and release */ \
	scope itemtype* prefix##alloc_item(pooltype* pool, int* err); \
	scope void prefix##free_item(pooltype* pool, itemtype* e);

/**
* @brief Defines function implementations for memory pool operations.
*
* This macro generates the actual implementations of all memory pool functions.
* Use this in exactly one source file (.c) per pool type to avoid multiple
* definition errors during linking.
*
* @param scope      Storage class specifier (e.g., `static` for internal
*                   linkage, or empty for external linkage). Must match the
*                   scope used in CSNIP_MEMPOOL_DECL_FUNCS.
* @param prefix     Prefix for generated function names. Must match the prefix
*                   used in CSNIP_MEMPOOL_DECL_FUNCS.
* @param itemtype   Type of items managed by the pool
* @param pooltype   The pool structure type (must be defined first with
*                   CSNIP_MEMPOOL_DEF_TYPE)
*
* @details Generated functions:
*
* **Pool lifecycle:**
* - **prefix##init_empty()**: Returns an empty pool with zero capacity.
*                             Allocates no memory initially. First allocation
*                             will trigger slab creation.
* - **prefix##init_with_cap(cap, err)**: Pre-allocates one slab with `cap`
*                                        items. All items start in the free
*                                        list. Returns error via `*err` on
*                                        allocation failure.
* - **prefix##deinit(pool)**: Frees all slabs and resets pool to empty state.
*                             Does NOT check if items are still in use - caller
*                             must ensure all allocated items are freed first.
*
* **Item allocation:**
* - **prefix##alloc_item(pool, err)**: Pops one item from the free list.
*                                      If free list is empty, allocates a new
*                                      slab with exponential growth (minimum 8
*                                      items, then doubles based on n_items).
*                                      Returns NULL and sets `*err` on failure.
*                                      **O(1) amortized time complexity.**
* - **prefix##free_item(pool, item)**: Pushes item back onto the free list head.
*                                      Does NOT actually deallocate memory -
*                                      memory is reused for the next allocation.
*                                      Does NOT decrement n_items counter.
*                                      **O(1) time complexity.**
*
* **Internal helper:**
* - **prefix##get_slab(n_items, err)**: Allocates a contiguous array of
*                                       `n_items` and links them into a free
*                                       list by writing "next" pointers into
*                                       each item's memory.
*
* @note This macro includes compile-time assertions verifying that itemtype
*       meets size and alignment requirements for the intrusive free list.
*
* @warning The pool does NOT track which items are allocated vs free. Freeing
*          an item twice, or freeing an invalid pointer, will corrupt the free
*          list and cause undefined behavior.
*
* @par Implementation details:
* The free list is **intrusive**: when an item is free, its first sizeof(void*)
* bytes are overwritten with a pointer to the next free item. This means:
* - Zero per-item metadata overhead when allocated
* - Item memory is reused for bookkeeping when free
* - Items must be large enough to hold a pointer (enforced by _Static_assert)
*
* Growth strategy: New slabs are allocated with size equal to the current
* n_items count (doubling), with a minimum of 8 items. This provides amortized
* O(1) allocation while avoiding excessive waste for small pools.
*
 * @par Example:
 * @code
 * // In mypool.c
 * #include "mypool.h"
 * CSNIP_MEMPOOL_DEF_FUNCS(, mypool_, struct MyItem, struct MyPool)
 *
 * // Use the pool
 * int err = 0;
 * struct MyPool pool = mypool_init_with_cap(100, &err);
 * if (err) { return -1; }
 *
 * struct MyItem* item = mypool_alloc_item(&pool, &err);
 * // ... use item ...
 * mypool_free_item(&pool, item);
 *
 * mypool_deinit(&pool);
 * @endcode
 *
 * @see CSNIP_MEMPOOL_DEF_TYPE For defining the pool structure
 * @see CSNIP_MEMPOOL_DECL_FUNCS For declaring function prototypes
 */
#define CSNIP_MEMPOOL_DEF_FUNCS(scope, \
		prefix, \
		itemtype, \
		pooltype) \
	\
	_Static_assert(sizeof(itemtype) >= sizeof(itemtype*), \
			"memory pool items too small"); \
	_Static_assert(_Alignof(itemtype) >= _Alignof(itemtype*), \
			"item alignment needs to be that of a pointer."); \
	\
	/* Slab helper */ \
	scope itemtype* prefix##get_slab(size_t n_items, int* err) \
	{ \
		itemtype* sl = NULL; \
		csnip_mem_Alloc(n_items, sl, *err); \
		if (sl == NULL) \
			return NULL; \
		for (size_t i = 0; i < n_items - 1; ++i) \
			*((itemtype**)&sl[i]) = &sl[i + 1]; \
		*((itemtype**)&sl[n_items - 1]) = NULL; \
		return sl; \
	} \
	\
	scope pooltype prefix##init_empty(void) \
	{ \
		return (pooltype) { NULL }; \
	} \
	\
	scope pooltype prefix##init_with_cap(size_t cap, int* err) \
	{ \
		itemtype* sl = prefix##get_slab(cap, err); \
		itemtype** slabs; \
		csnip_mem_Alloc(1, slabs, *err); \
		slabs[0] = sl; \
		return (pooltype) { \
			.slabs = slabs, \
			.n_slabs = 1, \
			.cap_slabs = 1, \
			.n_items = 0, \
			.first_free = sl, \
		}; \
	} \
	\
	scope void prefix##deinit(pooltype* pool) \
	{ \
		for (size_t i = 0; i < pool->n_slabs; ++i) { \
			csnip_mem_Free(pool->slabs[i]); \
		} \
		csnip_mem_Free(pool->slabs); \
		*pool = (pooltype) { NULL }; \
	} \
	\
	/* Item allocation and release */ \
	scope itemtype* prefix##alloc_item(pooltype* pool, int* err) \
	{ \
		/* More memory needed ? */ \
		if (pool->first_free == NULL) { \
			size_t sz = pool->n_items; \
			if (sz < 8) sz = 8; \
			pool->first_free = prefix##get_slab(sz, err); \
		} \
		\
		/* Alloc and return */ \
		itemtype* it = pool->first_free; \
		assert(it != NULL); \
		++pool->n_items; \
		pool->first_free = *((itemtype**)it); \
		return it; \
	} \
	\
	scope void prefix##free_item(pooltype* pool, itemtype* e) \
	{ \
		*((itemtype**)e) = pool->first_free; \
		pool->first_free = e; \
	}

/** @} */

#endif /* CSNIP_MEMPOOL_H */
