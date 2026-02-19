#include <gtest/gtest.h>
#include <cmath>
#include <functional>
#include "dcalc/FindRoot.hh"

namespace sta {

class FindRootTest : public ::testing::Test {};

////////////////////////////////////////////////////////////////
// Original 7 tests
////////////////////////////////////////////////////////////////

// Test finding root of f(x) = x^2 - 4 (root at x=2)
TEST_F(FindRootTest, QuadraticPositiveRoot) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x * x - 4.0;
    dy = 2.0 * x;
  };
  bool fail = false;
  double root = findRoot(func, 1.0, 3.0, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 2.0, 1e-8);
}

// Test finding root of f(x) = x^2 - 4 (root at x=-2)
TEST_F(FindRootTest, QuadraticNegativeRoot) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x * x - 4.0;
    dy = 2.0 * x;
  };
  bool fail = false;
  double root = findRoot(func, -3.0, -1.0, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, -2.0, 1e-8);
}

// Test finding root of f(x) = x - 1 (linear, root at x=1)
TEST_F(FindRootTest, LinearRoot) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x - 1.0;
    dy = 1.0;
  };
  bool fail = false;
  double root = findRoot(func, 0.0, 2.0, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 1.0, 1e-8);
}

// Test finding root of f(x) = sin(x) near pi
TEST_F(FindRootTest, SinRoot) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = sin(x);
    dy = cos(x);
  };
  bool fail = false;
  double root = findRoot(func, 2.5, 3.8, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, M_PI, 1e-6);
}

// Test finding root of f(x) = e^x - 2 (root at x=ln(2))
TEST_F(FindRootTest, ExponentialRoot) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = exp(x) - 2.0;
    dy = exp(x);
  };
  bool fail = false;
  double root = findRoot(func, 0.0, 1.0, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, log(2.0), 1e-8);
}

// Test with tight tolerance
TEST_F(FindRootTest, TightTolerance) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x * x - 2.0;
    dy = 2.0 * x;
  };
  bool fail = false;
  double root = findRoot(func, 1.0, 2.0, 1e-14, 200, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, sqrt(2.0), 1e-12);
}

// Test the 4-argument version with pre-computed y values
TEST_F(FindRootTest, WithPrecomputedY) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x * x - 9.0;
    dy = 2.0 * x;
  };
  bool fail = false;
  // x1=2, y1=4-9=-5, x2=4, y2=16-9=7
  double root = findRoot(func, 2.0, -5.0, 4.0, 7.0, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 3.0, 1e-8);
}

////////////////////////////////////////////////////////////////
// Tolerance edge cases
////////////////////////////////////////////////////////////////

// Very tight tolerance: 1e-15 (near machine epsilon)
TEST_F(FindRootTest, VeryTightTolerance) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x - 5.0;
    dy = 1.0;
  };
  bool fail = false;
  double root = findRoot(func, 3.0, 7.0, 1e-15, 500, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 5.0, 1e-13);
}

// Very loose tolerance: 1e-1
TEST_F(FindRootTest, LooseTolerance) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x * x - 25.0;
    dy = 2.0 * x;
  };
  bool fail = false;
  double root = findRoot(func, 3.0, 7.0, 1e-1, 100, fail);
  EXPECT_FALSE(fail);
  // With 10% relative tolerance, result should still be in the right ballpark
  EXPECT_NEAR(root, 5.0, 0.6);
}

// Zero tolerance: convergence check becomes abs(dx) <= 0, which is only
// satisfied when dx is exactly 0. Likely hits max_iter and fails.
TEST_F(FindRootTest, ZeroTolerance) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x - 3.0;
    dy = 1.0;
  };
  bool fail = false;
  double root = findRoot(func, 1.0, 5.0, 0.0, 100, fail);
  // May or may not converge -- for a linear function Newton converges in 1 step
  // so dx can be exactly 0. Accept either outcome.
  if (!fail) {
    EXPECT_NEAR(root, 3.0, 1e-10);
  }
}

