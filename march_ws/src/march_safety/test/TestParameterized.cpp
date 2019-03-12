// Copyright 2019 Project March.

#include <march_shared_resources/TopicNames.h>
#include "gtest/gtest.h"
#include "ros/ros.h"
#include "../src/TemperatureSafety.h"

/**
 * Counter
 */
struct ErrorCounter
{
  ErrorCounter() : count(0)
  {
  }

  void cb(const march_shared_resources::Error& msg)
  {
    ROS_INFO("CALLBACK");
    ++count;
  }

  uint32_t count;
};

/**
 * The input for the testcases we want to run.
 */
static const std::vector<std::tuple<float, float>> testCases = {
  // tuple(temperature, error_count)
  std::make_tuple(-10, 0), std::make_tuple(1, 0),  std::make_tuple(59, 0), std::make_tuple(60, 0),
  std::make_tuple(61, 1),  std::make_tuple(62, 1), std::make_tuple(120, 1)
};

class TestParameterized : public ::testing::Test, public ::testing::WithParamInterface<std::tuple<float, float>>
{
protected:
  float temperature;
  float error_count;

  /**
   * Load all the parametrized variables for this specific test.
   */
  void SetUp() override
  {
    temperature = std::get<0>(GetParam());
    error_count = std::get<1>(GetParam());
  }
};

TEST_P(TestParameterized, exceedSpecificThreshold)
{
  ros::NodeHandle nh;
  ros::Publisher pub_joint1 = nh.advertise<sensor_msgs::Temperature>("march/temperature/test_joint1", 0);
  ErrorCounter errorCounter;
  ros::Subscriber sub = nh.subscribe("march/error", 0, &ErrorCounter::cb, &errorCounter);
  sleep(1);  // wait short period for ros to create the publishers

  sensor_msgs::Temperature msg;
  msg.temperature = temperature;
  pub_joint1.publish(msg);

  // Wait to receive message
  sleep(1);
  ros::spinOnce();
  EXPECT_EQ(error_count, errorCounter.count);
}

/**
 * Name of the test, what fixture it uses and the input values.
 */
INSTANTIATE_TEST_CASE_P(MyGroup, TestParameterized, ::testing::ValuesIn(testCases));