#include "vortex_estimator/simple_estimator.h"
#include <cmath>
#include <eigen_conversions/eigen_msg.h>
#include <Eigen/Dense>

SimpleEstimator::SimpleEstimator()
{
  m_imu_sub      = m_nh.subscribe("/sensors/imu/data", 1, &SimpleEstimator::imuCallback, this);
  m_pressure_sub = m_nh.subscribe("/sensors/pressure", 1, &SimpleEstimator::pressureCallback, this);
  m_position_sub = m_nh.subscribe("/sensors/uwgps/data", 1, &SimpleEstimator::positionCallback, this);  //Added for UW GPS
  m_state_pub    = m_nh.advertise<nav_msgs::Odometry>("estimator/state", 1);

  if (!m_nh.getParam("atmosphere/pressure", m_atmospheric_pressure))
    ROS_ERROR("Could not read parameter: atmosphere/pressure");

  if (!m_nh.getParam("/water/density", m_water_density))
    ROS_ERROR("Could not read parameter: /water/density");

  if (!m_nh.getParam("/gravity/acceleration", m_gravitational_acceleration))
    ROS_ERROR("Could not read parameter: /gravity/acceleration.");

  m_state.pose.pose.orientation.w = 1.0;
  m_state.pose.pose.orientation.x = 0.0;
  m_state.pose.pose.orientation.y = 0.0;
  m_state.pose.pose.orientation.z = 0.0;
  m_state.pose.pose.position.x    = 0.0;  //Added for UW GPS
  m_state.pose.pose.position.y    = 0.0;  //Added for UW GPS
  m_state.pose.pose.position.z    = 0.0;  //Added for UW GPS

  ROS_INFO("Estimator initialized.");
}

void SimpleEstimator::imuCallback(const sensor_msgs::Imu &msg)
{
  // Rotation measured by IMU
  Eigen::Quaterniond quat_imu;
  tf::quaternionMsgToEigen(msg.orientation, quat_imu);
  Eigen::Vector3d euler_imu = quat_imu.toRotationMatrix().eulerAngles(2, 1, 0);

  // Alter IMU measurements to Z down, Y right, X forward
  Eigen::Vector3d euler_ned(-euler_imu(0), euler_imu(2) + c_pi, euler_imu(1) + c_pi);

  // Transform back to quaternion
  Eigen::Matrix3d R_ned;
  R_ned = Eigen::AngleAxisd(euler_ned(0), Eigen::Vector3d::UnitZ())
        * Eigen::AngleAxisd(euler_ned(1), Eigen::Vector3d::UnitY())
        * Eigen::AngleAxisd(euler_ned(2), Eigen::Vector3d::UnitX());
  Eigen::Quaterniond quat_ned;

  // Convert to quaternion message and publish
  tf::quaternionEigenToMsg(quat_ned, m_state.pose.pose.orientation);    // standardized
  m_state.twist.twist.angular.z = -msg.angular_velocity.z;              // standardized
  m_state_pub.publish(m_state);
}

void SimpleEstimator::pressureCallback(const sensor_msgs::FluidPressure &msg)
{
  const float gauge_pressure = msg.fluid_pressure - m_atmospheric_pressure;
  const float depth_meters = gauge_pressure / (m_water_density * m_gravitational_acceleration);
  m_state.pose.pose.position.z = depth_meters;                                        // standardized
  m_state_pub.publish(m_state);
}
