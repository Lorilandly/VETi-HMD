#pragma once

// VQF-style accelerometer-only inclination filter.
// Algorithm adapted from: https://github.com/dlaidig/vqf (MIT License, 2021 Daniel Laidig)
//
// Without a gyroscope, VQF's updateAcc() reduces to:
//   1. 2nd-order Butterworth LPF on the raw accelerometer vector (time constant tauAcc).
//   2. Inclination correction: compute the shortest-arc quaternion from [0,0,1]
//      to the filtered gravity direction and fold it into accQuat.
//
// The single tuning parameter is tauAcc (seconds). Larger values give more
// smoothing at the cost of more lag. VQF's default is 3.0 s.

#include <cmath>
#include <limits>

#include <openvr_driver.h>

namespace VETiDriver {

struct AccelOrientationFilter {
    static constexpr double kSqrt2 = 1.41421356237309504880168872420969808;
    static constexpr double kPi    = 3.14159265358979323846264338327950288;

    double tauAcc;  // low-pass time constant (seconds)
    double Ts;      // sample period (seconds)

    double accLpB[3]{};     // 2nd-order Butterworth b coefficients
    double accLpA[2]{};     // 2nd-order Butterworth a coefficients
    double accLpState[6]{}; // per-component IIR state; NaN-prefixed during warm-start
    double lastAccLp[3]{};  // last filtered accelerometer output
    double accQuat[4]{};    // accumulated inclination quaternion [w, x, y, z]

    explicit AccelOrientationFilter(double tauAcc_ = 3.0, double Ts_ = 0.01)
        : tauAcc(tauAcc_), Ts(Ts_)
    {
        reset();
    }

    void reset()
    {
        // Compute 2nd-order Butterworth coefficients (VQF filterCoeffs()).
        if (tauAcc >= Ts / 2.0) {
            double fc = kSqrt2 / (2.0 * kPi * tauAcc);
            double C  = std::tan(kPi * fc * Ts);
            double D  = C*C + kSqrt2*C + 1.0;
            double b0 = C*C / D;
            accLpB[0] = b0; accLpB[1] = 2.0*b0; accLpB[2] = b0;
            accLpA[0] = 2.0*(C*C - 1.0) / D;
            accLpA[1] = (1.0 - kSqrt2*C + C*C) / D;
        } else {
            // tau too small relative to Ts — use passthrough
            accLpB[0] = 1.0; accLpB[1] = 0.0; accLpB[2] = 0.0;
            accLpA[0] = 0.0; accLpA[1] = 0.0;
        }

        // NaN in [0] and [1] triggers warm-start averaging in filterVec().
        const double NaN = std::numeric_limits<double>::quiet_NaN();
        accLpState[0] = NaN; accLpState[1] = NaN;
        accLpState[2] = 0.0; accLpState[3] = 0.0;
        accLpState[4] = 0.0; accLpState[5] = 0.0;

        lastAccLp[0] = 0.0; lastAccLp[1] = 0.0; lastAccLp[2] = 1.0;
        accQuat[0] = 1.0; accQuat[1] = 0.0; accQuat[2] = 0.0; accQuat[3] = 0.0;
    }

