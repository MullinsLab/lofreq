/* -*- c-file-style: "k&r"; indent-tabs-mode: nil; -*- */


/*********************************************************************
 *
 * FIXME update license
 *
 *********************************************************************/

/* loosely based on 0.1.18 sam_view.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>

/* samtools includes */
#include "sam.h"
#include "faidx.h"
/* from bedidx.c */
void *bed_read(const char *fn);
void bed_destroy(void *_h);
int bed_overlap(const void *_h, const char *chr, int beg, int end);

/* lofreq includes */
#include "log.h"
#include "utils.h"
#include "samutils.h"


#if 1
#define MYNAME "lofreq bam-stats"
#else
#define MYNAME PACKAGE
#endif

#define MAX_OP_COUNT 1024


typedef struct {
     int min_mq;
     int min_bq;
     char *fa;
     faidx_t *fai;
     void *bed;
     int samflags_on;
     int samflags_off;
} bamstats_conf_t;



/* sam_view.c:__g_skip_aln */
static inline int skip_aln(const bam_header_t *h, const bam1_t *b,
                           const int min_mq, const int flag_on, const int flag_off, void *bed)
{
     if (b->core.qual < min_mq || ((b->core.flag & flag_on) != flag_on) || (b->core.flag & flag_off)) {
          /*fprintf(stderr, "Skipping because of flag or min_mq\n");*/
          return 1;
     }
     if (bed && b->core.tid >= 0 && !bed_overlap(bed, h->target_name[b->core.tid], b->core.pos, bam_calend(&b->core, bam1_cigar(b)))) {
          /*fprintf(stderr, "Skipping because of bed: h->target_name[b->core.tid=%d] = %s; b->core.pos = %d\n", b->core.tid, h->target_name[b->core.tid], b->core.pos);*/
          return 2;
     }
     /*fprintf(stderr, "Not skipping\n");*/
     return 0;
}



static void
usage(bamstats_conf_t *bamstats_conf)
{
     fprintf(stderr, "%s: Compiles statistics from BAM files\n\n", MYNAME);

     fprintf(stderr,"Usage: %s [options] in.bam\n\n", MYNAME);
     fprintf(stderr,"Options:\n");
     fprintf(stderr, "       --verbose       Be verbose\n");
     fprintf(stderr, "       --debug         Enable debugging\n");
     fprintf(stderr, "  -l | --bed FILE      List of positions (chr pos) or regions (BED) [null]\n");
     fprintf(stderr, "  -f | --reffa FILE    Indexed reference fasta file (gzip supported) [null]\n");
     fprintf(stderr, "  -o | --out FILE      Write stats to this output file [- = stdout]\n");
     fprintf(stderr, "  -q | --min-bq INT    Ignore any base with baseQ smaller than INT [%d]\n", bamstats_conf->min_bq);
     fprintf(stderr, "  -m | --min-mq INT    Ignore reads with mapQ smaller than INT [%d]\n", bamstats_conf->min_mq);
}
/* usage() */


void
writestats(char *target_name, unsigned long int **read_cat_counts, unsigned long int num_reads, FILE *out)
{
     int i, j;
     fprintf(out, "#chrom\top-category\top-count\tproportion of reads\n");
     fprintf(out, "#proportions are in scientific notation or missing altogether if no reads for that count were found\n");
     for (i=0; i<NUM_OP_CATS; i++) {
          unsigned long int cat_sum = 0;
          for (j=0; j<MAX_OP_COUNT; j++) {
               if (read_cat_counts[i][j]) {
                    fprintf(out, "%s\t%s\t%d\t%g\t(%lu/%lu)\n", 
                            target_name, op_cat_str[i], j, read_cat_counts[i][j]/(double)num_reads, read_cat_counts[i][j], num_reads);
                    cat_sum += read_cat_counts[i][j];
               }
          }
          if (cat_sum != num_reads) {
               LOG_FIXME("fail cat_sum=%lu != num_reads=%lu\n", cat_sum, num_reads);
          }
     }
}


