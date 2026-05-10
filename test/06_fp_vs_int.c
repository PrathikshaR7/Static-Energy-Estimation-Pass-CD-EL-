// Test 06: FP vs Integer ratio validation
// Purpose: Isolate the FloatScalar/Integer energy ratio.
// Our model: FloatScalar=1.5, Integer=1.0 -> ratio 1.5x
// Literature (ARM Cortex-A55 opt guide): FP scalar ops cost ~1.4-1.6x
// integer ops in terms of issue latency and power draw.
// This test runs equivalent workloads in FP and integer to confirm
// the ratio our pass reports matches the published range.

// Integer-only version: sum of squares
long int_sum_sq(int *arr, int n) {
    long sum = 0;
    for (int i = 0; i < n; i++)
        sum += (long)arr[i] * arr[i];
    return sum;
}

// FP-equivalent version: same operation in float
float fp_sum_sq(float *arr, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; i++)
        sum += arr[i] * arr[i];
    return sum;
}