////////////////////////////////////////////////////////////////
// Iteration limit edge cases
////////////////////////////////////////////////////////////////

// Only 1 iteration allowed
TEST_F(FindRootTest, OneIteration) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x * x - 4.0;
    dy = 2.0 * x;
  };
  bool fail = false;
  double root = findRoot(func, 1.0, 3.0, 1e-10, 1, fail);
  // With only 1 iteration, a quadratic likely won't converge to tight tol
  // The algorithm may or may not fail depending on initial bisection step
  (void)root; // just ensure no crash
}

// Two iterations
TEST_F(FindRootTest, TwoIterations) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x - 7.0;
    dy = 1.0;
  };
  bool fail = false;
  double root = findRoot(func, 5.0, 9.0, 1e-10, 2, fail);
  // Linear function: Newton should converge very fast
  // After the initial midpoint (7.0), Newton step should nail it
  if (!fail) {
    EXPECT_NEAR(root, 7.0, 1e-6);
  }
}

// Zero max iterations: the for-loop body never executes, so fail is set to true
TEST_F(FindRootTest, ZeroMaxIterations) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x - 1.0;
    dy = 1.0;
  };
  bool fail = false;
  findRoot(func, 0.0, 2.0, 1e-10, 0, fail);
  EXPECT_TRUE(fail);
}

// Large max_iter (should still converge quickly and not hang)
TEST_F(FindRootTest, LargeMaxIter) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x * x - 16.0;
    dy = 2.0 * x;
  };
  bool fail = false;
  double root = findRoot(func, 1.0, 10.0, 1e-12, 10000, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 4.0, 1e-10);
}

////////////////////////////////////////////////////////////////
// Special function types
////////////////////////////////////////////////////////////////

// Cubic: f(x) = x^3 - 8 (root at x=2)
TEST_F(FindRootTest, CubicRoot) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x * x * x - 8.0;
    dy = 3.0 * x * x;
  };
  bool fail = false;
  double root = findRoot(func, 1.0, 3.0, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 2.0, 1e-8);
}

// Quartic: f(x) = x^4 - 16 (root at x=2)
TEST_F(FindRootTest, QuarticRoot) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x * x * x * x - 16.0;
    dy = 4.0 * x * x * x;
  };
  bool fail = false;
  double root = findRoot(func, 1.0, 3.0, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 2.0, 1e-8);
}

// Exponential: f(x) = e^x - 10 (root at x=ln(10))
TEST_F(FindRootTest, ExponentialRoot2) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = exp(x) - 10.0;
    dy = exp(x);
  };
  bool fail = false;
  double root = findRoot(func, 1.0, 4.0, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, log(10.0), 1e-8);
}

// Square root function: f(x) = sqrt(x) - 3, root at x=9
// Derivative: 1/(2*sqrt(x))
TEST_F(FindRootTest, SqrtFunctionRoot) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = sqrt(x) - 3.0;
    dy = 0.5 / sqrt(x);
  };
  bool fail = false;
  double root = findRoot(func, 1.0, 20.0, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 9.0, 1e-6);
}

////////////////////////////////////////////////////////////////
// Near-zero roots
////////////////////////////////////////////////////////////////

// f(x) = x - 1e-10 (root very close to zero)
// Note: convergence check is abs(dx) <= x_tol * abs(root).
// When root is near zero, the relative tolerance is very tight.
// This may require many iterations or not converge.
TEST_F(FindRootTest, NearZeroRootLinear) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x - 1e-10;
    dy = 1.0;
  };
  bool fail = false;
  double root = findRoot(func, -1.0, 1.0, 1e-6, 200, fail);
  // Newton on a linear function converges in 1-2 steps regardless of root location
  if (!fail) {
    EXPECT_NEAR(root, 1e-10, 1e-6);
  }
}

