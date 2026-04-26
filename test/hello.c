#include <stdio.h>

// Integer-heavy: exercises Integer + Branch categories
int add(int a, int b) { return a + b; }

// Mixed: FP + Memory + loop — good stress test for the pass
float dot_product(float *a, float *b, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; i++)
        sum += a[i] * b[i];
    return sum;
}

int main() {
    printf("2 + 3 = %d\n", add(2, 3));
    return 0;
}
