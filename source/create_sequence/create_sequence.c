#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>

#define handle_perror(msg)				\
	do {						\
		fprintf(stderr,				\
			"%s:%d:%s(): PERROR: ",		\
			__FILE__, __LINE__, __func__);	\
		perror(msg);				\
		exit(EXIT_FAILURE);			\
	} while (0)

#define handle_error_en(en, msg)			\
	do {						\
		fprintf(stderr,				\
			"%s:%d:%s(): PERROR: ",		\
			__FILE__, __LINE__, __func__);	\
		errno = en;				\
		perror(msg);				\
		exit(EXIT_FAILURE);			\
	} while (0)


/* /\* arm 32bits without LPAE *\/ */
/* #define SIZE_T_BASE_TYPE uint32_t */
/* #define CACHE_LINE_SIZE 64 */
/* #define PAGE_SIZE 4096 */
/* #define PTRS_PER_PTE 512 */
/* #define PTRS_PER_PMD 1 */
/* #define PTRS_PER_PUD 1 */
/* #define PTRS_PER_P4D 1 */
/* #define PTRS_PER_PGD 2048 */

/* /\* arm 32bits with LPAE *\/ */
/* #define SIZE_T_BASE_TYPE uint32_t */
/* #define CACHE_LINE_SIZE 64 */
/* #define PAGE_SIZE 4096 */
/* #define PTRS_PER_PTE 512 */
/* #define PTRS_PER_PMD 512 */
/* #define PTRS_PER_PUD 1 */
/* #define PTRS_PER_P4D 1 */
/* #define PTRS_PER_PGD 4 */

/* /\* arm 64bits *\/ */
/* #define SIZE_T_BASE_TYPE uint64_t */
/* #define PAGE_SHIFT 12 */

/* #define CACHE_LINE_SIZE 64 */

/* #define PAGE_SIZE 4096 /\* 4KB *\/ */
/* #define VA_BITS 39 */
/* #define CONFIG_PGTABLE_LEVELS 3 */

/* #define PAGE_SIZE 4096 */
/* #define VA_BITS 48 */
/* #define CONFIG_PGTABLE_LEVELS 4 */

/* #define PAGE_SIZE 65536 /\* 64KB *\/ */
/* #define VA_BITS 48 */
/* #define CONFIG_PGTABLE_LEVELS 3 */

/* #define PAGE_SIZE 65536 /\* 64KB *\/ */
/* #define VA_BITS 52 */
/* #define CONFIG_PGTABLE_LEVELS 3 */

/* #define ARM64_HW_PGTABLE_LEVEL_SHIFT(n)	((PAGE_SHIFT - 3) * (4 - (n)) + 3) */
/* #define PGDIR_SHIFT ARM64_HW_PGTABLE_LEVEL_SHIFT(4 - CONFIG_PGTABLE_LEVELS) */

/* #define PTRS_PER_PTE		(1 << (PAGE_SHIFT - 3)) */

/* #if CONFIG_PGTABLE_LEVELS > 2 */
/* #define PTRS_PER_PMD		PTRS_PER_PTE */
/* #else */
/* #define PTRS_PER_PMD		1 */
/* #endif */

/* #if CONFIG_PGTABLE_LEVELS > 3 */
/* #define PTRS_PER_PUD		PTRS_PER_PTE */
/* #else */
/* #define PTRS_PER_PUD		1 */
/* #endif */

/* #define PTRS_PER_P4D		1 */

/* #define PTRS_PER_PGD		(1 << (VA_BITS - PGDIR_SHIFT)) */


#define NR_LAST_PAGE_ENTRY_TO_AVOID 0

#define CACHE_LINE_PER_PAGE (PAGE_SIZE / CACHE_LINE_SIZE)

/* #define PRODUCE_HUMAN_READABLE_LOCATION_FILE */

struct location {
	unsigned short int cl;
	unsigned short int pte;
	unsigned short int pmd;
	unsigned short int pud;
	unsigned short int p4d;
	unsigned int pgd;
};

