// Copyright 2018 Project March.

#include "main.h"
#include "ros/ros.h"

int main(int argc, char** argv)
{
  ros::init(argc, argv, "datalogger_node");
  ros::NodeHandle n;
  ros::spin();
  return 0;
}
