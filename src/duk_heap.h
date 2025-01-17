/*
 *  Heap structure.
 *
 *  Heap contains allocated heap objects, interned strings, and built-in
 *  strings for one or more threads.
 */

#ifndef DUK_HEAP_H_INCLUDED
#define DUK_HEAP_H_INCLUDED

/* alloc function typedefs in duktape.h */

/*
 *  Heap flags
 */

#define DUK_HEAP_FLAG_MARKANDSWEEP_RUNNING                     (1 << 0)  /* mark-and-sweep is currently running */
#define DUK_HEAP_FLAG_MARKANDSWEEP_RECLIMIT_REACHED            (1 << 1)  /* mark-and-sweep marking reached a recursion limit and must use multi-pass marking */
#define DUK_HEAP_FLAG_REFZERO_FREE_RUNNING                     (1 << 2)  /* refcount code is processing refzero list */
#define DUK_HEAP_FLAG_ERRHANDLER_RUNNING                       (1 << 3)  /* an error handler (user callback to augment/replace error) is running */

#define DUK__HEAP_HAS_FLAGS(heap,bits)               ((heap)->flags & (bits))
#define DUK__HEAP_SET_FLAGS(heap,bits)  do { \
		(heap)->flags |= (bits); \
	} while (0)
#define DUK__HEAP_CLEAR_FLAGS(heap,bits)  do { \
		(heap)->flags &= ~(bits); \
	} while (0)

#define DUK_HEAP_HAS_MARKANDSWEEP_RUNNING(heap)            DUK__HEAP_HAS_FLAGS((heap), DUK_HEAP_FLAG_MARKANDSWEEP_RUNNING)
#define DUK_HEAP_HAS_MARKANDSWEEP_RECLIMIT_REACHED(heap)   DUK__HEAP_HAS_FLAGS((heap), DUK_HEAP_FLAG_MARKANDSWEEP_RECLIMIT_REACHED)
#define DUK_HEAP_HAS_REFZERO_FREE_RUNNING(heap)            DUK__HEAP_HAS_FLAGS((heap), DUK_HEAP_FLAG_REFZERO_FREE_RUNNING)
#define DUK_HEAP_HAS_ERRHANDLER_RUNNING(heap)              DUK__HEAP_HAS_FLAGS((heap), DUK_HEAP_FLAG_ERRHANDLER_RUNNING)

#define DUK_HEAP_SET_MARKANDSWEEP_RUNNING(heap)            DUK__HEAP_SET_FLAGS((heap), DUK_HEAP_FLAG_MARKANDSWEEP_RUNNING)
#define DUK_HEAP_SET_MARKANDSWEEP_RECLIMIT_REACHED(heap)   DUK__HEAP_SET_FLAGS((heap), DUK_HEAP_FLAG_MARKANDSWEEP_RECLIMIT_REACHED)
#define DUK_HEAP_SET_REFZERO_FREE_RUNNING(heap)            DUK__HEAP_SET_FLAGS((heap), DUK_HEAP_FLAG_REFZERO_FREE_RUNNING)
#define DUK_HEAP_SET_ERRHANDLER_RUNNING(heap)              DUK__HEAP_SET_FLAGS((heap), DUK_HEAP_FLAG_ERRHANDLER_RUNNING)

#define DUK_HEAP_CLEAR_MARKANDSWEEP_RUNNING(heap)          DUK__HEAP_CLEAR_FLAGS((heap), DUK_HEAP_FLAG_MARKANDSWEEP_RUNNING)
#define DUK_HEAP_CLEAR_MARKANDSWEEP_RECLIMIT_REACHED(heap) DUK__HEAP_CLEAR_FLAGS((heap), DUK_HEAP_FLAG_MARKANDSWEEP_RECLIMIT_REACHED)
#define DUK_HEAP_CLEAR_REFZERO_FREE_RUNNING(heap)          DUK__HEAP_CLEAR_FLAGS((heap), DUK_HEAP_FLAG_REFZERO_FREE_RUNNING)
#define DUK_HEAP_CLEAR_ERRHANDLER_RUNNING(heap)            DUK__HEAP_CLEAR_FLAGS((heap), DUK_HEAP_FLAG_ERRHANDLER_RUNNING)

