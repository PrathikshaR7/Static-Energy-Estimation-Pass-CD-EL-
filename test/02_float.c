// Test 02: Floating-point heavy workload
// Expected: high FloatScalar%, higher energy per instruction than integer
// Exercises: FADD, FMUL, FDIV, scalar FP pipeline

float poly_eval(float x, float *coeffs, int degree) {
    float result = 0.0f;
    float xpow   = 1.0f;
    for (int i = 0; i <= degree; i++) {
        result += coeffs[i] * xpow;
        xpow   *= x;
    }
    return result;
}

double newton_sqrt(double n) {
    double x = n / 2.0;
    for (int i = 0; i < 20; i++)
        x = 0.5 * (x + n / x);
    return x;
}

float vector_norm(float *v, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; i++)
        sum += v[i] * v[i];
    return newton_sqrt(sum);
}