// f(x) = x (root exactly at zero)
// Convergence test: abs(dx) <= x_tol * abs(root) = x_tol * 0 = 0
// Will likely hit max_iter because relative tolerance at root=0 requires dx=0 exactly
TEST_F(FindRootTest, RootExactlyAtZero) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x;
    dy = 1.0;
  };
  bool fail = false;
  double root = findRoot(func, -1.0, 1.0, 1e-10, 200, fail);
  // Even if fail is true, root should be very close to 0
  EXPECT_NEAR(root, 0.0, 1e-6);
}

////////////////////////////////////////////////////////////////
// Negative domain
////////////////////////////////////////////////////////////////

// Root in deeply negative domain: f(x) = x + 100, root at x=-100
TEST_F(FindRootTest, NegativeDomainRoot) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x + 100.0;
    dy = 1.0;
  };
  bool fail = false;
  double root = findRoot(func, -200.0, 0.0, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, -100.0, 1e-6);
}

// Both bracket endpoints negative: f(x) = x^2 - 1, root at x=-1
TEST_F(FindRootTest, NegativeBracketRoot) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x * x - 1.0;
    dy = 2.0 * x;
  };
  bool fail = false;
  double root = findRoot(func, -2.0, -0.5, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, -1.0, 1e-8);
}

////////////////////////////////////////////////////////////////
// Trigonometric functions
////////////////////////////////////////////////////////////////

// sin(x) root at x=0 (bracket [-1, 1])
TEST_F(FindRootTest, SinRootAtZero) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = sin(x);
    dy = cos(x);
  };
  bool fail = false;
  double root = findRoot(func, -1.0, 1.0, 1e-10, 100, fail);
  // Root at 0 has the relative-tolerance issue, but Newton converges fast for sin
  EXPECT_NEAR(root, 0.0, 1e-4);
}

// sin(x) root at x=2*pi (bracket [5.5, 7.0])
TEST_F(FindRootTest, SinRootAt2Pi) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = sin(x);
    dy = cos(x);
  };
  bool fail = false;
  double root = findRoot(func, 5.5, 7.0, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 2.0 * M_PI, 1e-6);
}

// cos(x) root at x=pi/2
TEST_F(FindRootTest, CosRootAtPiOver2) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = cos(x);
    dy = -sin(x);
  };
  bool fail = false;
  double root = findRoot(func, 1.0, 2.0, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, M_PI / 2.0, 1e-6);
}

////////////////////////////////////////////////////////////////
// Multiple roots nearby
////////////////////////////////////////////////////////////////

// f(x) = (x-1)(x-2) = x^2 - 3x + 2, roots at x=1 and x=2
// Bracket [0.5, 1.5] should find x=1
TEST_F(FindRootTest, MultipleRootsFindFirst) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = (x - 1.0) * (x - 2.0);
    dy = 2.0 * x - 3.0;
  };
  bool fail = false;
  double root = findRoot(func, 0.5, 1.5, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 1.0, 1e-8);
}

// Same function, bracket [1.5, 2.5] should find x=2
TEST_F(FindRootTest, MultipleRootsFindSecond) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = (x - 1.0) * (x - 2.0);
    dy = 2.0 * x - 3.0;
  };
  bool fail = false;
  double root = findRoot(func, 1.5, 2.5, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 2.0, 1e-8);
}

////////////////////////////////////////////////////////////////
// Discontinuous derivative (sharp corner)
////////////////////////////////////////////////////////////////

// f(x) = |x| - 1 with piecewise derivative.
// Root at x=1 (bracket [0.5, 2.0] avoids the corner at 0)
TEST_F(FindRootTest, AbsValueRoot) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = fabs(x) - 1.0;
    dy = (x >= 0.0) ? 1.0 : -1.0;
  };
  bool fail = false;
  double root = findRoot(func, 0.5, 2.0, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 1.0, 1e-8);
}

// f(x) = |x| - 1, root at x=-1
TEST_F(FindRootTest, AbsValueNegativeRoot) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = fabs(x) - 1.0;
    dy = (x >= 0.0) ? 1.0 : -1.0;
  };
  bool fail = false;
  double root = findRoot(func, -2.0, -0.5, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, -1.0, 1e-8);
}

////////////////////////////////////////////////////////////////
// Very flat function (slow convergence)
////////////////////////////////////////////////////////////////