/*
 *  Longjmp types, also double as identifying continuation type for a rethrow (in 'finally')
 */

#define DUK_LJ_TYPE_UNKNOWN      0    /* unused */
#define DUK_LJ_TYPE_RETURN       1    /* value1 -> return value */
#define DUK_LJ_TYPE_THROW        2    /* value1 -> error object */
#define DUK_LJ_TYPE_BREAK        3    /* value1 -> label number */
#define DUK_LJ_TYPE_CONTINUE     4    /* value1 -> label number */
#define DUK_LJ_TYPE_YIELD        5    /* value1 -> yield value, iserror -> error / normal */
#define DUK_LJ_TYPE_RESUME       6    /* value1 -> resume value, value2 -> resumee thread, iserror -> error/normal */
#define DUK_LJ_TYPE_NORMAL       7    /* pseudo-type to indicate a normal continuation (for 'finally' rethrowing) */

/* dummy non-zero value to be used as an argument for longjmp(), see man longjmp */
#define DUK_LONGJMP_DUMMY_VALUE  1

/*
 *  Mark-and-sweep flags
 *
 *  These are separate from heap level flags now but could be merged.
 *  The heap structure only contains a 'base mark-and-sweep flags'
 *  field and the GC caller can impose further flags.
 */

#define DUK_MS_FLAG_EMERGENCY                (1 << 0)   /* emergency mode: try extra hard */
#define DUK_MS_FLAG_NO_STRINGTABLE_RESIZE    (1 << 1)   /* don't resize stringtable (but may sweep it); needed during stringtable resize */
#define DUK_MS_FLAG_NO_FINALIZERS            (1 << 2)   /* don't run finalizers (which may have arbitrary side effects) */
#define DUK_MS_FLAG_NO_OBJECT_COMPACTION     (1 << 3)   /* don't compact objects; needed during object property allocation resize */

/*
 *  Thread switching
 *
 *  To switch heap->curr_thread, use the macro below so that interrupt counters
 *  get updated correctly.  The macro allows a NULL target thread because that
 *  happens e.g. in call handling.
 */

#ifdef DUK_USE_INTERRUPT_COUNTER
#define DUK_HEAP_SWITCH_THREAD(heap,newthr)  duk_heap_switch_thread((heap), (newthr))
#else
#define DUK_HEAP_SWITCH_THREAD(heap,newthr)  do { \
		(heap)->curr_thread = (newthr); \
	} while (0)
#endif

/*
 *  Other heap related defines
 */

/* Maximum duk_handle_call / duk_handle_safe_call depth.  Note that this
 * does not limit bytecode executor internal call depth at all (e.g.
 * for Ecmascript-to-Ecmascript calls, thread yields/resumes, etc).
 * There is a separate callstack depth limit for threads.
 */

#if defined(DUK_USE_DEEP_C_STACK)
#define DUK_HEAP_DEFAULT_CALL_RECURSION_LIMIT             1000  /* assuming 0.5 kB between calls, about 500kB of stack */
#else
#define DUK_HEAP_DEFAULT_CALL_RECURSION_LIMIT             60    /* assuming 0.5 kB between calls, about 30kB of stack */
#endif

/* Mark-and-sweep C recursion depth for marking phase; if reached,
 * mark object as a TEMPROOT and use multi-pass marking.
 */
#if defined(DUK_USE_MARK_AND_SWEEP)
#if defined(DUK_USE_GC_TORTURE)
#define DUK_HEAP_MARK_AND_SWEEP_RECURSION_LIMIT   3
#elif defined(DUK_USE_DEEP_C_STACK)
#define DUK_HEAP_MARK_AND_SWEEP_RECURSION_LIMIT   256
#else
#define DUK_HEAP_MARK_AND_SWEEP_RECURSION_LIMIT   32
#endif
#endif

