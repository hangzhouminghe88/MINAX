/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2020 Kern Sibbald

   The original author of Bacula is Kern Sibbald, with contributions
   from many others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   This notice must be preserved when any source code is
   conveyed and/or propagated.

   Bacula(R) is a registered trademark of Kern Sibbald.
*/

/* Get the structure of a file to locate sparse blocks */

/* To compile:
   g++ -Wall -o bsparse bsparse.c
*/

#define VERSION "1.1"

#include <stropts.h>
#include <linux/fiemap.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/fiemap.h>
#include <string.h>

const char *progname="bsparse";

void usage()
{
   printf("VERSION: %s\n\nUsage: %s [-n nbrecords] [-i] [-b] [-s] files...\n"
          " -n <nb>        Allocate nb records for the ioctl interface\n"
          " -i             Use ioctl(FIEMAP) interface (default)\n"
          " -b             Use ioctl(FIBMAP) interface (slow)\n"
          " -s             Use lseek interface\n"
          " -h             Print this help message\n"
          " files...       List of files to analyze\n\n", VERSION, progname);
   exit(0);
}

#define FIEMAP_SIZE(nb) ((nb) * sizeof(struct fiemap_extent) + sizeof(struct fiemap))

int main(int argc, char **argv)
{
   unsigned int nb = 100000;
   int ch;
   int64_t size;
   int current;
   int tryioctl=0;
   int trylseek=0;
   int tryioctlB=0;

   progname = argv[0];
   
   while ((ch = getopt(argc, argv, "n:h?isb")) != -1) {
      switch (ch) {
      case 'i':
         tryioctl=1;
         break;
      case 's':
         trylseek=1;
         break;
      case 'n':                       /* number of entries */
         nb = atoi(optarg);
         break;
      case 'b':
         tryioctlB=1;
         break;
      case 'h':
      case '?':
      default:
         usage();
      }
   }
   if (nb <= 0) {
      printf("ERROR: invalid -n option\n");
      exit(1);
   }
   /* By default, we try the ioctl interface */
   if (!tryioctl && !trylseek && !tryioctlB) {
      tryioctl = 1;
   }
   size = FIEMAP_SIZE(nb);

   argc -= optind;
   argv += optind;

   if (argc == 0) {
      usage();
   }
   
   while (argc > 0) {
      int fd=-1;
      struct stat sp;
      int64_t pos,  start_hole, end_hole;
      struct fiemap *map;
      char ok;
      int retry=50;

      fd = open(argv[0], O_RDONLY);
      if (fd < 0) {
         printf("ERROR: Unable to open %s. ERR=%s\n", argv[0], strerror(errno));
         goto bail_out;
      }
      printf("%s:\n", argv[0]);
      if (fstat(fd, &sp) < 0) {
         printf("ERROR: Unable to stat %s. ERR=%s\n", argv[0], strerror(errno));
         goto bail_out;
      }
      if (!S_ISREG(sp.st_mode)) {
         printf("ERROR: %s is not a regular file\n", argv[0]);
         goto bail_out;
      }
      /* We list all blocks */
      if (tryioctlB) {
         int block, blocksize, blkcnt;
         if (ioctl(fd, FIGETBSZ, &blocksize)) {
            printf("ERROR: FIBMAP ioctl failed. ERR=%s\n", strerror(errno));
            goto bail_out;
         }
         blkcnt = (sp.st_size + blocksize - 1) / blocksize;
         printf("INFO: size %ld blocks %d blocksize %d\n",
                (int64_t)sp.st_size, blkcnt, blocksize);

         int64_t startH=0, endH=0;
         for (int i = 0; i < blkcnt; i++) {
            block = i;
            if (ioctl(fd, FIBMAP, &block)) {
               if (errno == EPERM) {
                  printf("ERROR: FIBMAP ioctl failed. ERR=%s\n", strerror(errno));
                  printf("INFO: Try again as \"root\"\n");
                  exit(1);
               }
               printf("ERROR: FIBMAP ioctl failed. %d/%d ERR=%s\n", i, blkcnt, strerror(errno));
            } else {
               if (block == 0) {
                  endH = i;
               } else if (startH < endH) {
                  printf("H %ld-%ld\n", startH*sp.st_blksize, (endH+1)*sp.st_blksize);
                  startH = i+1;
               }
            }
         }
      }
      if (tryioctl) {
         do {
            ok = 0;
            map = (struct fiemap *) malloc (size);
            memset(map, 0, size);
            map->fm_start = 0;
            map->fm_length = ~0ULL;
            map->fm_extent_count = nb;
            map->fm_flags = 0;
            if (ioctl(fd, FS_IOC_FIEMAP, map) == 0) {
               ok = 1;
               if (map->fm_mapped_extents == nb) {
                  printf("INFO: Need to grow the number of possible extent from %d to %d\n", nb, nb*2);
                  nb *= 2;
                  size = FIEMAP_SIZE(nb);
                  free(map);
                  retry--;
               } else {
                  retry = -1;   /* We have all records */
               }
            }
         } while (ok && retry > 0);

         if (ok && retry == 0) {
            printf("INFO: Need to restart this test with -n option > to %d\n", nb);
            exit(0);
         }

         if (ok) {
            uint64_t start=0;
            printf("INFO: found %d extent via ioct()\n", (int)map->fm_mapped_extents);
            for (unsigned int i = 0; i < map->fm_mapped_extents; i++) {
               if (start < map->fm_extents[i].fe_logical) {
                  printf("H %lu-%llu\n", start, map->fm_extents[i].fe_logical);
               }
               start = map->fm_extents[i].fe_logical + map->fm_extents[i].fe_length;
            }
            for (unsigned int i = 0; i < map->fm_mapped_extents; i++) {
               printf("D %lld:%lld [%s%s%s]\n",
                      map->fm_extents[i].fe_logical,
                      map->fm_extents[i].fe_length,
                      (map->fm_extents[i].fe_flags & FIEMAP_EXTENT_UNWRITTEN) ? "U":"",
                      (map->fm_extents[i].fe_flags & FIEMAP_EXTENT_DELALLOC) ? "D":"",
                      (map->fm_extents[i].fe_flags & FIEMAP_EXTENT_NOT_ALIGNED) ? "A":""
                  );
            }
         } else {
            printf("ERROR with ioctl(FIEMAP) on %s. ERR=%s\n", argv[0], strerror(errno));
         }
         free(map);
      }
      if (trylseek) {
         printf("INFO: Via lseek()\n");
         start_hole = pos = 0;
         current = SEEK_HOLE;
         do {
            if ((pos = lseek(fd, pos, current)) < 0) {
               printf("Unable to use lseek(SEEK_HOLE|SEEK_DATA) for %s\n", argv[0]);
               pos = sp.st_size;   /* stop here */

            } else {
               if (current == SEEK_HOLE) {
                  start_hole = pos;

               } else if (current == SEEK_DATA) {
                  end_hole = pos;
                  if (start_hole < end_hole) {
                     printf("H %ld:%ld\n", start_hole, end_hole);
                  }
                  end_hole = start_hole = 0;
               }
               /* flip between hole and data */
               current = (current == SEEK_HOLE) ? SEEK_DATA : SEEK_HOLE;
            }
         } while (pos < sp.st_size);
      }
   bail_out:
      close(fd);
      argc--;
      argv++;
   }
   return 0;
}