int 
bamstats(samfile_t *sam, bamstats_conf_t *bamstats_conf)
{
     char *target_name = NULL; /* chrom name */
     char *ref = NULL; /* reference sequence */
     unsigned long int **read_cat_counts;
     unsigned long int num_good_reads = 0;
     unsigned long int num_ign_reads = 0;
     unsigned int num_zero_matches = 0;
     bam1_t *b = bam_init1();
     int r, i, rc;

     /*  count_matches/read_cat_counts assume roughtly equal read length */
     LOG_WARN("%s\n", "assuming (roughly) equal read length");

     read_cat_counts = calloc(NUM_OP_CATS, sizeof(unsigned long int *));
     for (i=0; i<NUM_OP_CATS; i++) {
          read_cat_counts[i] = calloc(MAX_OP_COUNT, sizeof(unsigned long int));
     }
     
     while ((r = samread(sam, b)) >= 0) { // read one alignment from `in'
          int counts[NUM_OP_CATS];
          int ref_len = -1;
          
          if (skip_aln(sam->header, b, bamstats_conf->min_mq, 
                       bamstats_conf->samflags_on, bamstats_conf->samflags_off,
                       bamstats_conf->bed)) {
               num_ign_reads += 1;
               continue;
          }
          num_good_reads += 1;
          
          if (0 == (num_good_reads+num_ign_reads)%1000000) {
               LOG_VERBOSE("Still alive and happily crunching away on read number %d\n", (num_good_reads+num_ign_reads));
          }

          /* load ref only if necessary. also triggers output of
           * stats per chrom */
          if (ref == NULL || strcmp(target_name, sam->header->target_name[b->core.tid]) != 0) {
               if (ref) {
                    /* we already have some stats:  output */
                    writestats(target_name, read_cat_counts, num_good_reads, stdout);

                    /* reset everything for next chrom... */
                    for (i=0; i<NUM_OP_CATS; i++) {
                         memset(read_cat_counts[i], 0, MAX_OP_COUNT * sizeof(unsigned long int));
                    }
                    free(ref);
               } 

               target_name = sam->header->target_name[b->core.tid];
               ref = faidx_fetch_seq(bamstats_conf->fai, target_name,
                                     0, 0x7fffffff, &ref_len);
          }

          if (count_matches(counts, b, ref, bamstats_conf->min_mq)) {
               LOG_WARN("%s\n", "count_matches failed on read. ignoring"); /* FIXME print read */
               continue;
          }
          for (i=0; i<NUM_OP_CATS; i++) {
               assert(counts[i]<MAX_OP_COUNT);               
               read_cat_counts[i][counts[i]] += 1;
          }
          if (0 == counts[OP_MATCH]) {
               LOG_DEBUG("Got read with zero matches after filtering with min_mq %d: name:%s cigar:%s qual:%s\n", 
                        bamstats_conf->min_mq, bam1_qname(b), cigar_str_from_bam(b), bam1_qual(b));
               num_zero_matches += 1;
          }
#if 0
          LOG_DEBUG("good/ign=%u/%u: m=%d mm=%d i=%d d=%d\n", num_good_reads, num_ign_reads,
                    counts[OP_MATCH], counts[OP_MISMATCH], counts[OP_INS], counts[OP_DEL]);               
#endif
     }
               
     /* don't forget to output last seen chrom */
     if (ref) {
          writestats(target_name, read_cat_counts, num_good_reads, stdout);
          free(ref);
     }

     LOG_VERBOSE("#reads with zero matches (after bq filtering): %u\n", num_zero_matches);
     LOG_VERBOSE("#reads ignored for counting (due to bed/mq filtering): %u\n", num_ign_reads);
     LOG_VERBOSE("#reads used for counting: %u\n", num_good_reads);
     
     for (i=0; i<NUM_OP_CATS; i++) {
          free(read_cat_counts[i]);
     }
     free(read_cat_counts);
     
     if (r < -1) {
          LOG_FATAL("%s\n", "BAM file is truncated.\n");
          rc = 1;
     } else {
          rc = 0;
          bam_destroy1(b);
     }
     return rc;
}


