/* Smoke test for mem_Alloc* macros.
 */

#include <stdio.h>
#include <stdint.h>

#define CSNIP_SHORT_NAMES
#include <csnip/mem.h>

static int test_alloc_bytes() {
	size_t size = 123456;

	uint8_t* p;
	mem_AllocBytes(size, p, _);

	for (size_t i = 0; i < size; ++i) {
		uint8_t value = i % 255;

		p[i] = value;
	}

	for (size_t i = 0; i < size; ++i) {
		uint8_t expected = i % 255;
		uint8_t actual = p[i];

		if (expected != actual) {
			fprintf(stderr, "Error:  Value at offset [%zu] is [%u], expected [%u]\n", i, actual, expected);
			return 0;
		}
	}

	mem_Free(p);

	return 1;
}

static int test_alloc_bytesx() {
	size_t size = 123456;

	uint8_t* p;
	if (mem_AllocBytesx(size, p) != 0) {
		fprintf(stderr, "Error:  Allocating [%zu] bytes failed.\n", size);
		return 0;
	}

	for (size_t i = 0; i < size; ++i) {
		uint8_t value = i % 255;

		p[i] = value;
	}

	for (size_t i = 0; i < size; ++i) {
		uint8_t expected = i % 255;
		uint8_t actual = p[i];

		if (expected != actual) {
			fprintf(stderr, "Error:  Value at offset [%zu] is [%u], expected [%u]\n", i, actual, expected);
			return 0;
		}
	}

	mem_Free(p);

	return 1;
}

int main(int argc, char** argv)
{
	if (!test_alloc_bytes())
		return 1;
	if (!test_alloc_bytesx())
		return 1;
	return 0;
}
