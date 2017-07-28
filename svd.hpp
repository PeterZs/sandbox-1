#pragma once

#include "linalg_util.hpp"

#include <algorithm>
#include <assert.h>
#include <vector>
#include <iostream>

using namespace avl;

namespace svd
{
    template <typename T>
    inline T sqr(T a) { return (a == 0 ? 0 : a * a); }

    template <typename T>
    inline T sign(T a, T b) { return (b >= 0.0 ? std::abs(a) : -std::abs(a)); };

    // Computes (a^2 + b^2)^(1/2) without destructive underflow or overflow.
    template <typename T>
    inline T pythagora(T a, T b)
    {
        T abs_a = std::abs(a);
        T abs_b = std::abs(b);
        if (abs_a > abs_b) return abs_a*std::sqrt((T)1.0 + svd::sqr(abs_b / abs_a));
        else return (abs_b == (T)0.0 ? (T)0.0 : abs_b*std::sqrt((T)1.0 + svd::sqr(abs_a / abs_b)));
    };

    template<typename MatrixT, typename T>
    void sort(MatrixT & U, int m, int n, std::vector<T> & S, MatrixT & V)
    {
        int mu = m;
        int mv = m;

        for (int i = 0; i < n; i++)
        {
            int  k = i;
            T p = S[i];

            // Decending sort
            for (int j = i + 1; j < n; j++)
            {
                if (S[j] > p)
                {
                    k = j;
                    p = S[j];
                }
            }

            if (k != i)
            {
                S[k] = S[i];
                S[i] = p;

                int j = mu;
                for (int s = 0; j != 0; ++s, --j) std::swap(U[i][s], U[k][s]);
                j = mv;
                for (int s = 0; j != 0; ++s, --j) std::swap(V[i][s], V[k][s]);
            }
        }
    }
}

template <typename MatrixT, typename T>
inline bool singular_value_decomposition(MatrixT & A, int m, int n, std::vector<T> & S, MatrixT & V, const int max_iters = 32, bool sort = true)
{
    int flag, i, its, j, jj, k, l, nm;
    T anorm, c, f, g, h, s, scale, x, y, z;
    bool convergence = true;
    g = scale = anorm = 0;

    std::vector<T> rv1(n);

    // Householder reduction to bidiagonal form
    for (i = 0; i < n; i++)
    {
        l = i + 1;
        rv1[i] = scale*g;
        g = s = scale = 0.0;

        if (i < m)
        {
            for (k = i; k<m; k++)
                scale += std::abs(A[i][k]);

            if (scale)
            {
                for (k = i; k<m; k++)
                {
                    A[i][k] /= scale;
                    s += A[i][k] * A[i][k];
                }
                f = A[i][i];
                g = -svd::sign<T>(std::sqrt(s), f);
                h = f*g - s;
                A[i][i] = f - g;
                for (j = l; j<n; j++)
                {
                    for (s = 0.0, k = i; k<m; k++)
                        s += A[i][k] * A[j][k];
                    f = s / h;
                    for (k = i; k<m; k++)
                        A[j][k] += f*A[i][k];
                }
                for (k = i; k<m; k++)
                    A[i][k] *= scale;
            }
        }
        S[i] = scale *g;
        g = s = scale = 0.0;
        if (i < m && i != (n - 1))
        {
            for (k = l; k<n; k++)
                scale += std::abs(A[k][i]);
            if (scale)
            {
                for (k = l; k<n; k++)
                {
                    A[k][i] /= scale;
                    s += A[k][i] * A[k][i];
                }

                f = A[l][i];
                g = -svd::sign<T>(std::sqrt(s), f);
                h = f*g - s;
                A[l][i] = f - g;

                for (k = l; k < n; k++)
                    rv1[k] = A[k][i] / h;

                for (j = l; j < m; j++)
                {
                    for (s = 0.0, k = l; k<n; k++)
                        s += A[k][j] * A[k][i];
                    for (k = l; k<n; k++)
                        A[k][j] += s*rv1[k];
                }

                for (k = l; k < n; k++)
                    A[k][i] *= scale;
            }
        }
        anorm = std::max(anorm, (std::abs(S[i]) + std::abs(rv1[i])));
    }

    // Accumulation of right-hand transformations
    for (i = (n - 1); i >= 0; i--)
    {
        // Accumulation of right-hand transformations
        if (i < (n - 1))
        {
            if (g)
            {
                // Double division to avoid possible underflow
                for (j = l; j<n; j++)
                    V[i][j] = (A[j][i] / A[l][i]) / g;

                for (j = l; j<n; j++)
                {
                    for (s = 0.0, k = l; k<n; k++)
                        s += A[k][i] * V[j][k];

                    for (k = l; k<n; k++)
                        V[j][k] += s*V[i][k];
                }
            }

            for (j = l; j<n; j++)
                V[j][i] = V[i][j] = 0.0;
        }

        V[i][i] = 1.0;

        g = rv1[i];
        l = i;
    }

    // Accumulation of left-hand transformations
    for (i = std::min(m, n) - 1; i >= 0; i--)
    {
        l = i + 1;
        g = S[i];

        for (j = l; j<n; j++)
            A[j][i] = 0.0;

        if (g)
        {
            g = (T)1.0 / g;

            for (j = l; j<n; j++)
            {
                for (s = 0.0, k = l; k<m; k++)
                    s += A[i][k] * A[j][k];

                f = (s / A[i][i])*g;

                for (k = i; k<m; k++)
                    A[j][k] += f*A[i][k];
            }
            for (j = i; j<m; j++)
                A[i][j] *= g;
        }
        else
        {
            for (j = i; j < m; j++)
                A[i][j] = 0.0;
        }
        ++A[i][i];
    }

    // Diagonalization of the bidiagonal form: Loop over singular values, and over allowed iterations
    for (k = (n - 1); k >= 0; k--)
    {
        for (its = 1; its <= max_iters; its++)
        {
            flag = 1;
            for (l = k; l >= 0; l--)
            {
                // Test for splitting
                nm = l - 1;

                // Note that rv1[1] is always zero
                if ((T)(std::abs(rv1[l]) + anorm) == anorm)
                {
                    flag = 0;
                    break;
                }
                if ((T)(std::abs(S[nm]) + anorm) == anorm)
                    break;
            }
            if (flag)
            {
                // Cancellation of rv1[l], if l > 1
                c = 0.0; 
                s = 1.0;
                for (i = l; i <= k; i++)
                {
                    f = s*rv1[i];
                    rv1[i] = c*rv1[i];
                    if ((T)(std::abs(f) + anorm) == anorm) break;

                    g = S[i];
                    h = svd::pythagora<T>(f, g);
                    S[i] = h;
                    h = (T)1.0 / h;
                    c = g*h;
                    s = -f*h;

                    for (j = 0; j<m; j++)
                    {
                        y = A[nm][j];
                        z = A[i][j];
                        A[nm][j] = y*c + z*s;
                        A[i][j] = z*c - y*s;
                    }
                }
            }
            z = S[k];

            // Convergence
            if (l == k) 
            {
                // Singular value is made nonnegative
                if (z < 0.0) 
                {
                    S[k] = -z;
                    for (j = 0; j < n; j++)
                        V[k][j] = -V[k][j];
                }
                break;
            }

            if (its == max_iters) convergence = false;

            // Shift from bottom 2-by-2 minor
            x = S[l];
            nm = k - 1;
            y = S[nm];
            g = rv1[nm];
            h = rv1[k];
            f = ((y - z)*(y + z) + (g - h)*(g + h)) / ((T)2.0*h*y);
            g = svd::pythagora<T>(f, 1.0);
            f = ((x - z)*(x + z) + h*((y / (f + sign(g, f))) - h)) / x;
            c = s = 1.0;

            // QR transformation
            for (j = l; j <= nm; j++)
            {
                i = j + 1;
                g = rv1[i];
                y = S[i];
                h = s*g;
                g = c*g;
                z = svd::pythagora<T>(f, h);
                rv1[j] = z;
                c = f / z;
                s = h / z;
                f = x*c + g*s;
                g = g*c - x*s;
                h = y*s;
                y *= c;

                for (jj = 0; jj<n; jj++)
                {
                    x = V[j][jj];
                    z = V[i][jj];
                    V[j][jj] = x*c + z*s;
                    V[i][jj] = z*c - x*s;
                }

                z = svd::pythagora<T>(f, h);
                S[j] = z;

                // Rotation can be arbitrary if z = 0
                if (z)
                {
                    z = (T)1.0 / z;
                    c = f*z;
                    s = h*z;
                }

                f = c*g + s*y;
                x = c*y - s*g;

                for (jj = 0; jj<m; jj++)
                {
                    y = A[j][jj];
                    z = A[i][jj];
                    A[j][jj] = y*c + z*s;
                    A[i][jj] = z*c - y*s;
                }
            }

            rv1[l] = 0.0;
            rv1[k] = f;
            S[k] = x;
        }
    }

    if (sort) svd::sort(A, m, n, S, V);
    return convergence;
};