/* Mark-and-sweep interval is relative to combined count of objects and
 * strings kept in the heap during the latest mark-and-sweep pass.
 * Fixed point .8 multiplier and .0 adder.  Trigger count (interval) is
 * decreased by each (re)allocation attempt (regardless of size), and each
 * refzero processed object.
 *
 * 'SKIP' indicates how many (re)allocations to wait until a retry if
 * GC is skipped because there is no thread do it with yet (happens
 * only during init phases).
 */
#if defined(DUK_USE_MARK_AND_SWEEP)
#if defined(DUK_USE_REFERENCE_COUNTING)
#define DUK_HEAP_MARK_AND_SWEEP_TRIGGER_MULT              12800L  /* 50x heap size */
#define DUK_HEAP_MARK_AND_SWEEP_TRIGGER_ADD               1024L
#define DUK_HEAP_MARK_AND_SWEEP_TRIGGER_SKIP              256L
#else
#define DUK_HEAP_MARK_AND_SWEEP_TRIGGER_MULT              256L    /* 1x heap size */
#define DUK_HEAP_MARK_AND_SWEEP_TRIGGER_ADD               1024L
#define DUK_HEAP_MARK_AND_SWEEP_TRIGGER_SKIP              256L
#endif
#endif

/* Stringcache is used for speeding up char-offset-to-byte-offset
 * translations for non-ASCII strings.
 */
#define DUK_HEAP_STRCACHE_SIZE                            4
#define DUK_HEAP_STRINGCACHE_NOCACHE_LIMIT                16  /* strings up to the this length are not cached */

/* helper to insert a (non-string) heap object into heap allocated list */
#define DUK_HEAP_INSERT_INTO_HEAP_ALLOCATED(heap,hdr)     duk_heap_insert_into_heap_allocated((heap),(hdr))

/* Executor interrupt default interval when nothing else requires a
 * smaller value.  The default interval must be small enough to allow
 * for reasonable execution timeout checking.
 */
#ifdef DUK_USE_INTERRUPT_COUNTER
#define DUK_HEAP_INTCTR_DEFAULT                           (256L * 1024L)
#endif

/*
 *  Stringtable
 */

/* initial stringtable size, must be prime and higher than DUK_UTIL_MIN_HASH_PRIME */
#define DUK_STRTAB_INITIAL_SIZE            17

/* indicates a deleted string; any fixed non-NULL, non-hstring pointer works */
#define DUK_STRTAB_DELETED_MARKER(heap)    ((duk_hstring *) heap)

/* resizing parameters */
#define DUK_STRTAB_MIN_FREE_DIVISOR        4                /* load factor max 75% */
#define DUK_STRTAB_MIN_USED_DIVISOR        4                /* load factor min 25% */
#define DUK_STRTAB_GROW_ST_SIZE(n)         ((n) + (n))      /* used entries + approx 100% -> reset load to 50% */

#define DUK_STRTAB_U32_MAX_STRLEN          10               /* 4'294'967'295 */
#define DUK_STRTAB_HIGHEST_32BIT_PRIME     0xfffffffbUL

/* probe sequence */
#define DUK_STRTAB_HASH_INITIAL(hash,h_size)    ((hash) % (h_size))
#define DUK_STRTAB_HASH_PROBE_STEP(hash)        DUK_UTIL_GET_HASH_PROBE_STEP((hash))

/*
 *  Built-in strings
 */

/* heap string indices are autogenerated in duk_strings.h */
#if defined(DUK_USE_HEAPPTR16)
#define DUK_HEAP_GET_STRING(heap,idx) \
	((duk_hstring *) DUK_USE_HEAPPTR_DEC16((heap)->strs16[(idx)]))
#else
#define DUK_HEAP_GET_STRING(heap,idx) \
	((heap)->strs[(idx)])
#endif

/*
 *  Raw memory calls: relative to heap, but no GC interaction
 */

#define DUK_ALLOC_RAW(heap,size) \
	((heap)->alloc_func((heap)->alloc_udata, (size)))

#define DUK_REALLOC_RAW(heap,ptr,newsize) \
	((heap)->realloc_func((heap)->alloc_udata, (ptr), (newsize)))

#define DUK_FREE_RAW(heap,ptr) \
	((heap)->free_func((heap)->alloc_udata, (ptr)))

