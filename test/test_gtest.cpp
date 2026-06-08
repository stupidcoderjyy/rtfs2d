#include <gtest/gtest.h>

#include "Grid.h"

TEST(GridIndex, Basic) {
    rtfs2d::GridParams g(4, 3, 2.0f, 1.5f);
    EXPECT_FLOAT_EQ(g.dx, 0.5f);
    EXPECT_FLOAT_EQ(g.dy, 0.5f);
    EXPECT_EQ(g.TotalCells(), 12);
    EXPECT_EQ(g.Index(0, 0), 0);
    EXPECT_EQ(g.Index(3, 2), 11);
    EXPECT_EQ(g.Index(2, 1), 6);
}
