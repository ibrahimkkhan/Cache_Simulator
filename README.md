# CACHE SIMULATOR

## init_cache()
In this method I initialize the id of all all cores along with their sizes, head, tail and cache_contents. I initialize cache_contents to (cache size/ block size) and decrement the count when inserting into the cache and increment the count when deleting from the cache. The head and tail and tail are allocated memory as per the size of the cache_line.

## perform_access()
In both the store and load case, I start by checking if the head of the core that was passed (pid) is `NULL` or not and if it isn’t then a temp variable is associated to it for traversal. I then perform a search of whether the current cache has the cachline with the tag given to us. Every tag is equivalent to the address right shifted by `LOG2(cache_block_size)`. If the tag is found then we have a cache hit, otherwise a cache miss.

## *Case Load*
In the load case, when we have a hit (read hit), I simply delete the cache and insert it again to ensure that it is at the head of the Linked List (LRU Replacement Policy). In a Read Miss, I increment the misses and broadcast counters and search whether the tag is a hit in other caches using the `findOtherCores()` method. The `findOtherCores()` method returns a `Pcache_line` that points to the address where we have a hit in the other caches. This helps us know the state of the cache_line. If there’s no other `cache_line` with the same tag, then the cache is assigned the exclusive state. Finally, I perform a check on whether or not the cache is full and if it is then I delete the tail. If the tail is in the MODIFIED state then it is written back to memory so I increment the copies_back count. We then add the new cache_line (from memory).

## *Case Store*
Similar to the load case, even in this case when we have a hit (write hit), I simply delete the cache and insert it again to ensure that it is at the head of the Linked List (LRU Replacement Policy). One addition to this case is checking whether the cache_line was in the SHARED state and invalidating all copies of the cache_line. I use this using the `invalidateCopies()` method. This method traverses through all the cores (except pid) and deletes all cache_lines that are a tag match. Finally, the cache_line state also changes to MODIFIED and it is inserted and deleted.
In a Write Miss, I increment the misses and broadcast counters and search whether the tag is a hit in other caches using the `findOtherCores()` method. The `findOtherCores()` method returns a Pcache_line that points to the address where we have a hit in the other caches. This helps us know the state of the cache_line. If the cache_line is a hit in other caches, I use the `invalidateCopies()` method described before, to invalidate all the copies in other caches.
The cache is assigned the MODIFIED state. Finally, I perform a check on whether or not the cache is full and if it is then I delete the tail. If the tail is in the MODIFIED state then it is written back to memory so I increment the copies_back count. The cache_line is then inserted.

## flush()
This method traverses through all the cache lines in every core that are in the dirty state (MODIFIED) and deletes them. Before deleting, the copies back count is incremented as the cache lines are in the MODIFIED state.

## print_final _output()
This method appends the cache statistics for the caches tested with each test case trace file to the `output.txt` file.
The text file has all the cases run in the same order as in `validate.txt`. They display all the needed cache statistics. Each statistic is clearly labeled.  