/*
 *  Memory calls: relative to heap, GC interaction, but no error throwing.
 *
 *  XXX: Currently a mark-and-sweep triggered by memory allocation will run
 *  using the heap->heap_thread.  This thread is also used for running
 *  mark-and-sweep finalization; this is not ideal because it breaks the
 *  isolation between multiple global environments.
 *
 *  Notes:
 *
 *    - DUK_FREE() is required to ignore NULL and any other possible return
 *      value of a zero-sized alloc/realloc (same as ANSI C free()).
 *
 *    - There is no DUK_REALLOC_ZEROED because we don't assume to know the
 *      old size.  Caller must zero the reallocated memory.
 *
 *    - DUK_REALLOC_INDIRECT() must be used when a mark-and-sweep triggered
 *      by an allocation failure might invalidate the original 'ptr', thus
 *      causing a realloc retry to use an invalid pointer.  Example: we're
 *      reallocating the value stack and a finalizer resizes the same value
 *      stack during mark-and-sweep.  The indirect variant requests for the
 *      current location of the pointer being reallocated using a callback
 *      right before every realloc attempt; this circuitous approach is used
 *      to avoid strict aliasing issues in a more straightforward indirect
 *      pointer (void **) approach.  Note: the pointer in the storage
 *      location is read but is NOT updated; the caller must do that.
 */

/* callback for indirect reallocs, request for current pointer */
typedef void *(*duk_mem_getptr)(void *ud);

#define DUK_ALLOC(heap,size)                            duk_heap_mem_alloc((heap), (size))
#define DUK_ALLOC_ZEROED(heap,size)                     duk_heap_mem_alloc_zeroed((heap), (size))
#define DUK_REALLOC(heap,ptr,newsize)                   duk_heap_mem_realloc((heap), (ptr), (newsize))
#define DUK_REALLOC_INDIRECT(heap,cb,ud,newsize)        duk_heap_mem_realloc_indirect((heap), (cb), (ud), (newsize))
#define DUK_FREE(heap,ptr)                              duk_heap_mem_free((heap), (ptr))

/*
 *  Memory constants
 */

#define DUK_HEAP_ALLOC_FAIL_MARKANDSWEEP_LIMIT           5   /* Retry allocation after mark-and-sweep for this
                                                              * many times.  A single mark-and-sweep round is
                                                              * not guaranteed to free all unreferenced memory
                                                              * because of finalization (in fact, ANY number of
                                                              * rounds is strictly not enough).
                                                              */

#define DUK_HEAP_ALLOC_FAIL_MARKANDSWEEP_EMERGENCY_LIMIT  3  /* Starting from this round, use emergency mode
                                                              * for mark-and-sweep.
                                                              */

/*
 *  String cache should ideally be at duk_hthread level, but that would
 *  cause string finalization to slow down relative to the number of
 *  threads; string finalization must check the string cache for "weak"
 *  references to the string being finalized to avoid dead pointers.
 *
 *  Thus, string caches are now at the heap level now.
 */

struct duk_strcache {
	duk_hstring *h;
	duk_uint32_t bidx;
	duk_uint32_t cidx;
};

/*
 *  Longjmp state, contains the information needed to perform a longjmp.
 *  Longjmp related values are written to value1, value2, and iserror.
 */

struct duk_ljstate {
	duk_jmpbuf *jmpbuf_ptr;   /* current setjmp() catchpoint */
	duk_small_uint_t type;    /* longjmp type */
	duk_bool_t iserror;       /* isError flag for yield */
	duk_tval value1;          /* 1st related value (type specific) */
	duk_tval value2;          /* 2nd related value (type specific) */
};

/*
 *  Main heap structure
 */

struct duk_heap {
	duk_small_uint_t flags;

	/* allocator functions */
	duk_alloc_function alloc_func;
	duk_realloc_function realloc_func;
	duk_free_function free_func;
	void *alloc_udata;

	/* Precomputed pointers when using 16-bit heap pointer packing. */
#if defined(DUK_USE_HEAPPTR16)
	duk_uint16_t heapptr_null16;
	duk_uint16_t heapptr_deleted16;
#endif

