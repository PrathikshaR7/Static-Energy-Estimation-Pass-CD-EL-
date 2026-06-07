// Test 08: False Prediction (Static vs. Dynamic Discrepancy)
// Purpose: Demonstrate a scenario where static energy estimation
// gives a false/misleading prediction compared to actual runtime execution.
// Specifically, it exposes the limitations of using a fixed 10^depth loop-weight proxy
// and ignoring branch conditions/trip counts.

// Statically "Hot" but Dynamically "Cold"
// This function contains a nested loop (depth-2, weight 100x).
// Statically, the pass will estimate this as extremely high energy.
// Dynamically, the loop is never entered because n is 0 or it immediately exits.
int false_hot_loop(int *arr, int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            sum += arr[i * n + j];
        }
    }
    return sum;
}

// Statically "Cold" but Dynamically "Hot"
// This function has a flat loop (depth-1, weight 10x).
// Statically, the pass will estimate this as relatively low energy.
// Dynamically, it executes a loop with a massive trip count (e.g. 10^6 iterations).
int false_cold_loop(int *arr, int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += arr[i % 100];
    }
    return sum;
}
