#include <gtest/gtest.h>

#include "core.h"

TEST(Rtfs2dCoreTest, ReadyDoesNotThrow) {
    EXPECT_NO_THROW(rtfs2d::Ready());
}
