#include <check.h>
#include <talloc.h>
#include <inttypes.h>
#include <signal.h>
#include "../../src/array.h"
#include "../test_utils.h"

// Test successful array creation
START_TEST(test_array_create_success) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);

    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;
    ck_assert_ptr_nonnull(array);
    ck_assert_ptr_null(array->data); // Lazy allocation - no data yet
    ck_assert_uint_eq(array->element_size, sizeof(int32_t));
    ck_assert_uint_eq(array->size, 0);
    ck_assert_uint_eq(array->capacity, 0);
    ck_assert_uint_eq(array->increment, 10);

    talloc_free(ctx);
}
END_TEST
// Test array creation with invalid element_size (0)
START_TEST(test_array_create_invalid_element_size)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, 0, 10);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_INVALID_ARG);
    ck_assert_ptr_nonnull(strstr(error_message(res.err), "element_size"));

    talloc_free(ctx);
}

END_TEST
// Test array creation with invalid increment (0)
START_TEST(test_array_create_invalid_increment)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 0);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_INVALID_ARG);
    ck_assert_ptr_nonnull(strstr(error_message(res.err), "increment"));

    talloc_free(ctx);
}

END_TEST
// Test OOM during array creation
START_TEST(test_array_create_oom)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    oom_test_fail_next_alloc();
    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);
    oom_test_reset();

    talloc_free(ctx);
}

END_TEST
// Test array_size on empty array
START_TEST(test_array_size_empty)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    size_t size = ik_array_size(array);
    ck_assert_uint_eq(size, 0);

    talloc_free(ctx);
}

END_TEST
// Test array_capacity on empty array
START_TEST(test_array_capacity_empty)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    size_t capacity = ik_array_capacity(array);
    ck_assert_uint_eq(capacity, 0);

    talloc_free(ctx);
}

END_TEST
// Test appending to empty array (first allocation)
START_TEST(test_array_append_first)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    int32_t value = 42;
    res = ik_array_append(array, &value);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(array->size, 1);
    ck_assert_uint_eq(array->capacity, 10); // First allocation uses increment
    ck_assert_ptr_nonnull(array->data);

    // Verify the value was stored
    int32_t *stored = (int32_t *)ik_array_get(array, 0);
    ck_assert_int_eq(*stored, 42);

    talloc_free(ctx);
}

END_TEST
// Test appending within capacity (no growth)
START_TEST(test_array_append_no_growth)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Append 5 values (within capacity of 10)
    for (int32_t i = 0; i < 5; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    ck_assert_uint_eq(array->size, 5);
    ck_assert_uint_eq(array->capacity, 10);

    // Verify values
    for (int32_t i = 0; i < 5; i++) {
        int32_t *val = (int32_t *)ik_array_get(array, (size_t)i);
        ck_assert_int_eq(*val, i);
    }

    talloc_free(ctx);
}

END_TEST
// Test appending that triggers growth (doubling)
START_TEST(test_array_append_with_growth)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 2);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Append 5 values: capacity goes 0 -> 2 -> 4 -> 8
    for (int32_t i = 0; i < 5; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    ck_assert_uint_eq(array->size, 5);
    ck_assert_uint_eq(array->capacity, 8); // 2 -> 4 -> 8

    // Verify values survived growth
    for (int32_t i = 0; i < 5; i++) {
        int32_t *val = (int32_t *)ik_array_get(array, (size_t)i);
        ck_assert_int_eq(*val, i);
    }

    talloc_free(ctx);
}

END_TEST
// Test OOM during first allocation in append
START_TEST(test_array_append_oom_first_alloc)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    int32_t value = 42;
    oom_test_fail_next_alloc();
    res = ik_array_append(array, &value);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);
    oom_test_reset();

    // Array should be unchanged
    ck_assert_uint_eq(array->size, 0);
    ck_assert_uint_eq(array->capacity, 0);
    ck_assert_ptr_null(array->data);

    talloc_free(ctx);
}

END_TEST
// Test OOM during growth realloc
START_TEST(test_array_append_oom_growth)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 2);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Fill to capacity
    for (int32_t i = 0; i < 2; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Now try to append with OOM during growth
    int32_t value = 99;
    oom_test_fail_next_alloc();
    res = ik_array_append(array, &value);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);
    oom_test_reset();

    // Array should retain old values
    ck_assert_uint_eq(array->size, 2);
    ck_assert_uint_eq(array->capacity, 2);

    talloc_free(ctx);
}

