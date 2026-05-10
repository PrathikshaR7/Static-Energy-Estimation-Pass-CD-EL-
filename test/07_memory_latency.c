// Test 07: Memory access pattern validation
// Purpose: Confirm Memory category dominates and scales correctly.
// Our model: Memory=2.0 vs Integer=1.0 -> 2x cost per access.
// Literature (Cortex-A55): L1 load latency ~4 cycles vs ALU ~1 cycle,
// making memory ops 2-4x more expensive per instruction.
// Sequential vs strided access to demonstrate memory pressure scaling.

// Sequential access — L1 cache friendly
float seq_sum(float *arr, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; i++)
        sum += arr[i];
    return sum;
}

// Strided access — cache unfriendly, higher real energy cost
// (our static model treats both equally; this is a known limitation)
float strided_sum(float *arr, int n, int stride) {
    float sum = 0.0f;
    for (int i = 0; i < n; i += stride)
        sum += arr[i];
    return sum;
}

// Write-heavy — store operations
void fill_array(float *arr, int n, float val) {
    for (int i = 0; i < n; i++)
        arr[i] = val;
}
