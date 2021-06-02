/* Ibrahim Khan */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "cache.h"
#include "main.h"

/* cache configuration parameters */
static int cache_usize = DEFAULT_CACHE_SIZE;
static int cache_block_size = DEFAULT_CACHE_BLOCK_SIZE;
static int words_per_block = DEFAULT_CACHE_BLOCK_SIZE / WORD_SIZE;
static int cache_writeback = DEFAULT_CACHE_WRITEBACK;
static int cache_writealloc = DEFAULT_CACHE_WRITEALLOC;
static int num_core = DEFAULT_NUM_CORE;
static int hits = 0;
static int s_miss = 0;
static int l_miss = 0;

/* cache model data structures */
/* max of 8 cores */
static cache mesi_cache[8];
static cache_stat mesi_cache_stat[8];

/************************************************************/
void set_cache_param(param, value)
  int param;
  int value;
{
  switch (param) {
  case NUM_CORE:
    num_core = value;
    break;
  case CACHE_PARAM_BLOCK_SIZE:
    cache_block_size = value;
    words_per_block = value / WORD_SIZE;
    break;
  case CACHE_PARAM_USIZE:
    cache_usize = value;
    break;
  default:
    printf("error set_cache_param: bad parameter value\n");
    exit(-1);
  }
}
/************************************************************/

/************************************************************/
void init_cache()
{
  int i;

  /* initialize the cache, and cache statistics data structures */
  for(i = 0; i < num_core; i++){
    mesi_cache[i].id = i;
    mesi_cache[i].size = cache_usize;
    mesi_cache[i].LRU_head = (Pcache_line)malloc(sizeof(cache_line));
    mesi_cache[i].LRU_tail = (Pcache_line)malloc(sizeof(cache_line));
    mesi_cache[i].LRU_head = NULL;
		mesi_cache[i].LRU_tail = NULL;
    mesi_cache[i].cache_contents = cache_usize/cache_block_size;
  }
}

void invalidate_copies(unsigned tag, unsigned pid){
  int i;
	Pcache_line temp_head, temp_del;
  Pcache c;

  for(i=0;i<num_core;i++){
    if (i != pid){
      c = &mesi_cache[i];
      temp_head = c->LRU_head;
      while(temp_head != NULL){
        if(temp_head->tag == tag){
          delete(&(c->LRU_head), &(c->LRU_tail), temp_head, i);
          break;
        }
        temp_head = temp_head->LRU_next;
	    }
    }
  }
}
/************************************************************/

Pcache_line findOtherCores(unsigned tag, unsigned pid){
  int i;

  for (i = 0; i < num_core; i++){
    if (i != pid){
      Pcache_line temp;
      int found = 0;
      temp = mesi_cache[i].LRU_head;
      while(temp != NULL && found == 0){
        if(temp->tag == tag){
          found = 1;
          return temp;    
        }
        temp = temp->LRU_next;
      }
    }
  }
  return NULL;
}