size_t location_to_byte(struct location *l)
{
	return ((l->cl * CACHE_LINE_SIZE)
		+ (l->pte * PAGE_SIZE)
		+ (l->pmd * PTRS_PER_PTE * PAGE_SIZE)
		+ (l->pud * PTRS_PER_PMD * PTRS_PER_PTE * PAGE_SIZE)
		+ (l->p4d * PTRS_PER_PUD * PTRS_PER_PMD * PTRS_PER_PTE * PAGE_SIZE)
		+ (l->pgd * PTRS_PER_P4D * PTRS_PER_PUD * PTRS_PER_PMD * PTRS_PER_PTE * PAGE_SIZE));
}

void location_print(struct location *l)
{
	printf("%u %hu %hu %hu %hu %hu",
	       l->pgd, l->p4d, l->pud, l->pmd, l->pte, l->cl);
}

void location_swap(struct location *restrict left, struct location *restrict right)
{
	struct location tmp;
	memcpy(&tmp, right, sizeof(struct location));
	memcpy(right, left, sizeof(struct location));
	memcpy(left, &tmp, sizeof(struct location));
}

bool location_has_subset(struct location *restrict entry, struct location *restrict comp,
			 bool check_pgd, bool check_p4d, bool check_pud, bool check_pmd, bool check_pte)
{
	bool res = entry->cl == comp->cl;

	if (check_pgd)
		res |= entry->pgd == comp->pgd;
	if (check_p4d)
		res |= entry->p4d == comp->p4d;
	if (check_pud)
		res |= entry->pud == comp->pud;
	if (check_pmd)
		res |= entry->pmd == comp->pmd;
	if (check_pte)
		res |= entry->pte == comp->pte;

	return res;
}

bool location_has_common_IPT(struct location *restrict entry, struct location *restrict comp,
			     bool check_pgd, bool check_p4d, bool check_pud, bool check_pmd, bool check_pte)
{
	bool res = check_pte || check_pmd || check_pud || check_p4d || check_pgd;

	if (check_pgd)
		res &= entry->pgd == comp->pgd;
	if (check_p4d)
		res &= entry->p4d == comp->p4d;
	if (check_pud)
		res &= entry->pud == comp->pud;
	if (check_pmd)
		res &= entry->pmd == comp->pmd;
	if (check_pte)
		res &= entry->pte == comp->pte;

	return res;
}

bool location_is_in_last_entry_set(struct location *restrict entry, struct location *restrict array, size_t from, size_t to,
				   bool check_pgd, bool check_p4d, bool check_pud, bool check_pmd, bool check_pte)
{
#ifdef DEBUG
	printf("looking backward %zu %zu\n", from, to);
	location_print(entry);
	printf("\n");
#endif /* DEBUG */
	if (from == 0) {
#ifdef DEBUG
		printf("from == 0\n");
#endif /* DEBUG */
		if (location_has_common_IPT(entry, &array[0],
					    check_pgd, check_p4d, check_pud, check_pmd, check_pte)) {
#ifdef DEBUG
			printf("ret true\n");
#endif /* DEBUG */
			return true;
		}

		from = 1;
	}

	for (size_t idx = to; idx >= from; --idx) {
#ifdef DEBUG
		printf(" mem[%zu] ", idx);
		location_print(&array[idx]);
		printf("\n");
#endif /* DEBUG */
		if (location_has_common_IPT(entry, &array[idx],
					    check_pgd, check_p4d, check_pud, check_pmd, check_pte)) {
#ifdef DEBUG
			printf("ret true\n");
#endif /* DEBUG */
			return true;
		}
	}
#ifdef DEBUG
	printf("ret false\n");
#endif /* DEBUG */
	return false;
}

/* Arrange the N elements of ARRAY in random order.
   Only effective if N is much smaller than RAND_MAX;
   if this may not be the case, use a better random
   number generator. */
