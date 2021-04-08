#include "vortex_estimator/simple_estimator.h"
#include <cmath>
#include <eigen_conversions/eigen_msg.h>
#include <Eigen/Dense>

SimpleEstimator::SimpleEstimator()
{
  // Initiating topics
  m_imu_sub      = m_nh.subscribe("/sensors/imu/data", 1, &SimpleEstimator::imuCallback, this);
  m_pressure_sub = m_nh.subscribe("/sensors/pressure", 1, &SimpleEstimator::pressureCallback, this);
  m_state_pub    = m_nh.advertise<nav_msgs::Odometry>("estimator/state", 1);
  m_depth_pub    = m_nh.advertise<std_msgs::Float64>("depth/state", 1);

  // Parameters used for calculating depth
  if (!m_nh.getParam("atmosphere/pressure", m_atmospheric_pressure))
    ROS_ERROR("Could not read parameter: atmosphere/pressure");

  if (!m_nh.getParam("/water/density", m_water_density))
    ROS_ERROR("Could not read parameter: /water/density");

  if (!m_nh.getParam("/gravity/acceleration", m_gravitational_acceleration))
    ROS_ERROR("Could not read parameter: /gravity/acceleration.");

  // Initiating orientation in quaternions
  m_state.pose.pose.orientation.w = 1.0;
  m_state.pose.pose.orientation.x = 0.0;
  m_state.pose.pose.orientation.y = 0.0;
  m_state.pose.pose.orientation.z = 0.0;

  ROS_INFO("Estimator initialized.");
}

void SimpleEstimator::imuCallback(const sensor_msgs::Imu &msg)
{
  // The IMU publishes the orientation in quaternions and this function
  // ensures that they are represented as a NED body frame.

  // Converting the quaternion representation to Euler angles
  Eigen::Quaterniond quat_imu;
  tf::quaternionMsgToEigen(msg.orientation, quat_imu);
  Eigen::Vector3d euler_imu = quat_imu.toRotationMatrix().eulerAngles(2, 1, 0);

  // Convert Euler angles to "NED":
  // X positive forward/"north", Y positive to the right/"east", Z positive down.
  Eigen::Vector3d euler_ned(-euler_imu(0), euler_imu(2) + c_pi, euler_imu(1) + c_pi);

  // Transform back to quaternion
  Eigen::Matrix3d R_ned;
  R_ned = Eigen::AngleAxisd(euler_ned(0), Eigen::Vector3d::UnitZ())
        * Eigen::AngleAxisd(euler_ned(1), Eigen::Vector3d::UnitY())
        * Eigen::AngleAxisd(euler_ned(2), Eigen::Vector3d::UnitX());
  Eigen::Quaterniond quat_ned;

  // Publish the orientation in quaternions
  tf::quaternionEigenToMsg(quat_ned, m_state.pose.pose.orientation);
  m_state.twist.twist.angular.z = -msg.angular_velocity.z;
  m_state_pub.publish(m_state);
}

void SimpleEstimator::pressureCallback(const sensor_msgs::FluidPressure &msg)
{
  const float gauge_pressure = msg.fluid_pressure - m_atmospheric_pressure;
  const float depth_meters = gauge_pressure / (m_water_density * m_gravitational_acceleration);
  depth.data = depth_meters;
  m_depth_pub.publish(depth);
}