namespace svd_test
{
    inline void check_orthonormal(const float3x3 & matrix)
    {
        const float EPSILON = 100.f * std::numeric_limits<float>::epsilon();
        const auto prod = mul(matrix, transpose(matrix));

        for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j)
        {
            if (i == j) assert(std::abs(prod[i][j] - 1) < EPSILON);
            else assert(std::abs(prod[i][j]) < EPSILON);
        }
    }

    inline void run()
    {
        float3x3 A;
        float3x3 U, V;
        std::vector<float> S(3);

        A = { { -0.46673855799602715f, 0.67466260360310948f, 0.97646986796448998f },
              { -0.032460753747103721f, 0.046584527749418278f, 0.067431228641151142f }, 
              { -0.088885055229687815f, 0.1280389179308779f, 0.18532617511453064f } };

        float3x3 A_Copy = A;

        float maxEntry = 0;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                maxEntry = std::max(maxEntry, std::abs(A[j][i]));

        const float eps = std::numeric_limits<float>::epsilon();
        const float valueEps = maxEntry * 10.f * eps;

        auto result = singular_value_decomposition<float3x3, float>(A, 3, 3, S, V);
        U = A;

        float3x3 S_times_Vt;
        for (int j = 0; j < 3; ++j)
        {
            S_times_Vt[j].x = S[j] * V[j].x;
            S_times_Vt[j].y = S[j] * V[j].y;
            S_times_Vt[j].z = S[j] * V[j].z;
        }
        S_times_Vt = transpose(S_times_Vt);

        // Verify that the product of the matrices is A (Input)
        const float3x3 P = mul(U, S_times_Vt);

        for (int i = 0; i < 3; ++i)
        {
            assert(std::abs(P[i].x - A_Copy[i].x) <= valueEps);
            assert(std::abs(P[i].y - A_Copy[i].y) <= valueEps);
            assert(std::abs(P[i].z - A_Copy[i].z) <= valueEps);
        }

        // Check that U and V are orthogonal
        assert(determinant(U) < -0.9);
        assert(determinant(V) < -0.9);

        check_orthonormal(U);
        check_orthonormal(V);
    }
}