void shuffle(struct location *array, size_t n)
{
	if (n > 1) {
		for (size_t i = 0; i < n - 1; i++) {
			size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
			location_swap(&array[j], &array[i]);
		}
	}
}

int main(int argc, char *argv[])
{
#ifdef DEBUG
	unsigned int seed = 1;
#else
	unsigned int seed = time(NULL);
#endif /* DEBUG */

	srand(seed);

	if (argc != 2) {
		fprintf(stderr, "USAGE: %s <array_size>\n",
			argv[0]);
		exit(EXIT_FAILURE);
	}

	size_t array_size;

	char *to_parse;
	unsigned long long int parse_me;
	errno = 0;
	to_parse = argv[1];
	parse_me = strtoull(to_parse, NULL, 10);
	if ((parse_me == ULLONG_MAX && errno == ERANGE)
	    || (parse_me > SIZE_MAX - 1)) {
		fprintf(stderr, "<array_size> overflow %s > (%llu | %zu)\n",
			to_parse, ULLONG_MAX, SIZE_MAX - 1);
		exit(ERANGE);
	}
	array_size = (size_t) parse_me;

	/* array_size = 3424; */

	size_t array_size_byte = array_size * sizeof(SIZE_T_BASE_TYPE);
	size_t nr_cache_line = array_size_byte / CACHE_LINE_SIZE;
	size_t nr_page = array_size_byte / PAGE_SIZE;

	printf("sizeof(SIZE_T_BASE_TYPE) = %zu\n", sizeof(SIZE_T_BASE_TYPE));
	printf("array_size = %zu\n", array_size);
	printf("array_size_byte %zu\n", array_size_byte);
	printf("==> %zu B; %f KB; %f MB; %f GB; %f TB\n",
	       array_size_byte, (double) array_size_byte / 1024.,
	       (double) array_size_byte / 1024. / 1024.,
	       (double) array_size_byte / 1024. / 1024. / 1024.,
	       (double) array_size_byte / 1024. / 1024. / 1024. / 1024);

	printf("total full cache line %zu\n", nr_cache_line);
	printf("total full page %zu\n", nr_page);
	size_t last_page_offset = nr_page * CACHE_LINE_PER_PAGE;
	size_t last_page_elements = nr_cache_line - last_page_offset;
	if (last_page_elements > 0) {
		/* printf("%f\n", (double) array_size_byte / CACHE_LINE_SIZE); */
		printf("there is a page which is not full\n");
		printf("cache line in last page %zu\n", last_page_elements);
	}

	size_t nr_pte_entry = array_size_byte / PAGE_SIZE;
	size_t nr_pmd_entry = nr_pte_entry / PTRS_PER_PTE;
	size_t nr_pud_entry = nr_pmd_entry / PTRS_PER_PMD;
	size_t nr_p4d_entry = nr_pud_entry / PTRS_PER_PUD;
	size_t nr_pgd_entry = nr_p4d_entry / PTRS_PER_P4D;

	bool check_pte = PTRS_PER_PTE > 1 && nr_pte_entry > 1;
	bool check_pmd = PTRS_PER_PMD > 1 && nr_pmd_entry > 1;
	bool check_pud = PTRS_PER_PUD > 1 && nr_pud_entry > 1;
	bool check_p4d = PTRS_PER_P4D > 1 && nr_p4d_entry > 1;
	bool check_pgd = PTRS_PER_PGD > 1 && nr_pgd_entry > 1;

	/* printf("nr_pte_entry = %zu\n", nr_pte_entry); */
	/* printf("nr_pmd_entry = %zu\n", nr_pmd_entry); */
	/* printf("nr_pud_entry = %zu\n", nr_pud_entry); */
	/* printf("nr_p4d_entry = %zu\n", nr_p4d_entry); */
	/* printf("nr_pgd_entry = %zu\n", nr_pgd_entry); */

	/* printf("check_pte = %zu\n", check_pte); */
	/* printf("check_pmd = %zu\n", check_pmd); */
	/* printf("check_pud = %zu\n", check_pud); */
	/* printf("check_p4d = %zu\n", check_p4d); */
	/* printf("check_pgd = %zu\n", check_pgd); */

	/* getchar(); */

	size_t stride = 1;
	size_t page_stride = 1;

	char filename[1024];
	FILE *fd_sequence;
#ifdef PRODUCE_HUMAN_READABLE_LOCATION_FILE
	FILE *fd_sequence_location;
	size_t last_idx;
#endif /* PRODUCE_HUMAN_READABLE_LOCATION_FILE */

	SIZE_T_BASE_TYPE _array_size;
	SIZE_T_BASE_TYPE _stride;
	SIZE_T_BASE_TYPE _page_stride;

	snprintf(filename, sizeof(filename), "sequence_%zu_%zu_%zu.bin", array_size, stride, page_stride);
	fd_sequence = fopen(filename, "wb");
	if (!fd_sequence)
		handle_perror("fopen sequence");

	_array_size = array_size;
	if (fwrite(&_array_size, sizeof(SIZE_T_BASE_TYPE), 1, fd_sequence) < 1)
		handle_perror("fwrite array_size");

	_stride = stride;
	if (fwrite(&_stride, sizeof(SIZE_T_BASE_TYPE), 1, fd_sequence) < 1)
		handle_perror("fwrite stride");

	_page_stride = page_stride;
	if (fwrite(&_page_stride, sizeof(SIZE_T_BASE_TYPE), 1, fd_sequence) < 1)
		handle_perror("fwrite page_stride");
	fclose(fd_sequence);

	stride = CACHE_LINE_SIZE / sizeof(SIZE_T_BASE_TYPE);

	snprintf(filename, sizeof(filename), "sequence_%zu_%zu_%zu.bin", array_size, stride, page_stride);
	fd_sequence = fopen(filename, "wb");
	if (!fd_sequence)
		handle_perror("fopen sequence");

	_array_size = array_size;
	if (fwrite(&_array_size, sizeof(SIZE_T_BASE_TYPE), 1, fd_sequence) < 1)
		handle_perror("fwrite array_size");

	_stride = stride;
	if (fwrite(&_stride, sizeof(SIZE_T_BASE_TYPE), 1, fd_sequence) < 1)
		handle_perror("fwrite stride");

	_page_stride = page_stride;
	if (fwrite(&_page_stride, sizeof(SIZE_T_BASE_TYPE), 1, fd_sequence) < 1)
		handle_perror("fwrite page_stride");
	fclose(fd_sequence);

	if (nr_cache_line < 1) {
		printf("nothing to randomise\n");
		exit(1);
	}

	printf("\nworking on random sequence\n");
	printf("allocating {pgd, p4d, pud, pmd, pte, cl} array\n");
	struct location *mem = NULL;
	mem = malloc(nr_cache_line * sizeof(struct location));
	if (!mem)
		handle_perror("malloc");

	unsigned short int idx_cl = 0;
	unsigned short int idx_pte = 0;
	unsigned short int idx_pmd = 0;
	unsigned short int idx_pud = 0;
	unsigned short int idx_p4d = 0;
	unsigned int idx_pgd = 0;

	for (size_t idx = 0; idx < nr_cache_line; idx++) {
		mem[idx].cl = idx_cl;
		mem[idx].pte = idx_pte;
		mem[idx].pmd = idx_pmd;
		mem[idx].pud = idx_pud;
		mem[idx].p4d = idx_p4d;
		mem[idx].pgd = idx_pgd;

		idx_cl = (idx_cl + 1) % CACHE_LINE_PER_PAGE;

		if (idx_cl == 0) {
			idx_pte = (idx_pte + 1) % PTRS_PER_PTE;
			if (idx_pte == 0) {
				idx_pmd = (idx_pmd + 1) % PTRS_PER_PMD;
				if (idx_pmd == 0) {
					idx_pud = (idx_pud + 1) % PTRS_PER_PUD;
					if (idx_pud == 0) {
						idx_p4d = (idx_p4d + 1) % PTRS_PER_P4D;
						if (idx_p4d == 0) {
							idx_pgd = (idx_pgd + 1) % PTRS_PER_PGD;
						}
					}
				}
			}
		}
	}

	/* for (size_t idx = 0; idx < nr_cache_line; idx++) { */
	/*	printf("%zu ", idx); */
	/*	location_print(&mem[idx]); */
	/*	printf("\n"); */
	/* } */

	/* working inpage */
	printf("\nworking on random sequence for inpage\n");
	printf("randomise inpage\n");
	stride = 0;
	page_stride = 1;

	for (size_t idx_page = 0; idx_page < nr_page; idx_page++) {
		/* 1. shuffle the page */
		/* 2. move wraparound of the page to the last position in the page */
		/* 3. point to the next page */

		size_t page_offset = idx_page * CACHE_LINE_PER_PAGE;
		shuffle(&mem[page_offset], CACHE_LINE_PER_PAGE);

		for (size_t idx = page_offset; idx < (page_offset + CACHE_LINE_PER_PAGE); idx++) {
			if (mem[idx].cl == 0) {
				size_t idx_wraparound = page_offset + CACHE_LINE_PER_PAGE - 1;
				location_swap(&mem[idx], &mem[idx_wraparound]);

				mem[idx_wraparound].pte = (mem[idx_wraparound].pte + 1) % PTRS_PER_PTE;
				if (mem[idx_wraparound].pte == 0) {
					mem[idx_wraparound].pmd = (mem[idx_wraparound].pmd + 1) % PTRS_PER_PMD;
					if (mem[idx_wraparound].pmd == 0) {
						mem[idx_wraparound].pud = (mem[idx_wraparound].pud + 1) % PTRS_PER_PUD;
						if (mem[idx_wraparound].pud == 0) {
							mem[idx_wraparound].p4d = (mem[idx_wraparound].p4d + 1) % PTRS_PER_P4D;
							if (mem[idx_wraparound].p4d == 0) {
								mem[idx_wraparound].pgd = (mem[idx_wraparound].pgd + 1) % PTRS_PER_PGD;
							}
						}
					}
				}

				break;
			}
		}
	}

	if (last_page_elements > 0) {
		printf("working on last page\n");
		shuffle(&mem[last_page_offset], last_page_elements);

		/* printf("%zu / %zu, %zu\n", last_page_offset, nr_cache_line, last_page_elements); */
		/* getchar(); */

		for (size_t idx = last_page_offset; idx < nr_cache_line; idx++) {
			if (mem[idx].cl == 0) {
				location_swap(&mem[idx], &mem[nr_cache_line - 1]);
				break;
			}
		}
	}

	/* wraparound to the begining */
	mem[nr_cache_line - 1].cl = 0;
	mem[nr_cache_line - 1].pte = 0;
	mem[nr_cache_line - 1].pmd = 0;
	mem[nr_cache_line - 1].pud = 0;
	mem[nr_cache_line - 1].p4d = 0;
	mem[nr_cache_line - 1].pgd = 0;

	printf("randomise inpage done\n");

	printf("writing files\n");

	snprintf(filename, sizeof(filename), "sequence_%zu_%zu_%zu.bin", array_size, stride, page_stride);
	fd_sequence = fopen(filename, "wb");
	if (!fd_sequence)
		handle_perror("fopen sequence");

	_array_size = array_size;
	if (fwrite(&_array_size, sizeof(SIZE_T_BASE_TYPE), 1, fd_sequence) < 1)
		handle_perror("fwrite array_size");

	_stride = stride;
	if (fwrite(&_stride, sizeof(SIZE_T_BASE_TYPE), 1, fd_sequence) < 1)
		handle_perror("fwrite stride");

	_page_stride = page_stride;
	if (fwrite(&_page_stride, sizeof(SIZE_T_BASE_TYPE), 1, fd_sequence) < 1)
		handle_perror("fwrite page_stride");

#ifdef PRODUCE_HUMAN_READABLE_LOCATION_FILE
	snprintf(filename, sizeof(filename), "sequence_%zu_%zu_%zu_location.txt", array_size, stride, page_stride);
	fd_sequence_location = fopen(filename, "w");
	if (!fd_sequence)
		handle_perror("fopen sequence");

	last_idx = 0;
#endif /* PRODUCE_HUMAN_READABLE_LOCATION_FILE */

	size_t cache_line[CACHE_LINE_SIZE / sizeof(SIZE_T_BASE_TYPE)];
	memset(cache_line, 0, sizeof cache_line);

	for (size_t idx = 0; idx < nr_cache_line; idx++) {
		size_t next_idx_byte = location_to_byte(&mem[idx]);
		size_t next_idx = next_idx_byte / sizeof(SIZE_T_BASE_TYPE);

		cache_line[0] = next_idx;
		if (fwrite(cache_line, sizeof(SIZE_T_BASE_TYPE), CACHE_LINE_SIZE / sizeof(SIZE_T_BASE_TYPE), fd_sequence) < 1)
			handle_perror("fwrite array computed idx");

#ifdef PRODUCE_HUMAN_READABLE_LOCATION_FILE
		fprintf(fd_sequence_location, "%zu %u %hu %hu %hu %hu %hu %zu %zu\n",
			idx, mem[idx].pgd, mem[idx].p4d, mem[idx].pud, mem[idx].pmd, mem[idx].pte, mem[idx].cl, last_idx, next_idx);

		last_idx = next_idx;
#endif /* PRODUCE_HUMAN_READABLE_LOCATION_FILE */
	}

#ifdef PRODUCE_HUMAN_READABLE_LOCATION_FILE
	fclose(fd_sequence_location);
#endif /* PRODUCE_HUMAN_READABLE_LOCATION_FILE */

	fclose(fd_sequence);

	printf("done random inpage\n");
	/* getchar(); */

	/* working outpage */
	printf("\nworking on random sequence for outpage\n");
	/* 1. shuffle */
	/* 2. move wraparound at the top */
	/* 3. reorganise to satisfy constraints */
	/* 4. reverse */

	stride = 0;
	page_stride = 0;

	printf("shuffling\n");
	shuffle(mem, nr_cache_line);

	for (size_t idx = 0; idx < nr_cache_line; idx++) {
		if (mem[idx].cl == 0
		    && mem[idx].pte == 0
		    && mem[idx].pmd == 0
		    && mem[idx].pud == 0
		    && mem[idx].p4d == 0
		    && mem[idx].pgd == 0) {
			location_swap(&mem[idx], &mem[0]);
			break;
		}
	}

	printf("organise shuffled array per constraints\n");
	size_t cnt_fail_to_swap = 0;
	size_t cnt_fail_to_swap_relaxed = 0;
	bool first_fail_to_swap_has = false;
	size_t first_fail_to_swap_idx = SIZE_MAX;

	size_t starting_point = nr_cache_line - 1;

	for (size_t idx = 0; idx < nr_cache_line - 1; idx++) {
#ifdef DEBUG
		printf("\nat %zu ", idx);
		location_print(&mem[idx]);
		printf("\n");
		/* getchar(); */
#endif /* DEBUG */

		size_t low_idx;
		if (__builtin_sub_overflow(idx, NR_LAST_PAGE_ENTRY_TO_AVOID, &low_idx))
			low_idx = 0;

#ifdef DEBUG
		printf("\nchecking %zu ", idx + 1);
		location_print(&mem[idx + 1]);
		printf("\n");
#endif /* DEBUG */
		bool is_next_location_has_subset = location_has_subset(&mem[idx], &mem[idx + 1],
								       check_pgd,
								       check_p4d,
								       check_pud,
								       check_pmd,
								       check_pte);
		bool is_next_location_in_last_entry_set = location_is_in_last_entry_set(&mem[idx + 1], mem, low_idx, idx,
											check_pgd,
											check_p4d,
											check_pud,
											check_pmd,
											check_pte);
#ifdef DEBUG
		printf("is_next_location_has_subset = %d\n", is_next_location_has_subset);
		printf("is_next_location_in_last_entry_set = %d\n", is_next_location_in_last_entry_set);
#endif /* DEBUG */

		if (is_next_location_has_subset || is_next_location_in_last_entry_set) {
#ifdef DEBUG
			printf("should swap the next access at %zu\n", idx + 1);
			printf(" == %zu ", idx + 1);
			location_print(&mem[idx + 1]);
			printf("\n");
			/* getchar(); */
#endif /* DEBUG */
			bool swaped = false;
			size_t an_idx_no_subset = SIZE_MAX;


			bool retried = false;


			size_t idx_swap;
		retry:
			/* for (idx_swap = idx + 2; idx_swap < nr_cache_line; idx_swap++) { */
			for (idx_swap = starting_point; idx_swap >= idx + 2; --idx_swap) {
#ifdef DEBUG
				printf(" candidate %zu ", idx_swap);
				location_print(&mem[idx_swap]);
				printf("\n");
#endif /* DEBUG */
				bool _location_has_subset = location_has_subset(&mem[idx_swap], &mem[idx],
										check_pgd,
										check_p4d,
										check_pud,
										check_pmd,
										check_pte);
				if (!_location_has_subset)
					an_idx_no_subset = idx_swap;

				bool _location_is_in_last_entry_set = location_is_in_last_entry_set(&mem[idx_swap], mem, low_idx, idx,
												    check_pgd,
												    check_p4d,
												    check_pud,
												    check_pmd,
												    check_pte);
#ifdef DEBUG
				printf("_location_has_subset = %d\n", _location_has_subset);
				printf("_location_is_in_last_entry_set = %d\n", _location_is_in_last_entry_set);
#endif /* DEBUG */

				if (!_location_has_subset && !_location_is_in_last_entry_set) {
					location_swap(&mem[idx + 1], &mem[idx_swap]);
					swaped = true;

					starting_point = idx_swap - 1;

#ifdef DEBUG
					printf("found\n");
#endif /* DEBUG */
					break;
				}
			}

			if (idx_swap == idx + 2) {
				starting_point = nr_cache_line - 1;
			}

			/* TODO: if not swapped look after starting_point, and remove goto retry spaghetti */

			if (!swaped && !is_next_location_has_subset) {
#ifdef DEBUG
				printf("swapped by relaxed constraint (kept)\n");
#endif /* DEBUG */
				swaped = true;

				cnt_fail_to_swap_relaxed++;
				if (!first_fail_to_swap_has) {
					first_fail_to_swap_has = true;
					first_fail_to_swap_idx = nr_cache_line - 1 - idx;
				}
			}

			if (!swaped && is_next_location_has_subset && an_idx_no_subset != SIZE_MAX) {
#ifdef DEBUG
				printf("swapped by relaxed constraint %zu (%zu) %zu\n", idx, nr_cache_line - 1 - idx, an_idx_no_subset);
#endif /* DEBUG */
				location_swap(&mem[idx + 1], &mem[an_idx_no_subset]);

				swaped = true;

				cnt_fail_to_swap_relaxed++;
				if (!first_fail_to_swap_has) {
					first_fail_to_swap_has = true;
					first_fail_to_swap_idx = nr_cache_line - 1 - idx;
				}
			}

			if (!swaped) {
				if (!retried) {
					starting_point = nr_cache_line - 1;
					retried = true;
					goto retry;
				}

#ifdef DEBUG
				printf("no candidate for cache line %zu (%zu) %zu\n", idx, nr_cache_line - 1 - idx, an_idx_no_subset);
				location_print(&mem[idx]);
				printf("\n");
				getchar();
#endif /* DEBUG */

				cnt_fail_to_swap++;

				if (!first_fail_to_swap_has) {
					first_fail_to_swap_has = true;
					first_fail_to_swap_idx = nr_cache_line - 1 - idx;
				}
			}
#ifdef DEBUG
			getchar();
#endif /* DEBUG */
		}
	}

	printf("reverse array\n");
	for (size_t idx_low = 0, idx_high = nr_cache_line - 1;
	     idx_low < idx_high; idx_low++, idx_high--) {
		location_swap(&mem[idx_low], &mem[idx_high]);
	}

	if (cnt_fail_to_swap > 0 || cnt_fail_to_swap_relaxed > 0) {
		printf("\n\n====\n");
		printf("first_fail_to_swap_idx = %zu\n", first_fail_to_swap_idx);

		printf("nr cache lines that trully fail on the constraint %zu\n", cnt_fail_to_swap);
		printf("which is %f%% of the total cache line\n", (double)cnt_fail_to_swap * 100. / nr_cache_line);

		printf("nr cache lines that fail but pass on the relaxed constraint %zu\n", cnt_fail_to_swap_relaxed);
		printf("which is %f%% of the total cache line\n", (double)cnt_fail_to_swap_relaxed * 100. / nr_cache_line);

		printf("if it is a problem, increase the size of the array, or reduce NR_LAST_PAGE_ENTRY_TO_AVOID\n");
		printf("it also depends on the seed for srand()\n");
		printf("current seed %u\n", seed);
		printf("====\n\n");
	}

	printf("writing files\n");

	snprintf(filename, sizeof(filename), "sequence_%zu_%zu_%zu.bin", array_size, stride, page_stride);
	fd_sequence = fopen(filename, "wb");
	if (!fd_sequence)
		handle_perror("fopen sequence");

	_array_size = array_size;
	if (fwrite(&_array_size, sizeof(SIZE_T_BASE_TYPE), 1, fd_sequence) < 1)
		handle_perror("fwrite array_size");

	_stride = stride;
	if (fwrite(&_stride, sizeof(SIZE_T_BASE_TYPE), 1, fd_sequence) < 1)
		handle_perror("fwrite stride");

	_page_stride = page_stride;
	if (fwrite(&_page_stride, sizeof(SIZE_T_BASE_TYPE), 1, fd_sequence) < 1)
		handle_perror("fwrite page_stride");

#ifdef PRODUCE_HUMAN_READABLE_LOCATION_FILE
	snprintf(filename, sizeof(filename), "sequence_%zu_%zu_%zu_location.txt", array_size, stride, page_stride);
	fd_sequence_location = fopen(filename, "w");
	if (!fd_sequence)
		handle_perror("fopen sequence");

	last_idx = 0;
#endif /* PRODUCE_HUMAN_READABLE_LOCATION_FILE */

	for (size_t idx = 0; idx < nr_cache_line; idx++) {
		size_t next_idx_byte = location_to_byte(&mem[idx]);
		size_t next_idx = next_idx_byte / sizeof(SIZE_T_BASE_TYPE);

		cache_line[0] = next_idx;
		if (fwrite(cache_line, sizeof(SIZE_T_BASE_TYPE), CACHE_LINE_SIZE / sizeof(SIZE_T_BASE_TYPE), fd_sequence) < 1)
			handle_perror("fwrite array computed idx");

#ifdef PRODUCE_HUMAN_READABLE_LOCATION_FILE
		fprintf(fd_sequence_location, "%zu %u %hu %hu %hu %hu %hu %zu %zu\n",
			idx, mem[idx].pgd, mem[idx].p4d, mem[idx].pud, mem[idx].pmd, mem[idx].pte, mem[idx].cl, last_idx, next_idx);

		last_idx = next_idx;
#endif /* PRODUCE_HUMAN_READABLE_LOCATION_FILE */
	}

#ifdef PRODUCE_HUMAN_READABLE_LOCATION_FILE
	fclose(fd_sequence_location);
#endif /* PRODUCE_HUMAN_READABLE_LOCATION_FILE */

	fclose(fd_sequence);

	free(mem);

	printf("\ndone\n");
	return 0;
}