END_TEST
// Test insert at beginning
START_TEST(test_array_insert_at_beginning)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Add some values
    for (int32_t i = 0; i < 3; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Insert at beginning
    int32_t value = 99;
    res = ik_array_insert(array, 0, &value);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(array->size, 4);

    // Verify order: [99, 0, 1, 2]
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 99);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), 0);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 1);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 3), 2);

    talloc_free(ctx);
}

END_TEST
// Test insert in middle
START_TEST(test_array_insert_in_middle)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Add values [0, 1, 2, 3]
    for (int32_t i = 0; i < 4; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Insert 99 at index 2
    int32_t value = 99;
    res = ik_array_insert(array, 2, &value);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(array->size, 5);

    // Verify order: [0, 1, 99, 2, 3]
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 0);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), 1);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 99);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 3), 2);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 4), 3);

    talloc_free(ctx);
}

END_TEST
// Test insert at end (equivalent to append)
START_TEST(test_array_insert_at_end)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Add values [0, 1, 2]
    for (int32_t i = 0; i < 3; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Insert at end (index == size)
    int32_t value = 99;
    res = ik_array_insert(array, 3, &value);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(array->size, 4);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 3), 99);

    talloc_free(ctx);
}

END_TEST
// Test insert with growth
START_TEST(test_array_insert_with_growth)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 2);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Fill to capacity
    for (int32_t i = 0; i < 2; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Insert should trigger growth
    int32_t value = 99;
    res = ik_array_insert(array, 1, &value);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(array->size, 3);
    ck_assert_uint_eq(array->capacity, 4); // 2 -> 4

    // Verify order: [0, 99, 1]
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 0);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), 99);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 1);

    talloc_free(ctx);
}

END_TEST
// Test insert OOM
START_TEST(test_array_insert_oom)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 2);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Fill to capacity
    for (int32_t i = 0; i < 2; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Trigger OOM during growth
    int32_t value = 99;
    oom_test_fail_next_alloc();
    res = ik_array_insert(array, 0, &value);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);
    oom_test_reset();

    // Array should be unchanged
    ck_assert_uint_eq(array->size, 2);
    ck_assert_uint_eq(array->capacity, 2);

    talloc_free(ctx);
}

END_TEST
// Test delete from beginning
START_TEST(test_array_delete_from_beginning)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Add [0, 1, 2, 3]
    for (int32_t i = 0; i < 4; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Delete first element
    ik_array_delete(array, 0);

    ck_assert_uint_eq(array->size, 3);
    // Verify: [1, 2, 3]
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 1);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), 2);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 3);

    talloc_free(ctx);
}

END_TEST
// Test delete from middle
START_TEST(test_array_delete_from_middle)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Add [0, 1, 2, 3, 4]
    for (int32_t i = 0; i < 5; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Delete middle element
    ik_array_delete(array, 2);

    ck_assert_uint_eq(array->size, 4);
    // Verify: [0, 1, 3, 4]
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 0);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), 1);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 3);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 3), 4);

    talloc_free(ctx);
}

END_TEST
// Test delete from end
START_TEST(test_array_delete_from_end)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Add [0, 1, 2]
    for (int32_t i = 0; i < 3; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Delete last element
    ik_array_delete(array, 2);

    ck_assert_uint_eq(array->size, 2);
    // Verify: [0, 1]
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 0);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), 1);

    talloc_free(ctx);
}

END_TEST
// Test set element
START_TEST(test_array_set)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Add [0, 1, 2]
    for (int32_t i = 0; i < 3; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Set middle element
    int32_t new_value = 99;
    ik_array_set(array, 1, &new_value);

    ck_assert_uint_eq(array->size, 3);
    // Verify: [0, 99, 2]
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 0);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), 99);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 2);

    talloc_free(ctx);
}

END_TEST
// Test clear array
START_TEST(test_array_clear)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Add some elements
    for (int32_t i = 0; i < 5; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    ck_assert_uint_eq(array->size, 5);
    ck_assert_uint_eq(array->capacity, 10);

    // Clear array
    ik_array_clear(array);

    ck_assert_uint_eq(array->size, 0);
    ck_assert_uint_eq(array->capacity, 10); // Capacity unchanged
    ck_assert_ptr_nonnull(array->data); // Data buffer still allocated

    talloc_free(ctx);
}