    // acc: raw accelerometer reading in any consistent units (e.g. m/s² or g).
    // Normalization is applied internally after filtering, matching VQF's convention.
    void update(const double acc[3])
    {
        // Step 1: low-pass filter the accelerometer vector.
        filterVec(acc);

        double nx = lastAccLp[0], ny = lastAccLp[1], nz = lastAccLp[2];
        double n  = std::sqrt(nx*nx + ny*ny + nz*nz);
        if (n < 1e-9) return;
        nx /= n; ny /= n; nz /= n;

        // Step 2: rotate the filtered accel into the current accumulated frame.
        // accEarth = quatRotate(accQuat, [nx, ny, nz])
        const double* q = accQuat;
        double gx = (1.0 - 2.0*q[2]*q[2] - 2.0*q[3]*q[3])*nx
                  + 2.0*ny*(q[2]*q[1] - q[0]*q[3])
                  + 2.0*nz*(q[0]*q[2] + q[3]*q[1]);
        double gy = 2.0*nx*(q[0]*q[3] + q[2]*q[1])
                  + ny*(1.0 - 2.0*q[1]*q[1] - 2.0*q[3]*q[3])
                  + 2.0*nz*(q[2]*q[3] - q[1]*q[0]);
        double gz = 2.0*nx*(q[3]*q[1] - q[0]*q[2])
                  + 2.0*ny*(q[0]*q[1] + q[3]*q[2])
                  + nz*(1.0 - 2.0*q[1]*q[1] - 2.0*q[2]*q[2]);

        double gn = std::sqrt(gx*gx + gy*gy + gz*gz);
        if (gn < 1e-9) return;
        gx /= gn; gy /= gn; gz /= gn;

        // Step 3: inclination correction — shortest arc from [0,0,1] to [gx,gy,gz].
        double cw = std::sqrt((gz + 1.0) / 2.0);
        double cx, cy, cz;
        if (cw > 1e-6) {
            cx =  0.5 * gy / cw;
            cy = -0.5 * gx / cw;
            cz =  0.0;
        } else {
            // Near 180° singularity — arbitrary but stable flip.
            cw = 0.0; cx = 1.0; cy = 0.0; cz = 0.0;
        }

        // Step 4: accQuat = normalize(corrQuat * accQuat)
        // qy and qz are swapped relative to VQF's native output to align the
        // sensor's roll axis with OpenVR's Z (roll) axis; qz is negated to match
        // the observed rotation direction.
        double ow = cw*q[0] - cx*q[1] - cy*q[2] - cz*q[3];
        double ox = cw*q[1] + cx*q[0] + cy*q[3] - cz*q[2];
        double oy = cw*q[2] - cx*q[3] + cy*q[0] + cz*q[1];
        double oz = cw*q[3] + cx*q[2] - cy*q[1] + cz*q[0];

        double on = std::sqrt(ow*ow + ox*ox + oy*oy + oz*oz);
        if (on < 1e-9) return;
        accQuat[0] =  ow/on;
        accQuat[1] =  ox/on;
        accQuat[2] =  oz/on;  // qy ← oz
        accQuat[3] = -oy/on;  // qz ← -oy
    }

    vr::HmdQuaternion_t getQuat() const
    {
        vr::HmdQuaternion_t out;
        out.w = accQuat[0]; out.x = accQuat[1];
        out.y = accQuat[2]; out.z = accQuat[3];
        return out;
    }

private:
    // Direct Form II transposed single-sample 2nd-order IIR step.
    static double filterStep(double x, const double b[3], const double a[2], double state[2])
    {
        double y = b[0]*x + state[0];
        state[0] = b[1]*x - a[0]*y + state[1];
        state[1] = b[2]*x - a[1]*y;
        return y;
    }

    // Low-pass filter x[3] into lastAccLp[3].
    // Mirrors VQF filterVec(): warm-start via NaN sentinel, then standard IIR.
    // State layout during warm-start: [NaN, count, sum0, sum1, sum2, unused]
    // State layout after warm-start:  [s0_0, s1_0, s0_1, s1_1, s0_2, s1_2]
    void filterVec(const double x[3])
    {
        const double NaN = std::numeric_limits<double>::quiet_NaN();

        if (std::isnan(accLpState[0])) {
            // Warm-start: accumulate a running mean until tauAcc seconds of
            // data have been collected, then initialize the IIR at steady state.
            if (std::isnan(accLpState[1])) {
                accLpState[1] = 0.0;
                accLpState[2] = 0.0; accLpState[3] = 0.0; accLpState[4] = 0.0;
            }
            accLpState[1] += 1.0;
            accLpState[2] += x[0]; accLpState[3] += x[1]; accLpState[4] += x[2];
            for (int i = 0; i < 3; i++)
                lastAccLp[i] = accLpState[2 + i] / accLpState[1];

            if (accLpState[1] * Ts >= tauAcc) {
                // Transition: set IIR state to steady-state for current output.
                for (int i = 0; i < 3; i++) {
                    double y0 = lastAccLp[i];
                    accLpState[0 + 2*i] = y0 * (1.0 - accLpB[0]);
                    accLpState[1 + 2*i] = y0 * (accLpB[2] - accLpA[1]);
                }
            }
            return;
        }

        for (int i = 0; i < 3; i++)
            lastAccLp[i] = filterStep(x[i], accLpB, accLpA, accLpState + 2*i);
    }
};

} // namespace VETiDriver
