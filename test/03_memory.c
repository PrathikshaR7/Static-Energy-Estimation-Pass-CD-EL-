// Test 03: Memory-bound workload
// Expected: high Memory%, highest energy — memory dominates on AArch64
// Exercises: loads and stores across large arrays (cache pressure)

void matrix_add(float *C, float *A, float *B, int n) {
    for (int i = 0; i < n; i++)
        C[i] = A[i] + B[i];
}

void matrix_transpose(float *dst, float *src, int rows, int cols) {
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            dst[j * rows + i] = src[i * cols + j];
}

int find_max(int *arr, int n) {
    int max = arr[0];
    for (int i = 1; i < n; i++)
        if (arr[i] > max)
            max = arr[i];
    return max;
}