END_TEST
// Security test: Use-after-free attempt via stale pointer after reallocation
START_TEST(test_array_stale_pointer_after_reallocation)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 2);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Fill to capacity
    int32_t value = 42;
    res = ik_array_append(array, &value);
    ck_assert(is_ok(&res));

    // Get pointer to first element
    int32_t *ptr = (int32_t *)ik_array_get(array, 0);
    ck_assert_ptr_nonnull(ptr);
    ck_assert_int_eq(*ptr, 42);

    // Save the old data pointer
    void *old_data = array->data;

    // Force reallocation by appending more elements (capacity 2 -> 4)
    for (int32_t i = 0; i < 5; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Note: We don't check if array->data changed because realloc may expand in place
    // The important lesson: don't keep pointers into the array across modifications
    (void)old_data; // Suppress unused warning

    // The old pointer 'ptr' may point to freed memory (use-after-free if reallocated)
    // We cannot safely use it, but we can verify the data is still correct via new get
    int32_t *new_ptr = (int32_t *)ik_array_get(array, 0);
    ck_assert_int_eq(*new_ptr, 42);

    // Verify all data survived reallocation
    ck_assert_uint_eq(array->size, 6);
    for (size_t i = 1; i < 6; i++) {
        int32_t *val = (int32_t *)ik_array_get(array, i);
        ck_assert_int_eq(*val, (int32_t)(i - 1));
    }

    talloc_free(ctx);
}

END_TEST
// Security test: Delete all elements one by one (check for size underflow)
START_TEST(test_array_delete_all_elements_no_underflow)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Add 5 elements
    for (int32_t i = 0; i < 5; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    ck_assert_uint_eq(array->size, 5);

    // Delete all elements one by one from the end
    for (size_t i = 0; i < 5; i++) {
        ik_array_delete(array, 0);
        ck_assert_uint_eq(array->size, 4 - i);
    }

    // Verify size is exactly 0, not wrapped around to SIZE_MAX
    ck_assert_uint_eq(array->size, 0);
    ck_assert_uint_eq(array->capacity, 10); // Capacity unchanged

    talloc_free(ctx);
}

END_TEST
// Security test: Complex interleaved insert/delete sequence
START_TEST(test_array_interleaved_insert_delete_stress)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Build initial array [0, 1, 2, 3, 4]
    for (int32_t i = 0; i < 5; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Delete middle element -> [0, 1, 3, 4]
    ik_array_delete(array, 2);
    ck_assert_uint_eq(array->size, 4);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 3);

    // Insert at beginning -> [99, 0, 1, 3, 4]
    int32_t value = 99;
    res = ik_array_insert(array, 0, &value);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(array->size, 5);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 99);

    // Delete from beginning -> [0, 1, 3, 4]
    ik_array_delete(array, 0);
    ck_assert_uint_eq(array->size, 4);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 0);

    // Insert in middle -> [0, 1, 88, 3, 4]
    value = 88;
    res = ik_array_insert(array, 2, &value);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(array->size, 5);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 88);

    // Delete from end -> [0, 1, 88, 3]
    ik_array_delete(array, 4);
    ck_assert_uint_eq(array->size, 4);

    // Insert at end -> [0, 1, 88, 3, 77]
    value = 77;
    res = ik_array_insert(array, 4, &value);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(array->size, 5);

    // Verify final state: [0, 1, 88, 3, 77]
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 0);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), 1);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 88);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 3), 3);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 4), 77);

    talloc_free(ctx);
}

END_TEST
// Security test: Clear then append (verify array still works)
START_TEST(test_array_clear_then_append)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Add elements
    for (int32_t i = 0; i < 5; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Clear
    ik_array_clear(array);
    ck_assert_uint_eq(array->size, 0);
    ck_assert_uint_eq(array->capacity, 10);

    // Append new elements (should reuse existing capacity)
    for (int32_t i = 100; i < 103; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Verify new elements
    ck_assert_uint_eq(array->size, 3);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 100);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), 101);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 102);

    talloc_free(ctx);
}

END_TEST
// Security test: Repeated insert/delete at same position
START_TEST(test_array_repeated_insert_delete_same_position)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ck_assert(is_ok(&res));
    ik_array_t *array = res.ok;

    // Build initial array [0, 1, 2]
    for (int32_t i = 0; i < 3; i++) {
        res = ik_array_append(array, &i);
        ck_assert(is_ok(&res));
    }

    // Repeatedly insert and delete at position 1
    for (int32_t i = 0; i < 10; i++) {
        int32_t value = 99 + i;
        res = ik_array_insert(array, 1, &value);
        ck_assert(is_ok(&res));
        ck_assert_uint_eq(array->size, 4);
        ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), value);

        ik_array_delete(array, 1);
        ck_assert_uint_eq(array->size, 3);
    }

    // Verify original elements intact: [0, 1, 2]
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 0), 0);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 1), 1);
    ck_assert_int_eq(*(int32_t *)ik_array_get(array, 2), 2);

    talloc_free(ctx);
}

