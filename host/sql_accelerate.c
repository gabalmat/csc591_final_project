#include "postgres.h"
#include "fmgr.h"

#include "access/heapam.h"
#include "access/htup_details.h"
#include "catalog/namespace.h"
#include "executor/executor.h"
#include "storage/bufmgr.h"
#include "utils/builtins.h"
#include "utils/rel.h"
#include "utils/typcache.h"
#include "utils/varlena.h"
#include "funcapi.h"
#include <string.h>

#define BLOCK_SZ (2*1024*1024)
#define PAGE_SZ 8192    // 8K
#define PAGES_PER_BLK (BLOCK_SZ/PAGE_SZ)   // 256
#define NUM_ELEMENTS 149

typedef struct ModFillState {
    int blk_num;
    int num_blks;
    int line_num;
} ModFillState;

float4 *input_array;
float4 *arr_ptr;

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1( get_data );

Datum get_data(PG_FUNCTION_ARGS);
bool fill_block(Relation, unsigned char*, unsigned*, ModFillState*, TupleDesc);
int get_next_page(Relation, unsigned char*, ModFillState*, TupleDesc);

// UDF Arguments:
// ARGS[0] - Relation Name
// ARGS[1] - number of tuples to process
Datum get_data( PG_FUNCTION_ARGS )
{
    // variable declarations
    text *relname;
    List *names_list;
    RangeVar *rel_rv;
    Relation rel;
    int num_blks;
    unsigned char *p_blk;
	unsigned num_tups;
    bool real_pages;
    ModFillState modFillState;
    TupleDesc tup_desc;
    int i;
    float4 sum;
    float4 avg;

	input_array = (float4 *)palloc(sizeof(float4) * NUM_ELEMENTS);
	arr_ptr = input_array;
	sum = 0;

    relname = PG_GETARG_TEXT_PP(0);

    names_list = textToQualifiedNameList(relname);
    rel_rv = makeRangeVarFromNameList(names_list);
    rel = relation_openrv(rel_rv, AccessShareLock);
    tup_desc = CreateTupleDescCopy(RelationGetDescr(rel));

    num_blks = RelationGetNumberOfBlocksInFork(rel, MAIN_FORKNUM);
    p_blk = (unsigned char *) palloc(BLOCK_SZ);

    modFillState.num_blks = num_blks;
    modFillState.blk_num = 0;
    modFillState.line_num = 0;

    num_tups = 0;

    real_pages = true;
    while(real_pages) {
        real_pages = fill_block(rel, p_blk, &num_tups, &modFillState, tup_desc);
    }
    
    // At this point, the input array has been filled and can be sent to fpga
    
    
    //for (i = 0; i < NUM_ELEMENTS; ++i) {
		//sum += input_array[i];
		
		//ereport(INFO,
			//(errcode(ERRCODE_SUCCESSFUL_COMPLETION),
			//errmsg("value: %f\n", input_array[i])));
	//}
	
	//avg = sum / NUM_ELEMENTS;
	
	//ereport(INFO,
			//(errcode(ERRCODE_SUCCESSFUL_COMPLETION),
			//errmsg("average: %f\n", avg)));

    pfree(p_blk);
    
    pfree(input_array);

    relation_close(rel, AccessShareLock);

    //PG_RETURN_INT32(num_tups);
    
    PG_RETURN_FLOAT4(avg);
}

// Routines for retreiving data from PG
bool fill_block(Relation rel, unsigned char *p_buf, unsigned *p_num_tups,
                ModFillState *p_fill_state, TupleDesc tup_desc)
{
    // variable declarations
    bool real_pages;
    int pg_num;

    real_pages = true;

    for (pg_num = 0; pg_num < PAGES_PER_BLK; ++pg_num) {
        int pg_tups;

        pg_tups = get_next_page(rel, p_buf, p_fill_state, tup_desc);
        real_pages = (pg_tups != 0);

        *p_num_tups += pg_tups;

         p_buf += PAGE_SZ;
    }

    return real_pages;
}

int get_next_page(Relation rel, unsigned char *p_buf, ModFillState *p_fill_state, TupleDesc tup_desc)
{
    // variable declarations
    unsigned char *p_dst;
    unsigned char *p_dst_end;
    int num_tups;
    bool buf_filled;
    int blk_num;
    int line_num;

    Buffer buf;
    Page page;
    int num_lines;

    ItemId id;
    uint16 lp_len;
    HeapTupleHeader tup_hdr;
    
    int tuple_data_len;

    p_dst = p_buf;
    p_dst_end = p_buf + PAGE_SZ;
    num_tups = 0;
    line_num = 0;
    buf_filled = false;

    p_dst += 2;

    // Continuously retrieve blocks of data from PG
    for (blk_num = p_fill_state->blk_num; blk_num < p_fill_state->num_blks; ++blk_num)
    {
        // retrieve a buffer containing the requested block of the requested relation
        // RBM_NORMAL mode -the page is read from disk, and the page header is validated.
        buf = ReadBufferExtended(rel, MAIN_FORKNUM, blk_num, RBM_NORMAL, NULL);
        LockBuffer(buf, BUFFER_LOCK_SHARE);

        page = (Page) BufferGetPage(buf);
        num_lines = PageGetMaxOffsetNumber(page);

        line_num = p_fill_state->line_num;
        p_fill_state->line_num = 0;
        for ( ; line_num <= num_lines; ++line_num) {
            HeapTupleData tmp_tup;
            Datum result;
            bool isnull;
			//float4 value;

			// get the id of current tuple
            id = PageGetItemId(page, line_num);
            lp_len = ItemIdGetLength(id);

			// retrieves an item (tuple) from the given page
            tup_hdr = (HeapTupleHeader) PageGetItem(page, id);
            tmp_tup.t_len = HeapTupleHeaderGetDatumLength(tup_hdr);
            ItemPointerSetInvalid(&(tmp_tup.t_self));
            tmp_tup.t_tableOid = InvalidOid;
            tmp_tup.t_data = tup_hdr;
            
			// get the value of the 4th field in the relation (returned as a Datum)
            result = heap_getattr(&tmp_tup, 4, tup_desc, &isnull);

			// add the float value to our input array
			*arr_ptr = (float4)DatumGetFloat4(result);
			arr_ptr++;      

			// user data (columns) begin at the offset indicated by t_hoff
            tuple_data_len = lp_len - tup_hdr->t_hoff;
            if (tuple_data_len == 0) {
                continue;
            }

            if (p_dst + tuple_data_len < p_dst_end) {
				// copy only the user data into the p_dst buffer
                memcpy(p_dst, (unsigned char*) tup_hdr + tup_hdr->t_hoff, tuple_data_len);

                p_dst += tuple_data_len;
                ++ num_tups;

            } else {
                buf_filled = true;
                break;
            }
        }

        LockBuffer(buf, BUFFER_LOCK_UNLOCK);
        ReleaseBuffer(buf);

        if (buf_filled) break;
    }

    p_buf[0] = (unsigned char) (num_tups & 0xFF);
    p_buf[1] = (unsigned char) (num_tups >> 8 & 0xFF);
    p_fill_state->blk_num = blk_num;
    p_fill_state->line_num = line_num;

    return num_tups;
}
