// Test 05: Mixed realistic workload
// Expected: balanced category distribution, moderate weighted-energy
// Combines integer control flow, FP computation, and memory access

typedef struct { float x, y, z; } Vec3;

static float vec3_dot(Vec3 a, Vec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

static Vec3 vec3_scale(Vec3 v, float s) {
    return (Vec3){ v.x*s, v.y*s, v.z*s };
}

// Simple ray-sphere intersection (energy-relevant graphics primitive)
int ray_sphere(Vec3 origin, Vec3 dir, Vec3 center, float radius,
               float *t_out) {
    Vec3  oc  = { origin.x - center.x,
                  origin.y - center.y,
                  origin.z - center.z };
    float a   = vec3_dot(dir, dir);
    float b   = 2.0f * vec3_dot(oc, dir);
    float c   = vec3_dot(oc, oc) - radius * radius;
    float disc = b*b - 4.0f*a*c;
    if (disc < 0.0f) return 0;
    *t_out = (-b - __builtin_sqrtf(disc)) / (2.0f * a);
    return 1;
}

// Reduce an array with a running average
float running_avg(float *data, int n) {
    float avg = 0.0f;
    for (int i = 0; i < n; i++)
        avg += (data[i] - avg) / (float)(i + 1);
    return avg;
}
