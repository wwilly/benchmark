// SPDX-License-Identifier: GPL-2.0
/*
 * Microbe is a microbenchmark to stress-test low-level hardware features.
 *
 *  Copyright (C) Willy Wolff <willy.mh.wolff@gmail.com>
 */

#include <limits.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

define(mym4for,
       `ifelse(eval(($4==0) || (($4>0) && ($2>$3)) || (($4<0) && ($2<$3))),1,,
       `define(`$1',$2)$5`'mym4for(`$1',eval($2 + $4),$3,$4,`$5')')')

#if defined(GLOBAL_ITERATOR)
/* if the iterator is a global, you have a store */
mym4for(`i', `1', ACCESS_REQ, +1, `format(`size_t idx_in_array_%d = 0;
', i)')
#endif /* GLOBAL_ITERATOR */

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

int main(int argc, char *argv[]) {
    for (int idx_arg = 0; idx_arg < argc; idx_arg++) {
    printf("argv[%d] = %s\n", idx_arg, argv[idx_arg]);
    }
	if (argc != 6 + ACCESS_REQ) {
		fprintf(stderr, "USAGE: %s <cpu_freq:KHz> <directory_to_put_results> <id_run> <nr_iter_1> <nr_iter_2>"
		mym4for(`i', `1', ACCESS_REQ, +1, `format(`" <sequence_file_access_req_%d>"
', i)')
		 "\n",
			argv[0]);
		exit(EXIT_FAILURE);
	}

	char *to_parse;
	unsigned long long int parse_me;

	// Note: we keep nr_iter and nr_iter_2 as size_t
	//       as too limit computation whether it is 32bit or 64bit
	//       nr_iter_2: outer-loop, nr_iter: inner-loop
	//       fix nr_iter first, then multiply this timing with nr_iter_2
	//       a nice idea is to set nr_iter to compute for 1s, and nr_iter_2 is a second multiplyer
	size_t nr_iter;
	size_t nr_iter_2;

	errno = 0;
	to_parse = argv[4];
	parse_me = strtoull(to_parse, NULL, 10);
	if ((parse_me == ULLONG_MAX && errno == ERANGE)
	    || (parse_me > SIZE_MAX - 1)) {
		fprintf(stderr, "<nr_iter> overflow %s > (%llu | %zu)\n",
			to_parse, ULLONG_MAX, SIZE_MAX - 1);
		exit(ERANGE);
	}
	nr_iter = (size_t) parse_me;

	errno = 0;
	to_parse = argv[5];
	parse_me = strtoull(to_parse, NULL, 10);
	if ((parse_me == ULLONG_MAX && errno == ERANGE)
	    || (parse_me > SIZE_MAX - 1)) {
		fprintf(stderr, "<nr_iter_2> overflow %s > (%llu | %zu)\n",
			to_parse, ULLONG_MAX, SIZE_MAX - 1);
		exit(ERANGE);
	}
	nr_iter_2 = (size_t) parse_me;

	unsigned long long effective_nr_iter;
	if (__builtin_mul_overflow(1ULL * nr_iter, 1ULL * nr_iter_2, &effective_nr_iter)) {
		fprintf(stderr, "effective_nr_iter overflowed\n");
		exit(ERANGE);
	}

	unsigned int cpu_freq = atoi(argv[1]);

	printf("sizeof(size_t) = %zu\n", sizeof(size_t));

	FILE *fd_configuration = fopen("configuration_array.txt", "w");
	if (!fd_configuration)
		handle_perror("configuration_array.txt");

	mym4for(`i', `1', ACCESS_REQ, +1, `format(`
	size_t *array_%d = NULL;

#if defined(LOCAL_ITERATOR)
	/* if the iterator is a local variable, we could have pure load */
	/* however, this depends on the number of hardware register available */
	/* and on the loop unroll factor */
	size_t idx_in_array_%d = 0;
#endif /* LOCAL_ITERATOR */

	FILE *sequence_%d = fopen(argv[5 + %d], "rb");
	if (!sequence_%d) {
		char error_msg[128];
		snprintf(error_msg, sizeof error_msg, "%%s", argv[5 + %d]);
		handle_perror(error_msg);
	}

	printf("preparing fread\n");

	size_t array_size_%d = SIZE_MAX;
	size_t stride_%d = SIZE_MAX;
	size_t page_stride_%d = SIZE_MAX;

	if (fread(&array_size_%d, sizeof(size_t), 1, sequence_%d) < 1)
		handle_perror("fread array_size_%d");
	if (fread(&stride_%d, sizeof(size_t), 1, sequence_%d) < 1)
		handle_perror("fread stride_%d");
	if (fread(&page_stride_%d, sizeof(size_t), 1, sequence_%d) < 1)
		handle_perror("fread page_stride_%d");

	fprintf(fd_configuration, "%%zu,%%zu,%%zu\n", array_size_%d, stride_%d, page_stride_%d);
	printf("preparing array_%d of size %%zu, stride_%d %%zu, page_stride_%d %%zu\n", array_size_%d, stride_%d, page_stride_%d);
	printf("allocation\n");

	printf("array_size_%d = %%zu\n", array_size_%d);
	size_t array_byte_size_%d = array_size_%d * sizeof(size_t);

	printf("==> %%zu B; %%f KB; %%f MB; %%f GB\n",
	       array_byte_size_%d, (double) array_byte_size_%d / 1024.,
	       (double) array_byte_size_%d / 1024. / 1024.,
	       (double) array_byte_size_%d / 1024. / 1024. / 1024.);
	// grep MemTotal /proc/meminfo to check physical memory
	printf("stride_%d = %%zu\n", stride_%d);
	printf("page_stride_%d = %%zu\n", page_stride_%d);

	/* array_%d = (size_t *) malloc(array_size * sizeof(size_t)); */

	void *p_%d = NULL;
	int ret_%d = posix_memalign(&p_%d, PAGE_SIZE,
				    array_size_%d * sizeof(size_t));

	if ((ret_%d != 0) | (p_%d == NULL)) {
		char error_msg[128];
		snprintf(error_msg, sizeof error_msg,
			 "alloc p_%d");
		handle_error_en(ret_%d, error_msg);
	}

	array_%d = p_%d;
	for (size_t idx_prep_array_%d = 0; idx_prep_array_%d < array_size_%d; idx_prep_array_%d++) {
		array_%d[idx_prep_array_%d] = SIZE_MAX;
	}

	if (stride_%d == 0) {
		printf("reading\n");
		if (fread(array_%d, sizeof(size_t), array_size_%d, sequence_%d) < array_size_%d)
			handle_perror("fread array");
	} else {
		printf("Init array\n");
		for (size_t idx = 0; idx < array_size_%d; idx++) {
			array_%d[idx %% array_size_%d] = (idx + stride_%d) %% array_size_%d;
		}
	}

	printf("done\n");

	fclose(sequence_%d);
	printf("preparation done\n");

	',
	i, i, i, i, i, i, i, i, i, i,
	i, i, i, i, i, i, i, i, i, i,
	i, i, i, i, i, i, i, i, i, i,
	i, i, i, i, i, i, i, i, i, i,
	i, i, i, i, i, i, i, i, i, i,
	i, i, i, i, i, i, i, i, i, i,
	i, i, i, i, i, i, i)')

	fclose(fd_configuration);
	printf("nr_iter = %zu\n", nr_iter);
	printf("nr_iter_2 = %zu\n", nr_iter_2);
	printf("effective_nr_iter = %llu\n", effective_nr_iter);
	printf("cpu_freq = %u\n", cpu_freq);
	printf("==> %g GHz; %g MHz; %u KHz\n",
	       cpu_freq * 1e-6, cpu_freq * 1e-3, cpu_freq);

	struct timespec export_time_start;
	clock_gettime(CLOCK_REALTIME, &export_time_start);

	struct timespec start;
	clock_gettime(CLOCK_MONOTONIC_RAW, &start);

	__asm__ volatile ("# critical in");
	for (size_t iter_2 = nr_iter_2; iter_2 > 0; --iter_2) {
		size_t iter = nr_iter;

		if (iter % UNROLL == 0) {
		  goto unroll_none;
		}mym4for(`unroll_idx', `1', UNROLL - 1, +1, `format(` else if (iter %% UNROLL == (UNROLL - %d)) {
		  iter = iter - (UNROLL - %d);
		  goto unroll_%d;
		}', unroll_idx, unroll_idx, unroll_idx)')

		mym4for(`unroll_idx', `1', UNROLL - 1, +1, `format(`
	unroll_%d:', unroll_idx)
		mym4for(`i', `1', ACCESS_REQ, +1, `format(`idx_in_array_%d = array_%d[idx_in_array_%d];
		', i, i, i)')')

	unroll_none:
		for (; iter > 0; iter -= UNROLL) {
			mym4for(`unroll_idx', `0', UNROLL - 1, +1, `
			mym4for(`i', `1', ACCESS_REQ, +1, `format(`idx_in_array_%d = array_%d[idx_in_array_%d];
			', i, i, i)')')
		}
	}
	__asm__ volatile ("# critical out");

	struct timespec stop;
	clock_gettime(CLOCK_MONOTONIC_RAW, &stop);

	struct timespec export_time_stop;
	clock_gettime(CLOCK_REALTIME, &export_time_stop);

	FILE *fd_timing = fopen("timing.csv", "w");
	if (!fd_timing)
		handle_perror("timing.csv");

	fprintf(fd_timing, "%10lu%09lu,start\n", export_time_start.tv_sec, export_time_start.tv_nsec);
	fprintf(fd_timing, "%10lu%09lu,stop\n", export_time_stop.tv_sec, export_time_stop.tv_nsec);
	fclose(fd_timing);

	mym4for(`i', `1', ACCESS_REQ, +1, `format(`/* print the iterator to cut its optimisation */
	printf("print to cut optimisation %s\n", idx_in_array_%d);
	free(array_%d);
	', %zu, i, i)')

	unsigned long long start_ = start.tv_sec * 1000000000ULL + start.tv_nsec;
	unsigned long long stop_ = stop.tv_sec * 1000000000ULL + stop.tv_nsec;

	unsigned long long delta = stop_ - start_;
	double time_per_iter = (double)delta / effective_nr_iter;
	double cycles_per_iter = time_per_iter * cpu_freq * 1e-6;

	printf("total time = %llu ns; %g s\n", delta, (double) delta * 1e-9);
	printf("time per iter %g ns\n", time_per_iter);
	printf("estimated cycles per iter %g c\n", cycles_per_iter);

	printf("\nTiming depends on the memory allocator, CPUs and memory frequency, "
	       "system busyness, number of iterations, etc...\n"
	       "Set the number of iteration to run for at least a few seconds.\n"
	       "Run multiple times before making any conclusion.\n\n");

	char general_path[4096] = {0};
	strncpy(general_path, argv[2], sizeof general_path - 1);

	char fd_summary_path[sizeof general_path + 13] = {0};
	snprintf(fd_summary_path, sizeof fd_summary_path, "%s/summary.csv",
		 general_path);

	FILE *fd_summary;
	fd_summary = fopen(fd_summary_path, "a");
	if (!fd_summary) {
		char error_msg[sizeof fd_summary_path + 8];
		snprintf(error_msg, sizeof error_msg,
		 "open(%s)", fd_summary_path);
		handle_perror(error_msg);
	}

	fprintf(fd_summary, "%s,%s,%llu,%u,%llu,%g,%g\n",
		argv[3], argv[0], effective_nr_iter, cpu_freq,
		delta, time_per_iter, cycles_per_iter);
	fclose(fd_summary);

	return EXIT_SUCCESS;
}
