/* Test for a memory leak from just closing the lock tree manager (should close
   all lock trees. */

#include "test.h"

toku_range_tree* toku__lt_ifexist_selfwrite(toku_lock_tree* tree, DB_TXN* txn);
toku_range_tree* toku__lt_ifexist_selfread(toku_lock_tree* tree, DB_TXN* txn);

void initial_setup(void);

static int r;
static u_int32_t       lt_refs[100];
static toku_lock_tree* lts   [100];
static toku_ltm*       ltm = NULL;
static toku_db_id*     db_ids[100];
static char            subdb [100][5];
static u_int32_t max_locks = 10;
int  nums[10000];

void setup_ltm(void) {
    assert(!ltm);
    r = toku_ltm_create(&ltm, max_locks, dbpanic,
                        get_compare_fun_from_db, get_dup_compare_from_db,
                        toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    assert(ltm);
}

void db_open_tree(BOOL dups, size_t index, size_t db_id_index) {
    assert((lt_refs[index] == 0 && !lts[index]) ||
           (lt_refs[index] > 0 && lts[index]));
    assert(ltm);
    lt_refs[index]++;
    r = toku_ltm_get_lt(ltm, &lts[index], dups, db_ids[db_id_index]);
    CKERR(r);
    assert(lts[index]);
}

void db_close_tree(size_t index) {
    assert(lts[index] && ltm && lt_refs[index] > 0);
    r = toku_lt_remove_ref(lts[index]); CKERR(r);
    lt_refs[index]--;
    if (lt_refs[index] == 0) { lts[index] = NULL; }
}

void txn_open_tree(size_t index) {
    assert(lts[index] && ltm && lt_refs[index] > 0);
    toku_lt_add_ref(lts[index]);
    lt_refs[index]++;
}

void txn_close_tree(size_t index) {
    assert(lts[index] && ltm && lt_refs[index] > 0);
    r = toku_lt_remove_ref(lts[index]); CKERR(r);
    lt_refs[index]--;
    if (lt_refs[index] == 0) { lts[index] = NULL; }
}

void close_ltm(void) {
    assert(ltm);
    r = toku_ltm_close(ltm);
    CKERR(r);
    initial_setup();
    ltm = NULL;
}

void run_test(BOOL dups) {
    setup_ltm();
    //Start:

    /* ********************************************************************** */
    //Open and close.
    db_open_tree(dups, 0, 0);
    db_close_tree(0);
    /* ********************************************************************** */
    //Open with db and transaction, db closes first.
    db_open_tree(dups, 0, 0);
    txn_open_tree(0);
    db_close_tree(0);
    txn_close_tree(0);
    /* ********************************************************************** */
    //Open with db and transaction, txn closes first.
    db_open_tree(dups, 0, 0);
    txn_open_tree(0);
    txn_close_tree(0);
    db_close_tree(0);
    /* ********************************************************************** */
    //Open with multiple db handles.
    db_open_tree(dups, 0, 0);
    db_open_tree(dups, 0, 0);
    db_close_tree(0);
    db_close_tree(0);
    /* ********************************************************************** */
    //Open with multiple db handles and txns.
    db_open_tree(dups, 0, 0);
    txn_open_tree(0);
    db_open_tree(dups, 0, 0);
    db_close_tree(0);
    db_close_tree(0);
    txn_close_tree(0);
    /* ********************************************************************** */
    //Open with multiple db handles and txns.
    db_open_tree(dups, 0, 0);
    db_open_tree(dups, 0, 0);
    txn_open_tree(0);
    db_close_tree(0);
    db_close_tree(0);
    txn_close_tree(0);
    /* ********************************************************************** */

    //End:
    close_ltm();
}

void initial_setup(void) {
    u_int32_t i;

    ltm = NULL;
    assert(sizeof(db_ids) / sizeof(db_ids[0]) == sizeof(lts) / sizeof(lts[0]));
    assert(sizeof(subdb) / sizeof(subdb[0]) == sizeof(lts) / sizeof(lts[0]));
    for (i = 0; i < sizeof(lts) / sizeof(lts[0]); i++) {
        lts[i] = NULL;
        sprintf(subdb[i], "%05x", i);
        if (!db_ids[i]) toku_db_id_create(&db_ids[i], DIR, subdb[i]);
        assert(db_ids[i]);
        lt_refs[i] = 0;
    }
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);
    compare_fun = intcmp;
    dup_compare = intcmp;

    system("rm -rf " DIR);
    mkdir(DIR, 0777);

    initial_setup();

    run_test(FALSE);
    
    run_test(TRUE);

    return 0;
}