	/* Fatal error handling, called e.g. when a longjmp() is needed but
	 * lj.jmpbuf_ptr is NULL.  fatal_func must never return; it's not
	 * declared as "noreturn" because doing that for typedefs is a bit
	 * challenging portability-wise.
	 */
	duk_fatal_function fatal_func;

	/* allocated heap objects */
	duk_heaphdr *heap_allocated;

	/* work list for objects whose refcounts are zero but which have not been
	 * "finalized"; avoids recursive C calls when refcounts go to zero in a
	 * chain of objects.
	 */
#ifdef DUK_USE_REFERENCE_COUNTING
	duk_heaphdr *refzero_list;
	duk_heaphdr *refzero_list_tail;
#endif

#ifdef DUK_USE_MARK_AND_SWEEP
	/* mark-and-sweep control */
#ifdef DUK_USE_VOLUNTARY_GC
	duk_int_t mark_and_sweep_trigger_counter;
#endif
	duk_int_t mark_and_sweep_recursion_depth;

	/* mark-and-sweep flags automatically active (used for critical sections) */
	duk_small_uint_t mark_and_sweep_base_flags;

	/* work list for objects to be finalized (by mark-and-sweep) */
	duk_heaphdr *finalize_list;
#endif

	/* longjmp state */
	duk_ljstate lj;

	/* marker for detecting internal "double faults", see duk_error_throw.c */
	duk_bool_t handling_error;

	/* heap thread, used internally and for finalization */
	duk_hthread *heap_thread;

	/* current thread */
	duk_hthread *curr_thread;	/* currently running thread */

	/* heap level "stash" object (e.g., various reachability roots) */
	duk_hobject *heap_object;

	/* heap level temporary log formatting buffer */
	duk_hbuffer_dynamic *log_buffer;

	/* duk_handle_call / duk_handle_safe_call recursion depth limiting */
	duk_int_t call_recursion_depth;
	duk_int_t call_recursion_limit;

	/* mix-in value for computing string hashes; should be reasonably unpredictable */
	duk_uint32_t hash_seed;

	/* rnd_state for duk_util_tinyrandom.c */
	duk_uint32_t rnd_state;

	/* interrupt counter */
#ifdef DUK_USE_INTERRUPT_COUNTER
	duk_int_t interrupt_init;     /* start value for current countdown */
	duk_int_t interrupt_counter;  /* countdown state (mirrored in current thread state) */
#endif

	/* string intern table (weak refs) */
#if defined(DUK_USE_HEAPPTR16)
	duk_uint16_t *strtable16;
#else
	duk_hstring **strtable;
#endif
	duk_uint32_t st_size;     /* alloc size in elements */
	duk_uint32_t st_used;     /* used elements (includes DELETED) */

	/* string access cache (codepoint offset -> byte offset) for fast string
	 * character looping; 'weak' reference which needs special handling in GC.
	 */
	duk_strcache strcache[DUK_HEAP_STRCACHE_SIZE];

	/* built-in strings */
#if defined(DUK_USE_HEAPPTR16)
	duk_uint16_t strs16[DUK_HEAP_NUM_STRINGS];
#else
	duk_hstring *strs[DUK_HEAP_NUM_STRINGS];
#endif
};

/*
 *  Prototypes
 */

DUK_INTERNAL_DECL
duk_heap *duk_heap_alloc(duk_alloc_function alloc_func,
                         duk_realloc_function realloc_func,
                         duk_free_function free_func,
                         void *alloc_udata,
                         duk_fatal_function fatal_func);
DUK_INTERNAL_DECL void duk_heap_free(duk_heap *heap);
DUK_INTERNAL_DECL void duk_heap_free_heaphdr_raw(duk_heap *heap, duk_heaphdr *hdr);

DUK_INTERNAL_DECL void duk_heap_insert_into_heap_allocated(duk_heap *heap, duk_heaphdr *hdr);
#if defined(DUK_USE_DOUBLE_LINKED_HEAP) && defined(DUK_USE_REFERENCE_COUNTING)
DUK_INTERNAL_DECL void duk_heap_remove_any_from_heap_allocated(duk_heap *heap, duk_heaphdr *hdr);
#endif
#ifdef DUK_USE_INTERRUPT_COUNTER
DUK_INTERNAL_DECL void duk_heap_switch_thread(duk_heap *heap, duk_hthread *new_thr);
#endif

