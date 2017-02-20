/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (c) 2016 Pedro Gonnet (pedro.gonnet@durham.ac.uk)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

/* Config parameters. */
#include "../config.h"

/* Some standard headers. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* This object's header. */
#include "../src/dump.h"

/* Local headers. */
#include "../src/threadpool.h"

void dump_mapper(void *map_data, int num_elements, void *extra_data) {
  struct dump *d = (struct dump *)extra_data;
  size_t offset;
  char *out_string = dump_get(d, 7, &offset);
  char out_buff[8];
  snprintf(out_buff, 8, "%06zi\n", offset / 7);
  memcpy(out_string, out_buff, 7);
}

int main(int argc, char *argv[]) {

  /* Some constants. */
  const int num_threads = 4;
  const char *filename = "/tmp/dump_test.out";
  const int num_runs = 20;
  const int chunk_size = 1000;

  /* Prepare a threadpool to write to the dump. */
  struct threadpool t;
  threadpool_init(&t, num_threads);

  /* Prepare a dump. */
  struct dump d;
  dump_init(&d, filename, 1024);

  /* Dump numbers in chunks. */
  for (int run = 0; run < num_runs; run++) {

    /* Ensure capacity. */
    dump_ensure(&d, 7 * chunk_size);

    /* Dump a few numbers. */
    printf("dumping %i chunks...\n", chunk_size);
    fflush(stdout);
    threadpool_map(&t, dump_mapper, NULL, chunk_size, 0, 1, &d);
  }

  /* Sync the file, not necessary before dump_close, but just to test this. */
  dump_sync(&d);

  /* Finalize the dump. */
  dump_close(&d);

  /* Clean the threads */
  threadpool_clean(&t);
  
  /* Return a happy number. */
  return 0;
}