void perform_access(addr, access_type, pid)
     unsigned addr, access_type, pid;
{
  int i, idx, hit = 0, hitOtherCache;
  unsigned tag = addr;
  Pcache c = &mesi_cache[pid];
  
  Pcache_line temp_head, temp_line, temp_tail, check_line, check_line2;

  mesi_cache_stat[pid].accesses++;
  tag = (addr) >> LOG2(cache_block_size);

  
  // idx = (addr) >> LOG2(sizeof(addr));
  switch(access_type)
  { 
    case TRACE_STORE: 
      if(c->LRU_head != NULL){
        temp_head = c->LRU_head;
      }else{
        temp_head = NULL;
      }

      while(temp_head != NULL && hit == 0){
				if(temp_head->tag == tag){
          if (temp_head->state != INVALID){
            hit = 1;
          }
        }else{
          temp_head = temp_head->LRU_next;
        }
			}

      if(hit){ // write hit
        if (temp_head->state == SHARED){
          mesi_cache_stat[pid].broadcasts++;
          invalidate_copies(tag, pid);
        }

        temp_head->state = MODIFIED;
        delete(&(c->LRU_head), &(c->LRU_tail), temp_head, pid);
        insert(&(c->LRU_head), &(c->LRU_tail), temp_head, pid);


      }else{// write miss
        temp_line = (Pcache_line)malloc(sizeof(cache_line));
        temp_line->tag = tag;
        temp_line->LRU_next = NULL;
        temp_line->LRU_prev = NULL;
        mesi_cache_stat[pid].misses++;
        mesi_cache_stat[pid].broadcasts++;
        mesi_cache_stat[pid].demand_fetches++;

        check_line2 = findOtherCores(tag, pid);

        if(check_line2 != NULL){
          invalidate_copies(tag,pid);
        }

        if (c->cache_contents == 0){ //full
          temp_tail = c->LRU_tail;
          if (temp_tail->state == MODIFIED){
            mesi_cache_stat[pid].copies_back += words_per_block;
          }
          delete(&(c->LRU_head), &(c->LRU_tail), temp_tail, pid);
        }
        temp_line->state = MODIFIED;
        insert(&(c->LRU_head), &(c->LRU_tail), temp_line, pid);
        
      }

      break;
    case TRACE_LOAD:
      if(c->LRU_head != NULL){
        temp_head = c->LRU_head;
      }else{
        temp_head = NULL;
      }

      while(temp_head != NULL && hit == 0){
				if(temp_head->tag == tag){
          hit = 1;
        }else{
          temp_head = temp_head->LRU_next;
        }
			}
      if (hit == 1){ //read hit
        delete(&(c->LRU_head), &(c->LRU_tail), temp_head, pid);
				insert(&(c->LRU_head), &(c->LRU_tail), temp_head, pid);
      }else{ //read miss
        mesi_cache_stat[pid].misses++;
				mesi_cache_stat[pid].broadcasts++;
        temp_line = (Pcache_line)malloc(sizeof(cache_line));
        temp_line->tag = tag;
        temp_line->LRU_next = NULL;
        temp_line->LRU_prev = NULL;
        mesi_cache_stat[pid].demand_fetches++;
        
        check_line = findOtherCores(tag,pid);

        if (check_line == NULL){ //no other cache has it
          temp_line->state = EXCLUSIVE;
        }else{
          if(check_line->state == MODIFIED){
            mesi_cache_stat[pid].copies_back += words_per_block;
          }
          temp_line->state = SHARED;
          check_line->state = SHARED;
        }

        if (c->cache_contents == 0){ //full
          temp_tail = c->LRU_tail;
          if (temp_tail->state == MODIFIED){
            mesi_cache_stat[pid].copies_back += words_per_block;
          }
          delete(&(c->LRU_head), &(c->LRU_tail), temp_tail, pid);
        }
        insert(&(c->LRU_head), &(c->LRU_tail), temp_line, pid);
      }
      break;
      free(temp_line);
  }

}
/************************************************************/

/************************************************************/

void flush(){
  int i;
	Pcache_line temp_head, temp_del;
	temp_head = (Pcache_line)malloc(sizeof(cache_line));
  for(i=0;i<num_core;i++){
    temp_head = mesi_cache[i].LRU_head;
    while(temp_head != NULL){
      if(temp_head->state == MODIFIED){
        delete(&(mesi_cache[i].LRU_head), &(mesi_cache[i].LRU_head), temp_head, i);
        mesi_cache_stat[i].copies_back += words_per_block;
      }
      temp_head = temp_head->LRU_next;
	  }
  }
}
/************************************************************/