int 
main_bamstats(int argc, char *argv[])
{
     char *outfile = NULL;
     char *bamfile = NULL;
     char *bedfile = NULL;
     samfile_t *sam =  NULL;
     int rc = 0;
     bamstats_conf_t bamstats_conf;

     memset(&bamstats_conf, 0, sizeof(bamstats_conf_t));
     bamstats_conf.min_mq = 13;
     bamstats_conf.min_bq = 3;
     /* will skip read if any of the following is set */
     bamstats_conf.samflags_off = 0;
     bamstats_conf.samflags_off |= 0x4; /* segment unmapped */
     bamstats_conf.samflags_off |= 0x100; /* secondary alignment */
     bamstats_conf.samflags_off |= 0x200; /* not passing quality controls */
     bamstats_conf.samflags_off |= 0x400; /* PCR or optical duplicate */
     bamstats_conf.samflags_off |= 0x800; /* supplementary alignment */

     /* FIXME enable BAQ on request ? */

     /* keep in sync with long_opts_str and usage 
     *
     * getopt is a pain in the whole when it comes to syncing of long
     * and short args and usage. check out gopt, libcfu
     */
     while (1) {
          int c;
          static struct option long_opts[] = {
               /* see usage sync */               
               {"bed", required_argument, NULL, 'l'},
               {"reffa", required_argument, NULL, 'f'},
               {"out", required_argument, NULL, 'o'},
               {"min-bq", required_argument, NULL, 'q'},
               {"min-mq", required_argument, NULL, 'm'},

               {"help", no_argument, NULL, 'h'},
               {"verbose", no_argument, &verbose, 1},
               {"debug", no_argument, &debug, 1},

               {0, 0, 0, 0} /* sentinel */
          };
          
          /* keep in sync with long_opts and usage */
          static const char *long_opts_str = "hl:f:o:q:m:"; 
          
          /* getopt_long stores the option index here. */
          int long_opts_index = 0;
          c = getopt_long(argc-1, argv+1, /* skipping 'lofreq', just leaving 'command', i.e. call */
                          long_opts_str, long_opts, & long_opts_index);
          if (c == -1) {
               break;
          }
          switch (c) {
               /* keep in sync with long_opts etc */

          case 'h': 
               usage(&bamstats_conf); 
               rc = 0;
               goto free_and_exit;

          case 'l': 
              bedfile = strdup(optarg);
              break;

          case 'f':
               bamstats_conf.fa = strdup(optarg);
               bamstats_conf.fai = fai_load(optarg);
               if (bamstats_conf.fai == 0)  {
                    rc = 1;
                    goto free_and_exit;
               }
               break;

          case 'o':
               if (0 != strcmp(optarg, "-")) {
                    if (file_exists(optarg)) {
                         LOG_FATAL("Cowardly refusing to overwrite file '%s'. Exiting...\n", optarg);
                         rc = 1;
                         goto free_and_exit;
                    }
               } 
               outfile = strdup(optarg);
               break;
               
         case 'q': 
              bamstats_conf.min_bq = atoi(optarg); 
              break;

          case 'm': 
              bamstats_conf.min_mq = atoi(optarg); 
              break;

         case '?': 
               LOG_FATAL("%s\n", "Unrecognized arguments found. Exiting...\n"); 
               rc = 1;
               goto free_and_exit;
               
          default:
               break;
          }
     }
     
     
     if (argc == 2) {
          fprintf(stderr, "\n");
          usage(&bamstats_conf);
          rc = 1;
          goto free_and_exit;
     }
     
     if (1 != argc - optind - 1) {
          LOG_FATAL("%s\n\n", "Need exactly one BAM file as last argument");
          rc = 1;
          goto free_and_exit;
     }
     bamfile = (argv + optind + 1)[0];
     if (!file_exists(bamfile)) {
          LOG_FATAL("BAM file %s does not exist.\n\n", bamfile);
          rc = 1;
          goto free_and_exit;
     }
     
     if (NULL == bamstats_conf.fa) {
          LOG_FATAL("%s\n\n", "ERROR: Missing reference fasta argument");
          usage(&bamstats_conf);
          rc = 1;
          goto free_and_exit;
     }

     if (bedfile) {
          LOG_VERBOSE("%s\n", "NOTE: bed routines don't make use of indexing and are therefore as slow as reading the whole BAM file."); /* FIXME */
          bamstats_conf.bed = bed_read(bedfile);
     }


    
     if ((sam = samopen(bamfile, "rb", NULL)) == 0) {
          LOG_FATAL("Failed to open \"%s\" for reading.\n", bamfile);
          rc = 1;
     } else {
          rc = bamstats(sam, &bamstats_conf);
     }

     samclose(sam);

free_and_exit:

     free(outfile);

     free(bamstats_conf.fa);
     if (bamstats_conf.fai) {
         fai_destroy(bamstats_conf.fai);
     }

     free(bedfile);
     if (bamstats_conf.bed) {
          bed_destroy(bamstats_conf.bed);
     }

     if (0==rc) {
          LOG_VERBOSE("%s\n", "Successful exit.");
     }

     return rc;
}
/* main_bamstats */