END_TEST

#ifndef NDEBUG
// Test assertion: get with NULL array
START_TEST(test_array_get_null_array_asserts)
{
    ik_array_get(NULL, 0);
}

END_TEST
// Test assertion: get with out of bounds index
START_TEST(test_array_get_out_of_bounds_asserts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ik_array_t *array = res.ok;

    // Access beyond bounds
    ik_array_get(array, 0); // size is 0, so any index is out of bounds

    talloc_free(ctx);
}

END_TEST
// Test assertion: append with NULL array
START_TEST(test_array_append_null_array_asserts)
{
    int32_t value = 42;
    ik_array_append(NULL, &value);
}

END_TEST
// Test assertion: append with NULL element
START_TEST(test_array_append_null_element_asserts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ik_array_t *array = res.ok;

    ik_array_append(array, NULL);

    talloc_free(ctx);
}

END_TEST
// Test assertion: insert with invalid index
START_TEST(test_array_insert_invalid_index_asserts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ik_array_t *array = res.ok;

    int32_t value = 42;
    // Insert at index > size is invalid
    ik_array_insert(array, 1, &value); // size is 0, so index 1 is invalid

    talloc_free(ctx);
}

END_TEST
// Test assertion: delete with invalid index
START_TEST(test_array_delete_invalid_index_asserts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ik_array_t *array = res.ok;

    // Delete from empty array
    ik_array_delete(array, 0);

    talloc_free(ctx);
}

END_TEST
// Test assertion: set with invalid index
START_TEST(test_array_set_invalid_index_asserts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ik_array_t *array = res.ok;

    int32_t value = 42;
    // Set on empty array
    ik_array_set(array, 0, &value);

    talloc_free(ctx);
}

END_TEST
// Test assertion: size with NULL array
START_TEST(test_array_size_null_array_asserts)
{
    ik_array_size(NULL);
}

END_TEST
#endif

// Test suite setup
static Suite *array_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Array");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_array_create_success);
    tcase_add_test(tc_core, test_array_create_invalid_element_size);
    tcase_add_test(tc_core, test_array_create_invalid_increment);
    tcase_add_test(tc_core, test_array_create_oom);
    tcase_add_test(tc_core, test_array_size_empty);
    tcase_add_test(tc_core, test_array_capacity_empty);
    tcase_add_test(tc_core, test_array_append_first);
    tcase_add_test(tc_core, test_array_append_no_growth);
    tcase_add_test(tc_core, test_array_append_with_growth);
    tcase_add_test(tc_core, test_array_append_oom_first_alloc);
    tcase_add_test(tc_core, test_array_append_oom_growth);
    tcase_add_test(tc_core, test_array_insert_at_beginning);
    tcase_add_test(tc_core, test_array_insert_in_middle);
    tcase_add_test(tc_core, test_array_insert_at_end);
    tcase_add_test(tc_core, test_array_insert_with_growth);
    tcase_add_test(tc_core, test_array_insert_oom);
    tcase_add_test(tc_core, test_array_delete_from_beginning);
    tcase_add_test(tc_core, test_array_delete_from_middle);
    tcase_add_test(tc_core, test_array_delete_from_end);
    tcase_add_test(tc_core, test_array_set);
    tcase_add_test(tc_core, test_array_clear);

    // Security tests
    tcase_add_test(tc_core, test_array_stale_pointer_after_reallocation);
    tcase_add_test(tc_core, test_array_delete_all_elements_no_underflow);
    tcase_add_test(tc_core, test_array_interleaved_insert_delete_stress);
    tcase_add_test(tc_core, test_array_clear_then_append);
    tcase_add_test(tc_core, test_array_repeated_insert_delete_same_position);

#ifndef NDEBUG
    // Assertion tests - only in debug builds
    tcase_add_test_raise_signal(tc_core, test_array_get_null_array_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_array_get_out_of_bounds_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_array_append_null_array_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_array_append_null_element_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_array_insert_invalid_index_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_array_delete_invalid_index_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_array_set_invalid_index_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_array_size_null_array_asserts, SIGABRT);
#endif

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = array_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