/************************************************************/
void delete(head, tail, item, pid)
  Pcache_line *head, *tail;
  Pcache_line item;
{
  mesi_cache[pid].cache_contents++;
  if (item->LRU_prev) {
    item->LRU_prev->LRU_next = item->LRU_next;
  } else {
    /* item at head */
    *head = item->LRU_next;
  }

  if (item->LRU_next) {
    item->LRU_next->LRU_prev = item->LRU_prev;
  } else {
    /* item at tail */
    *tail = item->LRU_prev;
  }
}
/************************************************************/

/************************************************************/
/* inserts at the head of the list */
void insert(head, tail, item, pid)
  Pcache_line *head, *tail;
  Pcache_line item;
{
  mesi_cache[pid].cache_contents--;
  item->LRU_next = *head;
  item->LRU_prev = (Pcache_line)NULL;

  if (item->LRU_next)
    item->LRU_next->LRU_prev = item;
  else
    *tail = item;

  *head = item;
}
/************************************************************/

/************************************************************/
void dump_settings()
{
  printf("Cache Settings:\n");
  printf("\tSize: \t%d\n", cache_usize);
  printf("\tBlock size: \t%d\n", cache_block_size);
}
/************************************************************/

/************************************************************/

void print_final_output(){
    FILE *fp;
    int i, demand_fetches = 0, copies_back = 0, broadcasts = 0;
    fp = fopen("./final_output.txt", "a");
    fprintf(fp, "\nNumber of Cores: %d\tCache Size: %d\tBlock Size: %d\n", num_core,cache_usize,cache_block_size);
      for (i = 0; i < num_core; i++) {
        fprintf(fp, "  CORE %d\n", i);
        fprintf(fp,"  accesses:  %d\n", mesi_cache_stat[i].accesses);
        fprintf(fp, "  misses:    %d\n\n", mesi_cache_stat[i].misses);
        demand_fetches += mesi_cache_stat[i].demand_fetches;
        copies_back += mesi_cache_stat[i].copies_back;
        broadcasts += mesi_cache_stat[i].broadcasts;
      }
    fprintf(fp, "  demand fetch (words): %d\n", (demand_fetches)*cache_block_size/WORD_SIZE);
    fprintf(fp, "  broadcasts:           %d\n", broadcasts);
    fprintf(fp, "  copies back (words):  %d\n\n", copies_back);

    fclose(fp);


}

void print_stats()
{
  int i;
  int demand_fetches = 0;
  int copies_back = 0;
  int broadcasts = 0;

  printf("*** CACHE STATISTICS ***\n");

  for (i = 0; i < num_core; i++) {
    printf("  CORE %d\n", i);
    printf("  accesses:  %d\n", mesi_cache_stat[i].accesses);
    printf("  misses:    %d\n", mesi_cache_stat[i].misses);
    printf("  miss rate: %f (%f)\n", 
	   (float)mesi_cache_stat[i].misses / (float)mesi_cache_stat[i].accesses,
	   1.0 - (float)mesi_cache_stat[i].misses / (float)mesi_cache_stat[i].accesses);
    printf("  replace:   %d\n", mesi_cache_stat[i].replacements);
  }

  printf("\n");
  printf("  TRAFFIC\n");
  for (i = 0; i < num_core; i++) {
    demand_fetches += mesi_cache_stat[i].demand_fetches;
    copies_back += mesi_cache_stat[i].copies_back;
    broadcasts += mesi_cache_stat[i].broadcasts;
  }
  printf("  demand fetch (words): %d\n", (demand_fetches)*cache_block_size/WORD_SIZE);
  /* number of broadcasts */
  printf("  broadcasts:           %d\n", broadcasts);
  printf("  copies back (words):  %d\n", copies_back);

  print_final_output();

}
/************************************************************/

/************************************************************/
void init_stat(Pcache_stat stat)
{
  stat->accesses = 0;
  stat->misses = 0;
  stat->replacements = 0;
  stat->demand_fetches = 0;
  stat->copies_back = 0;
  stat->broadcasts = 0;
}
/************************************************************/

