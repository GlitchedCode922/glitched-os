#include "math.h"
#include "stdlib.h"

int atoi(const char *str) {
    int result = 0;
    int sign = 1;

    while (*str == ' ' || *str == '\t' || *str == '\n') str++;

    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    while (*str >= '0' && *str <= '9') {
        result *= 10;
        result += *str - '0';
        str++;
    }
    return sign * result;
}

long atol(const char *str) {
    long result = 0;
    int sign = 1;

    while (*str == ' ' || *str == '\t' || *str == '\n') str++;

    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    while (*str >= '0' && *str <= '9') {
        result *= 10;
        result += *str - '0';
        str++;
    }
    return sign * result;
}

long long atoll(const char *str) {
    long long result = 0;
    int sign = 1;

    while (*str == ' ' || *str == '\t' || *str == '\n') str++;

    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    while (*str >= '0' && *str <= '9') {
        result *= 10;
        result += *str - '0';
        str++;
    }
    return sign * result;
}

int abs(int x) {
    return x < 0 ? -x : x;
}

long labs(long x) {
    return x < 0 ? -x : x;
}

long long llabs(long long x) {
    return x < 0 ? -x : x;
}

double fabs(double x) {
    return x < 0 ? -x : x;
}

double sqrt(double x) {
    if (x < 0) return -1; // error, no complex support
    double guess = x / 2.0;
    for (int i = 0; i < 20; i++) {
        guess = (guess + x / guess) / 2.0;
    }
    return guess;
}

static double factorial(int n) {
    double result = 1.0;
    for (int i = 2; i <= n; i++) result *= i;
    return result;
}

static double exp_taylor(double x) {
    double sum = 1.0;
    double term = 1.0;
    for (int i = 1; i < 20; i++) {
        term *= x / i;
        sum += term;
    }
    return sum;
}

static double log_taylor(double x) {
    if (x <= 0) return -1; // error, no complex support
    double y = (x - 1) / (x + 1);
    double y2 = y * y;
    double sum = 0.0;
    for (int n = 0; n < 20; n++) {
        sum += (1.0 / (2*n + 1)) * (1 << 0) * y;  // will correct below
    }
    sum = 0.0;
    double term = y;
    for (int n = 0; n < 20; n++) {
        sum += term / (2*n + 1);
        term *= y2;
    }
    return 2 * sum;
}

double pow(double x, double y) {
    if (x == 0) return 0;
    if (y == 0) return 1;
    return exp_taylor(y * log_taylor(x));
}

double sin(double x) {
    double term = x;
    double sum = x;
    for (int i = 1; i < 10; i++) {
        term *= -1 * x * x / ((2*i)*(2*i+1));
        sum += term;
    }
    return sum;
}

double cos(double x) {
    double term = 1.0;
    double sum = 1.0;
    for (int i = 1; i < 10; i++) {
        term *= -1 * x * x / ((2*i-1)*(2*i));
        sum += term;
    }
    return sum;
}

double tan(double x) {
    return sin(x) / cos(x);
}

double floor(double x) {
    int xi = (int)x;
    return x < xi ? xi - 1 : xi;
}

double ceil(double x) {
    int xi = (int)x;
    return x > xi ? xi + 1 : xi;
}
