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
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <sys/time.h>
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include "sql_accelerate.h"

#define NUM_ELEMENTS 				1048576
#define PORT     					8080

float4 *input_array;
float4 *arr_ptr;

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1( get_data );

// UDF Arguments:
// ARGS[0] - Relation Name
Datum get_data( PG_FUNCTION_ARGS )
{
    // variable declarations
    text *relname;
    List *names_list;
    RangeVar *rel_rv;
    Relation rel;
    struct timeval time_start, time_end;
    int num_blks;
    unsigned char *p_blk;
	unsigned num_tups;
    bool real_pages;
    ModFillState modFillState;
    TupleDesc tup_desc;
    float4 avg;
    
    // Start the timer
    gettimeofday(&time_start, NULL);

	input_array = (float4 *)palloc(sizeof(float4) * NUM_ELEMENTS);
	arr_ptr = input_array;

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
    
    // At this point, the input array has been filled and the data can be sent to fpga
    avg = client_send_data();

    pfree(p_blk);
    
    pfree(input_array);

    relation_close(rel, AccessShareLock);
    
    // End the timer and print execution time to the postgres console
    gettimeofday(&time_end, NULL);
    ereport(INFO,
		(errcode(ERRCODE_SUCCESSFUL_COMPLETION),
		errmsg("C UDF execution time: %ld ms\n", 
			((time_end.tv_sec * 1000000 + time_end.tv_usec) - (time_start.tv_sec * 1000000 + time_start.tv_usec)) / 1000)));
    
    PG_RETURN_FLOAT4(avg);
}

// Routines for retreiving data from Postgres
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

			// get the id of current tuple
            id = PageGetItemId(page, line_num);
            lp_len = ItemIdGetLength(id);     
            
            tup_hdr = (HeapTupleHeader) PageGetItem(page, id);

			// user data (columns) begin at the offset indicated by t_hoff
            tuple_data_len = lp_len - tup_hdr->t_hoff;
            if (tuple_data_len == 0) {
                continue;
            }

			// if we have a valid tuple, get the target value
            if (p_dst + tuple_data_len < p_dst_end) {
				
				// copy only the user data into the p_dst buffer
                memcpy(p_dst, (unsigned char*) tup_hdr + tup_hdr->t_hoff, tuple_data_len);
                
                // retrieves an item (tuple) from the given page
				tmp_tup.t_len = HeapTupleHeaderGetDatumLength(tup_hdr);
				ItemPointerSetInvalid(&(tmp_tup.t_self));
				tmp_tup.t_tableOid = InvalidOid;
				tmp_tup.t_data = tup_hdr;
                
                // get the value of the 4th field in the relation (returned as a Datum)
				result = heap_getattr(&tmp_tup, 4, tup_desc, &isnull);
				
				// add the float value to our input array
				*arr_ptr = (float4)DatumGetFloat4(result);
				arr_ptr++; 

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

float client_send_data()
{
	int sockfd;
	char *server_ip = "192.168.2.7";
	struct sockaddr_in server;
	float recv_message;
	int sent_total, sent_now, mem_size;

	// Server information
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr(server_ip);
	
	// Get a stream socket
	if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    }

	// Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0) {
		perror("Connect()");
        exit(4);
	}
	
	sent_total = 0;
	mem_size = NUM_ELEMENTS * sizeof(float);
    
	// send floating point values to the DE1-Soc over network
	// I already know that both architectures are Little Endian so I will bypass the hton() call
	for (sent_now = 0; sent_total < mem_size; sent_total += sent_now) {
		
		sent_now = send(sockfd, input_array+sent_total, mem_size - sent_total, 0);
		
		ereport(INFO,
			(errcode(ERRCODE_SUCCESSFUL_COMPLETION),
			errmsg("Bytes sent: %d\n", sent_now)));
		
		if (sent_now == -1) break;
		
	}
	
	// Receive message back from server
	if (recv(sockfd, &recv_message, sizeof(float), 0) < 0) {
		perror("Recv()");
        exit(6);
	}
  
    close(sockfd); 
    return recv_message;	
}

