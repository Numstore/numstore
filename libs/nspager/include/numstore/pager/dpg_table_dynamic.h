/**
 * @file dpg_table_dynamic.h
 * @brief Dynamic dirty page table using adaptive hash table
 *
 * This module implements a dynamic dirty page table that uses adptv_htable
 * as its underlying data structure. Similar to txn_table, this is a stack-based
 * transparent struct that manages its own memory using malloc/free.
 */

#ifndef NUMSTORE_PAGER_DPG_TABLE_DYNAMIC_H
#define NUMSTORE_PAGER_DPG_TABLE_DYNAMIC_H

#include <numstore/core/adptv_hash_table.h>
#include <numstore/core/error.h>
#include <numstore/core/spx_latch.h>
#include <numstore/intf/types.h>

/**
 * @brief Entry in the dynamic dirty page table
 *
 * Each entry tracks a dirty page with its recovery LSN.
 * Entries are dynamically allocated and freed by the table.
 */
struct dpg_entry_dynamic {
	pgno pg;                /**< Page number (key) */
	lsn rec_lsn;            /**< Recovery LSN */
	struct spx_latch l;     /**< Entry-level latch */
	struct hnode node;      /**< Hash table node */
};

/**
 * @brief Dynamic dirty page table
 *
 * Stack-based transparent struct that manages a set of dirty pages.
 * Uses adptv_htable for dynamic resizing and manages entry memory internally.
 */
struct dpg_table_dynamic {
	struct adptv_htable t;  /**< Underlying adaptive hash table */
	struct spx_latch l;     /**< Table-level latch */
};

/**
 * @brief Initialize a dynamic dirty page table
 *
 * @param dest Pointer to table to initialize
 * @param e Error context
 * @return OK on success, error code on failure
 */
err_t dpgt_dyn_open(struct dpg_table_dynamic *dest, error *e);

/**
 * @brief Close and cleanup a dynamic dirty page table
 *
 * Frees all entries and releases resources. Table can be safely
 * destroyed after this call.
 *
 * @param t Pointer to table to close
 */
void dpgt_dyn_close(struct dpg_table_dynamic *t);

/**
 * @brief Insert a dirty page entry
 *
 * Creates and inserts a new entry for the given page number.
 * If the page already exists, returns an error.
 *
 * @param t Table to insert into
 * @param pg Page number
 * @param rec_lsn Recovery LSN
 * @param e Error context
 * @return OK on success, ERR_EXISTS if page already exists
 */
err_t dpgt_dyn_add(struct dpg_table_dynamic *t, pgno pg, lsn rec_lsn, error *e);

/**
 * @brief Insert or update a dirty page entry
 *
 * If the page doesn't exist, creates a new entry.
 * If it exists, updates the recovery LSN.
 *
 * @param t Table to insert/update
 * @param pg Page number
 * @param rec_lsn Recovery LSN
 * @param e Error context
 * @return OK on success, error code on failure
 */
err_t dpgt_dyn_add_or_update(struct dpg_table_dynamic *t, pgno pg, lsn rec_lsn, error *e);

/**
 * @brief Get a dirty page entry
 *
 * Looks up an entry by page number. Copies the entry data to dest.
 *
 * @param dest Pointer to store entry data (if found)
 * @param t Table to search
 * @param pg Page number to look up
 * @return true if found, false otherwise
 */
bool dpgt_dyn_get(struct dpg_entry_dynamic *dest, struct dpg_table_dynamic *t, pgno pg);

/**
 * @brief Get a dirty page entry (expects to exist)
 *
 * Looks up an entry and asserts it exists. Use when you know the page
 * must be in the table.
 *
 * @param dest Pointer to store entry data
 * @param t Table to search
 * @param pg Page number to look up
 */
void dpgt_dyn_get_expect(struct dpg_entry_dynamic *dest, struct dpg_table_dynamic *t, pgno pg);

/**
 * @brief Check if a page exists in the table
 *
 * @param t Table to search
 * @param pg Page number
 * @return true if page exists, false otherwise
 */
