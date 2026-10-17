/*	Check that deinit releases slabs that alloc_item grew on demand.
 *
 *	A pool made with init_empty() gets all of its slabs from
 *	alloc_item().  This test allocates past the first slab, frees
 *	everything and deinits the pool.  The grown slabs must be
 *	recorded in pool.slabs; LeakSanitizer (-fsanitize=address)
 *	additionally reports them as leaked if deinit misses them.
 */

#include <stdio.h>
#include <stdlib.h>

#include <csnip/cext.h>
#include <csnip/mem.h>
#include <csnip/mempool.h>

typedef struct {
	void* p;
	int v;
} My;

CSNIP_MEMPOOL_DEF_TYPE(MyPool_s, My)
typedef struct MyPool_s MyPool;

CSNIP_MEMPOOL_DECL_FUNCS(static csnip_cext_unused,
		MyPool_,
		My,
		MyPool)
CSNIP_MEMPOOL_DEF_FUNCS(static csnip_cext_unused,
		MyPool_,
		My,
		MyPool)

int main(void)
{
	/* Enough items for several slabs (the first one holds 8) */
	enum { N = 100 };
	My* it[N];
	int pass = 1;

	MyPool pool = MyPool_init_empty();
	for (int i = 0; i < N; ++i) {
		int err = 0;
		it[i] = MyPool_alloc_item(&pool, &err);
		if (err || it[i] == NULL) {
			fprintf(stderr, "Error:  alloc_item failed at %d\n", i);
			pass = 0;
			break;
		}
		it[i]->v = i;
	}
	if (pass && pool.n_slabs < 2) {
		fprintf(stderr, "Error:  expected at least 2 slabs, "
				"have %zu\n", pool.n_slabs);
		pass = 0;
	}
	for (size_t i = 0; pass && i < pool.n_slabs; ++i) {
		if (pool.slabs[i] == NULL) {
			fprintf(stderr, "Error:  slab %zu is NULL\n", i);
			pass = 0;
		}
	}
	for (int i = 0; pass && i < N; ++i) {
		if (it[i]->v != i) {
			fprintf(stderr, "Error:  Value mismatch at %d\n", i);
			pass = 0;
		}
	}
	for (int i = 0; pass && i < N; ++i)
		MyPool_free_item(&pool, it[i]);
	MyPool_deinit(&pool);

	puts(pass ? "pass" : "FAIL");
	return (pass ? EXIT_SUCCESS : EXIT_FAILURE);
}
