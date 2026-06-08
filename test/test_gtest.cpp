#include <gtest/gtest.h>

#include "grid.h"

TEST(GridIndex, Basic) {
    rtfs2d::GridParams g(4, 3, 2.0f, 1.5f);
    EXPECT_FLOAT_EQ(g.dx, 0.5f);
    EXPECT_FLOAT_EQ(g.dy, 0.5f);
    EXPECT_EQ(g.TotalCells(), 12);
    EXPECT_EQ(g.Index(0, 0), 0);
    EXPECT_EQ(g.Index(3, 2), 11);
    EXPECT_EQ(g.Index(2, 1), 6);
}

TEST(ScalarFieldAccess, ReadWrite) {
    rtfs2d::GridParams g(4, 3, 2.0f, 1.5f);
    rtfs2d::ScalarField s(g);
    s(2, 1) = 3.14f;
    EXPECT_FLOAT_EQ(s(2,1), 3.14f);
    EXPECT_FLOAT_EQ(s(0,0), 0.0f);
}

TEST(VectorFieldAccess, SetAndGet) {
    rtfs2d::GridParams r(4, 3, 2.0f, 1.5f);
    rtfs2d::VectorField vf(r);
    vf.Set(2, 1, 1.0f, -2.0f);
    EXPECT_FLOAT_EQ(vf.U(2, 1), 1.0f);
    EXPECT_FLOAT_EQ(vf.V(2, 1), -2.0f);
    EXPECT_FLOAT_EQ(vf.U(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(vf.V(0, 0), 0.0f);
}