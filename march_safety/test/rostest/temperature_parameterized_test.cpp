// Copyright 2019 Project March.

#include <gtest/gtest.h>
#include <ros/ros.h>
#include <sensor_msgs/Temperature.h>
#include "error_counter.h"

/**
 * The input for the test cases we want to run.
 */
static const std::vector<std::tuple<float, size_t>> testCases = {
  // tuple(temperature, error_count)
  std::make_tuple(-10, 0), std::make_tuple(1, 0),  std::make_tuple(69, 0), std::make_tuple(70, 0),
  std::make_tuple(71, 1),  std::make_tuple(72, 1), std::make_tuple(120, 1)
};

class TestTemperatureParameterized : public ::testing::Test,
                                     public ::testing::WithParamInterface<std::tuple<float, size_t>>
{
protected:
  float temperature;
  size_t error_count;

  /**
   * Load all the parametrized variables for this specific test.
   */
  void SetUp() override
  {
    temperature = std::get<0>(GetParam());
    error_count = std::get<1>(GetParam());
  }
};

/**
 * This is testing values below, on and above the threshold.
 * Testing all kind of values is more robust, then just testing 1 value.
 * This should also prevent off by one errors.
 */
TEST_P(TestTemperatureParameterized, valuesAroundThreshold)
{
  ros::NodeHandle nh;
  ros::Publisher pub_joint1 = nh.advertise<sensor_msgs::Temperature>("march/temperature/test_joint1", 0);
  ErrorCounter errorCounter;
  ros::Subscriber sub = nh.subscribe("march/error", 0, &ErrorCounter::cb, &errorCounter);

  while (0 == pub_joint1.getNumSubscribers() || 0 == sub.getNumPublishers())
  {
    ros::Duration(0.1).sleep();
  }
  EXPECT_EQ(1u, pub_joint1.getNumSubscribers());
  EXPECT_EQ(1u, sub.getNumPublishers());
  EXPECT_EQ(0u, errorCounter.count);

  sensor_msgs::Temperature msg;
  msg.temperature = temperature;
  pub_joint1.publish(msg);

  // Wait to receive message
  int timeout_duration;
  nh.getParam("/march_safety_node/ros_timeout", timeout_duration);

  ros::Duration duration = ros::Duration(timeout_duration);
  ros::topic::waitForMessage<sensor_msgs::Temperature>("march/temperature/test_joint1", duration);
  ros::spinOnce();

  EXPECT_EQ(error_count, errorCounter.count);
}

INSTANTIATE_TEST_CASE_P(MyGroup, TestTemperatureParameterized, ::testing::ValuesIn(testCases));