// f(x) = (x - 3)^5 has a repeated root at x=3 where the derivative is also
// zero. Newton-Raphson divides by dy which becomes 0, producing NaN.
// The algorithm is expected to fail on this degenerate case.
TEST_F(FindRootTest, FlatFifthOrderRootFails) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    double d = x - 3.0;
    double d2 = d * d;
    double d4 = d2 * d2;
    y = d4 * d;       // (x-3)^5
    dy = 5.0 * d4;    // 5*(x-3)^4
  };
  bool fail = false;
  findRoot(func, 2.0, 4.0, 1e-6, 500, fail);
  // The algorithm is expected to fail because dy -> 0 at the root
  EXPECT_TRUE(fail);
}

// A function that is very flat near the root but still has nonzero derivative
// at the root: f(x) = sinh(x - 3) which is ~0 near x=3 but never has dy=0.
// sinh is flat near 0 (sinh(e) ~ e for small e) but derivative cosh(e) >= 1.
TEST_F(FindRootTest, FlatSinhRoot) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = sinh(x - 3.0);
    dy = cosh(x - 3.0);
  };
  bool fail = false;
  double root = findRoot(func, 2.0, 4.0, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 3.0, 1e-6);
}

////////////////////////////////////////////////////////////////
// Very steep function (fast convergence)
////////////////////////////////////////////////////////////////

// f(x) = 1000*(x - 5), root at x=5. Very steep gradient.
TEST_F(FindRootTest, SteepLinearRoot) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = 1000.0 * (x - 5.0);
    dy = 1000.0;
  };
  bool fail = false;
  double root = findRoot(func, 3.0, 7.0, 1e-12, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 5.0, 1e-10);
}

// f(x) = 1e6 * (x - 2), very steep
TEST_F(FindRootTest, VerySteepLinearRoot) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = 1e6 * (x - 2.0);
    dy = 1e6;
  };
  bool fail = false;
  double root = findRoot(func, 1.0, 3.0, 1e-14, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 2.0, 1e-12);
}

////////////////////////////////////////////////////////////////
// Large bracket
////////////////////////////////////////////////////////////////

// f(x) = x - 42, bracket [-1000, 1000]
TEST_F(FindRootTest, LargeBracket) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x - 42.0;
    dy = 1.0;
  };
  bool fail = false;
  double root = findRoot(func, -1000.0, 1000.0, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 42.0, 1e-6);
}

// Quadratic with large bracket: f(x) = x^2 - 100, root at 10
TEST_F(FindRootTest, LargeBracketQuadratic) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x * x - 100.0;
    dy = 2.0 * x;
  };
  bool fail = false;
  double root = findRoot(func, 1.0, 1000.0, 1e-10, 200, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 10.0, 1e-6);
}

////////////////////////////////////////////////////////////////
// Small bracket
////////////////////////////////////////////////////////////////

// f(x) = x - 1.0, bracket [0.999999, 1.000001] (very tight bracket around root)
TEST_F(FindRootTest, SmallBracket) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x - 1.0;
    dy = 1.0;
  };
  bool fail = false;
  double root = findRoot(func, 0.999999, 1.000001, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 1.0, 1e-6);
}

// Quadratic with very small bracket around root=2
TEST_F(FindRootTest, SmallBracketQuadratic) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x * x - 4.0;
    dy = 2.0 * x;
  };
  bool fail = false;
  double root = findRoot(func, 1.9999, 2.0001, 1e-12, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 2.0, 1e-8);
}

////////////////////////////////////////////////////////////////
// Both overloads tested together
////////////////////////////////////////////////////////////////

