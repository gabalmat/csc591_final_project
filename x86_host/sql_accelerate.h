#define BLOCK_SZ (2*1024*1024)
#define PAGE_SZ 8192    // 8K
#define PAGES_PER_BLK (BLOCK_SZ/PAGE_SZ)   // 256

typedef struct ModFillState {
    int blk_num;
    int num_blks;
    int line_num;
} ModFillState;

Datum get_data(PG_FUNCTION_ARGS);
bool fill_block(Relation, unsigned char*, unsigned*, ModFillState*, TupleDesc);
int get_next_page(Relation, unsigned char*, ModFillState*, TupleDesc);
float client_send_data(void);
