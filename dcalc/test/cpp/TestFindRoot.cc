#include <gtest/gtest.h>
#include <cmath>
#include <functional>
#include "dcalc/FindRoot.hh"

namespace sta {

class FindRootTest : public ::testing::Test {};

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

} // namespace sta