#if 0  /*unused*/
DUK_INTERNAL_DECL duk_hstring *duk_heap_string_lookup(duk_heap *heap, duk_uint8_t *str, duk_uint32_t blen);
#endif
DUK_INTERNAL_DECL duk_hstring *duk_heap_string_intern(duk_heap *heap, duk_uint8_t *str, duk_uint32_t blen);
DUK_INTERNAL_DECL duk_hstring *duk_heap_string_intern_checked(duk_hthread *thr, duk_uint8_t *str, duk_uint32_t len);
#if 0  /*unused*/
DUK_INTERNAL_DECL duk_hstring *duk_heap_string_lookup_u32(duk_heap *heap, duk_uint32_t val);
#endif
DUK_INTERNAL_DECL duk_hstring *duk_heap_string_intern_u32(duk_heap *heap, duk_uint32_t val);
DUK_INTERNAL_DECL duk_hstring *duk_heap_string_intern_u32_checked(duk_hthread *thr, duk_uint32_t val);
DUK_INTERNAL_DECL void duk_heap_string_remove(duk_heap *heap, duk_hstring *h);
#if defined(DUK_USE_MARK_AND_SWEEP) && defined(DUK_USE_MS_STRINGTABLE_RESIZE)
DUK_INTERNAL_DECL void duk_heap_force_stringtable_resize(duk_heap *heap);
#endif

DUK_INTERNAL_DECL void duk_heap_strcache_string_remove(duk_heap *heap, duk_hstring *h);
DUK_INTERNAL_DECL duk_uint_fast32_t duk_heap_strcache_offset_char2byte(duk_hthread *thr, duk_hstring *h, duk_uint_fast32_t char_offset);

#ifdef DUK_USE_PROVIDE_DEFAULT_ALLOC_FUNCTIONS
DUK_INTERNAL_DECL void *duk_default_alloc_function(void *udata, duk_size_t size);
DUK_INTERNAL_DECL void *duk_default_realloc_function(void *udata, void *ptr, duk_size_t newsize);
DUK_INTERNAL_DECL void duk_default_free_function(void *udata, void *ptr);
#endif

DUK_INTERNAL_DECL void *duk_heap_mem_alloc(duk_heap *heap, duk_size_t size);
DUK_INTERNAL_DECL void *duk_heap_mem_alloc_zeroed(duk_heap *heap, duk_size_t size);
DUK_INTERNAL_DECL void *duk_heap_mem_realloc(duk_heap *heap, void *ptr, duk_size_t newsize);
DUK_INTERNAL_DECL void *duk_heap_mem_realloc_indirect(duk_heap *heap, duk_mem_getptr cb, void *ud, duk_size_t newsize);
DUK_INTERNAL_DECL void duk_heap_mem_free(duk_heap *heap, void *ptr);

#ifdef DUK_USE_REFERENCE_COUNTING
DUK_INTERNAL_DECL void duk_heap_tval_incref(duk_tval *tv);
DUK_INTERNAL_DECL void duk_heap_tval_decref(duk_hthread *thr, duk_tval *tv);
DUK_INTERNAL_DECL void duk_heap_heaphdr_incref(duk_heaphdr *h);
DUK_INTERNAL_DECL void duk_heap_heaphdr_decref(duk_hthread *thr, duk_heaphdr *h);
DUK_INTERNAL_DECL void duk_heap_refcount_finalize_heaphdr(duk_hthread *thr, duk_heaphdr *hdr);
#else
/* no refcounting */
#endif

#ifdef DUK_USE_MARK_AND_SWEEP
DUK_INTERNAL_DECL duk_bool_t duk_heap_mark_and_sweep(duk_heap *heap, duk_small_uint_t flags);
#endif

DUK_INTERNAL_DECL duk_uint32_t duk_heap_hashstring(duk_heap *heap, duk_uint8_t *str, duk_size_t len);

#endif  /* DUK_HEAP_H_INCLUDED */