bool dpgt_dyn_exists(struct dpg_table_dynamic *t, pgno pg);

/**
 * @brief Remove a dirty page entry
 *
 * Removes and frees the entry for the given page number.
 *
 * @param exists Output: true if entry existed and was removed
 * @param t Table to remove from
 * @param pg Page number to remove
 * @param e Error context
 * @return OK on success, error code on failure
 */
err_t dpgt_dyn_remove(bool *exists, struct dpg_table_dynamic *t, pgno pg, error *e);

/**
 * @brief Remove a dirty page entry (expects to exist)
 *
 * Removes the entry and asserts it existed.
 *
 * @param t Table to remove from
 * @param pg Page number to remove
 * @param e Error context
 * @return OK on success, error code on failure
 */
err_t dpgt_dyn_remove_expect(struct dpg_table_dynamic *t, pgno pg, error *e);

/**
 * @brief Update the recovery LSN for a page
 *
 * Updates an existing entry's recovery LSN. Asserts the page exists.
 *
 * @param t Table containing the entry
 * @param pg Page number
 * @param new_rec_lsn New recovery LSN
 */
void dpgt_dyn_update(struct dpg_table_dynamic *t, pgno pg, lsn new_rec_lsn);

/**
 * @brief Get the current size of the table
 *
 * @param t Table to query
 * @return Number of entries in the table
 */
u32 dpgt_dyn_get_size(struct dpg_table_dynamic *t);

/**
 * @brief Find the minimum recovery LSN across all entries
 *
 * Scans all entries to find the minimum recovery LSN.
 * Returns INVALID_LSN if table is empty.
 *
 * @param t Table to scan
 * @return Minimum recovery LSN, or INVALID_LSN if empty
 */
lsn dpgt_dyn_min_rec_lsn(struct dpg_table_dynamic *t);

/**
 * @brief Iterate over all entries in the table
 *
 * Calls the action function for each entry with the provided context.
 *
 * @param t Table to iterate
 * @param action Function to call for each entry
 * @param ctx User context passed to action
 */
void dpgt_dyn_foreach(struct dpg_table_dynamic *t,
                      void (*action)(struct dpg_entry_dynamic *, void *ctx),
                      void *ctx);

/**
 * @brief Merge source table into destination table
 *
 * Moves all entries from src to dest. Entries in src are removed but not freed,
 * as ownership transfers to dest. After merge, src is empty.
 *
 * @param dest Destination table
 * @param src Source table (will be emptied)
 * @param e Error context
 * @return OK on success, error code on failure
 */
err_t dpgt_dyn_merge_into(struct dpg_table_dynamic *dest,
                          struct dpg_table_dynamic *src,
                          error *e);

/**
 * @brief Get the serialized size of the table
 *
 * @param t Table to serialize
 * @return Size in bytes needed to serialize
 */
u32 dpgt_dyn_get_serialize_size(struct dpg_table_dynamic *t);

/**
 * @brief Serialize the table to a byte buffer
 *
 * @param dest Destination buffer
 * @param dlen Destination buffer length
 * @param t Table to serialize
 * @return Number of bytes written
 */
u32 dpgt_dyn_serialize(u8 *dest, u32 dlen, struct dpg_table_dynamic *t);

/**
 * @brief Deserialize a table from a byte buffer
 *
 * @param dest Table to deserialize into (must be uninitialized)
 * @param src Source buffer
 * @param slen Source buffer length
 * @param e Error context
 * @return OK on success, error code on failure
 */
err_t dpgt_dyn_deserialize(struct dpg_table_dynamic *dest,
                           const u8 *src,
                           u32 slen,
                           error *e);

/**
 * @brief Check if two tables are equal
 *
 * @param left First table
 * @param right Second table
 * @return true if tables contain the same entries, false otherwise
 */
bool dpgt_dyn_equals(struct dpg_table_dynamic *left, struct dpg_table_dynamic *right);

#endif /* NUMSTORE_PAGER_DPG_TABLE_DYNAMIC_H */