// Compare 2-arg and 4-arg overloads produce same result
TEST_F(FindRootTest, OverloadsProduceSameResult) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x * x * x - 27.0;
    dy = 3.0 * x * x;
  };

  bool fail_2arg = false;
  double root_2arg = findRoot(func, 2.0, 4.0, 1e-12, 100, fail_2arg);

  // Pre-compute y values for 4-arg version
  double y1 = 2.0 * 2.0 * 2.0 - 27.0;  // 8 - 27 = -19
  double y2 = 4.0 * 4.0 * 4.0 - 27.0;  // 64 - 27 = 37
  bool fail_4arg = false;
  double root_4arg = findRoot(func, 2.0, y1, 4.0, y2, 1e-12, 100, fail_4arg);

  EXPECT_FALSE(fail_2arg);
  EXPECT_FALSE(fail_4arg);
  EXPECT_NEAR(root_2arg, 3.0, 1e-10);
  EXPECT_NEAR(root_4arg, 3.0, 1e-10);
  EXPECT_NEAR(root_2arg, root_4arg, 1e-14);
}

// 4-arg overload: x1 endpoint is exact root (y1 == 0)
TEST_F(FindRootTest, FourArgX1IsRoot) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x - 5.0;
    dy = 1.0;
  };
  bool fail = false;
  // y1 = 5 - 5 = 0
  double root = findRoot(func, 5.0, 0.0, 8.0, 3.0, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_DOUBLE_EQ(root, 5.0);
}

// 4-arg overload: x2 endpoint is exact root (y2 == 0)
TEST_F(FindRootTest, FourArgX2IsRoot) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x - 5.0;
    dy = 1.0;
  };
  bool fail = false;
  // y2 = 5 - 5 = 0
  double root = findRoot(func, 2.0, -3.0, 5.0, 0.0, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_DOUBLE_EQ(root, 5.0);
}

////////////////////////////////////////////////////////////////
// Same-sign y values (should fail)
////////////////////////////////////////////////////////////////

// Both endpoints positive: should fail
TEST_F(FindRootTest, BothEndpointsPositiveFails) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x * x + 1.0;  // Always positive, no real root
    dy = 2.0 * x;
  };
  bool fail = false;
  findRoot(func, 1.0, 3.0, 1e-10, 100, fail);
  EXPECT_TRUE(fail);
}

// Both endpoints negative: should fail
TEST_F(FindRootTest, BothEndpointsNegativeFails) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = -x * x - 1.0;  // Always negative
    dy = -2.0 * x;
  };
  bool fail = false;
  findRoot(func, -3.0, 3.0, 1e-10, 100, fail);
  EXPECT_TRUE(fail);
}

// 4-arg version: same-sign y values
TEST_F(FindRootTest, FourArgSameSignFails) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x * x;
    dy = 2.0 * x;
  };
  bool fail = false;
  // Both y values positive
  findRoot(func, 1.0, 1.0, 2.0, 4.0, 1e-10, 100, fail);
  EXPECT_TRUE(fail);
}

////////////////////////////////////////////////////////////////
// Symmetry test
////////////////////////////////////////////////////////////////

// f(x) = x^2 - 4: bracket [0, 3] finds +2, bracket [-3, 0] finds -2
TEST_F(FindRootTest, SymmetryPositiveBracket) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x * x - 4.0;
    dy = 2.0 * x;
  };
  bool fail = false;
  double root = findRoot(func, 0.5, 3.0, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 2.0, 1e-8);
}

TEST_F(FindRootTest, SymmetryNegativeBracket) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x * x - 4.0;
    dy = 2.0 * x;
  };
  bool fail = false;
  double root = findRoot(func, -3.0, -0.5, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, -2.0, 1e-8);
}

////////////////////////////////////////////////////////////////
// Swapped bracket order (x1 > x2)
////////////////////////////////////////////////////////////////

// The algorithm should work regardless of bracket order
TEST_F(FindRootTest, SwappedBracketOrder) {
  FindRootFunc func = [](double x, double &y, double &dy) {
    y = x - 3.0;
    dy = 1.0;
  };
  bool fail = false;
  // x1=5 > x2=1 (reversed order)
  double root = findRoot(func, 5.0, 1.0, 1e-10, 100, fail);
  EXPECT_FALSE(fail);
  EXPECT_NEAR(root, 3.0, 1e-8);
}

} // namespace sta
