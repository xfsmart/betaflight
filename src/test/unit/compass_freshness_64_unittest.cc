#include "compass_init_unittest.cc"

TEST_F(CompassInitTest, Simulator64ExactBoundaryFailureDoesNotRefreshAndNextSampleRecovers)
{
    ASSERT_EQ(8U, sizeof(timeUs_t));
    ASSERT_TRUE(compassInit());
    const timeUs_t publishedAt = (static_cast<timeUs_t>(1) << 48) + 12345U;
    publishMagSample(publishedAt, 11, 22, 33);

    testTimeUs = publishedAt + 499999U;
    ASSERT_TRUE(compassIsHealthy());

    magReadResults.push_back({false, {0, 0, 0}});
    testTimeUs = publishedAt + 500000U;
    EXPECT_EQ(1000U, compassUpdate(testTimeUs));
    EXPECT_FALSE(compassIsHealthy());

    publishMagSample(publishedAt + 700000U, 44, 55, 66);
    EXPECT_TRUE(compassIsHealthy());
}
