/*
 *  Gazebo - Outdoor Multi-Robot Simulator
 *  Copyright (C) 2012 Open Source Robotics Foundation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
/*
 * Desc: Plugin to allow development shortcuts for VRC competition.
 * Author: John Hsu and Steven Peters
 * Date: December 2012
 */
#ifndef GAZEBO_VRC_PLUGIN_HH
#define GAZEBO_VRC_PLUGIN_HH

#include <ros/ros.h>
#include <ros/callback_queue.h>
#include <ros/advertise_options.h>
#include <ros/subscribe_options.h>
#include <geometry_msgs/Twist.h>
#include <geometry_msgs/Pose.h>
#include <std_msgs/String.h>
#include <sensor_msgs/JointState.h>

#include <control_msgs/FollowJointTrajectoryAction.h>
#include <control_msgs/JointTolerance.h>
#include <actionlib/client/simple_action_client.h>

#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

#include "gazebo/math/Vector3.hh"
#include "gazebo/physics/physics.hh"
#include "gazebo/transport/TransportTypes.hh"
#include "gazebo/common/Time.hh"
#include "gazebo/common/Plugin.hh"
#include "gazebo/common/Events.hh"

typedef actionlib::SimpleActionClient<
  control_msgs::FollowJointTrajectoryAction > TrajClient;
namespace gazebo
{
  class VRCPlugin : public WorldPlugin
  {
    /// \brief Constructor
    public: VRCPlugin();

    /// \brief Destructor
    public: virtual ~VRCPlugin();

    /// \brief Load the plugin.
    /// \param[in] _parent Pointer to parent world.
    /// \param[in] _sdf Pointer to sdf element.
    public: void Load(physics::WorldPtr _parent, sdf::ElementPtr _sdf);

    /// \brief Update the controller
    private: void UpdateStates();

    /// \brief Pointer to parent world.
    private: physics::WorldPtr world;

    /// \brief Mutex for VRC Plugin
    private: boost::mutex update_mutex;

    /// \brief Pointer to the update event connection
    private: event::ConnectionPtr updateConnection;

    ////////////////////////////////////////////////////////////////////////////
    //                                                                        //
    //   DRC Robot properties and states                                      //
    //                                                                        //
    ////////////////////////////////////////////////////////////////////////////
    private: class Robot
    {
      public: physics::ModelPtr model;
      public: physics::LinkPtr pinLink;
      public: physics::JointPtr pinJoint;

      /// \brief keep initial pose of robot to prevent z-drifting when
      /// teleporting the robot.
      public: math::Pose initialPose;

      /// \brief Pose of robot relative to vehicle.
      public: math::Pose vehicleRelPose;

      /// \brief Robot configuration when inside of vehicle.
      public: std::map<std::string, double> inVehicleConfiguration;

      /// \brief Flag to keep track of start-up 'harness' on the robot.
      public: bool startupHarness;

      /// \brief Load the drc_robot portion of plugin.
      /// \param[in] _parent Pointer to parent world.
      /// \param[in] _sdf Pointer to sdf element.
      public: void Load(physics::WorldPtr _parent, sdf::ElementPtr _sdf);

      /// \brief flag for successful initialization of fire hose, standpipe
      public: bool isInitialized;

      public: ros::Subscriber trajectory_sub_;
      public: ros::Subscriber pose_sub_;
      public: ros::Subscriber configuration_sub_;
      public: ros::Subscriber mode_sub_;

    } drc_robot;

    ////////////////////////////////////////////////////////////////////////////
    //                                                                        //
    //   DRC Vehicle properties and states                                    //
    //                                                                        //
    ////////////////////////////////////////////////////////////////////////////
    private: class Vehicle
    {
      public: physics::ModelPtr model;
      public: math::Pose initialPose;
      public: physics::LinkPtr seatLink;

      /// \brief flag for successful initialization of fire hose, standpipe
      public: bool isInitialized;

      /// \brief Load the drc_vehicle portion of plugin.
      /// \param[in] _parent Pointer to parent world.
      /// \param[in] _sdf Pointer to sdf element.
      public: void Load(physics::WorldPtr _parent, sdf::ElementPtr _sdf);
    } drc_vehicle;

    ////////////////////////////////////////////////////////////////////////////
    //                                                                        //
    //   DRC Fire Hose (and Standpipe)                                        //
    //                                                                        //
    ////////////////////////////////////////////////////////////////////////////
    private: class FireHose
    {
      public: physics::ModelPtr fireHoseModel;
      public: physics::ModelPtr standpipeModel;

      /// joint for pinning a link to the world
      public: physics::JointPtr fixedJoint;

      /// joints and links
      public: physics::Joint_V fireHoseJoints;
      public: physics::Link_V fireHoseLinks;
      /// screw joint
      public: physics::JointPtr screwJoint;
      public: double threadPitch;

      /// Pointer to the update event connection
      public: event::ConnectionPtr updateConnection;

      public: physics::LinkPtr couplingLink;
      public: physics::LinkPtr spoutLink;
      public: math::Pose couplingRelativePose;
      public: math::Pose initialFireHosePose;

      /// \brief flag for successful initialization of fire hose, standpipe
      public: bool isInitialized;

      /// \brief set initial configuration of the fire hose link
      public: void SetInitialConfiguration()
      {
        // for (unsigned int i = 0; i < this->fireHoseJoints.size(); ++i)
        //   gzerr << "joint [" << this->fireHoseJoints[i]->GetName() << "]\n";
        // for (unsigned int i = 0; i < this->links.size(); ++i)
        //   gzerr << "link [" << this->links[i]->GetName() << "]\n";
        this->fireHoseModel->SetWorldPose(this->initialFireHosePose);
        this->fireHoseJoints[fireHoseJoints.size()-4]->SetAngle(0, -M_PI/4.0);
        this->fireHoseJoints[fireHoseJoints.size()-2]->SetAngle(0, -M_PI/4.0);
      }

      /// \brief Load the drc_fire_hose portion of plugin.
      /// \param[in] _parent Pointer to parent world.
      /// \param[in] _sdf Pointer to sdf element.
      public: void Load(physics::WorldPtr _parent, sdf::ElementPtr _sdf);
    } drc_fire_hose;

    /// \brief check and spawn thread if links are aligned
    private: void CheckThreadStart();

    /// \brief fix robot butt to vehicle for efficiency
    // public: std::pair<physics::LinkPtr, physics::LinkPtr> vehicleRobot;
    public: physics::JointPtr vehicleRobotJoint;

    ////////////////////////////////////////////////////////////////////////////
    //                                                                        //
    //   Fire Hose   properties and states                                    //
    //                                                                        //
    ////////////////////////////////////////////////////////////////////////////
    private: physics::ModelPtr fire_hose;

    ////////////////////////////////////////////////////////////////////////////
    //                                                                        //
    //   Standpipe   properties and states                                    //
    //                                                                        //
    ////////////////////////////////////////////////////////////////////////////
    private: physics::ModelPtr standpipe;


    ////////////////////////////////////////////////////////////////////////////
    //                                                                        //
    //   Joint Trajectory Controller                                          //
    //                                                                        //
    ////////////////////////////////////////////////////////////////////////////


    private: class JointTrajectory
    {
    public:
      // Action client for the joint trajectory action 
      // used to trigger the arm movement action
      TrajClient* traj_client_;

    public:
      //! Initialize the action client and wait for action server to come up
      JointTrajectory() 
      {
        // tell the action client that we want to spin a thread by default
        //traj_client_ = new TrajClient("/drc_controller/joint_trajectory_action", true);
        traj_client_ = new TrajClient("/drc_controller/follow_joint_trajectory", true);

      }

      //! Clean up the action client
      ~JointTrajectory()
      {
        delete traj_client_;
      }

      //! Sends the command to start a given trajectory
      void startTrajectory(control_msgs::FollowJointTrajectoryGoal _goal)
      {
        // When to start the trajectory: 1s from now
        _goal.trajectory.header.stamp = ros::Time::now() + ros::Duration(1.0);

        traj_client_->sendGoal(_goal);
      }

      //! Generates a simple trajectory with two waypoints, used as an example
      /*! Note that this trajectory contains two waypoints, joined together
          as a single trajectory. Alternatively, each of these waypoints could
          be in its own trajectory - a trajectory can have one or more waypoints
          depending on the desired application.
      */
      control_msgs::FollowJointTrajectoryGoal seatingConfiguration()
      {
        //our goal variable
        control_msgs::FollowJointTrajectoryGoal goal;

        // First, the joint names, which apply to all waypoints
        goal.trajectory.joint_names.push_back("l_leg_uhz");
        goal.trajectory.joint_names.push_back("l_leg_mhx");
        goal.trajectory.joint_names.push_back("l_leg_lhy");
        goal.trajectory.joint_names.push_back("l_leg_kny");
        goal.trajectory.joint_names.push_back("l_leg_uay");
        goal.trajectory.joint_names.push_back("l_leg_lax");

        goal.trajectory.joint_names.push_back("r_leg_uhz");
        goal.trajectory.joint_names.push_back("r_leg_mhx");
        goal.trajectory.joint_names.push_back("r_leg_lhy");
        goal.trajectory.joint_names.push_back("r_leg_kny");
        goal.trajectory.joint_names.push_back("r_leg_uay");
        goal.trajectory.joint_names.push_back("r_leg_lax");

        goal.trajectory.joint_names.push_back("l_arm_usy");
        goal.trajectory.joint_names.push_back("l_arm_shx");
        goal.trajectory.joint_names.push_back("l_arm_ely");
        goal.trajectory.joint_names.push_back("l_arm_elx");
        goal.trajectory.joint_names.push_back("l_arm_uwy");
        goal.trajectory.joint_names.push_back("l_arm_mwx");

        goal.trajectory.joint_names.push_back("r_arm_usy");
        goal.trajectory.joint_names.push_back("r_arm_shx");
        goal.trajectory.joint_names.push_back("r_arm_ely");
        goal.trajectory.joint_names.push_back("r_arm_elx");
        goal.trajectory.joint_names.push_back("r_arm_uwy");
        goal.trajectory.joint_names.push_back("r_arm_mwx");

        goal.trajectory.joint_names.push_back("neck_ay"  );
        goal.trajectory.joint_names.push_back("back_lbz" );
        goal.trajectory.joint_names.push_back("back_mby" );
        goal.trajectory.joint_names.push_back("back_ubx" );

        // We will have two waypoints in this goal trajectory
        goal.trajectory.points.resize(1);

        // First trajectory point
        // Positions
        int ind = 0;
        goal.trajectory.points[ind].positions.resize(28);
        goal.trajectory.points[ind].positions[0]  =   0.00;
        goal.trajectory.points[ind].positions[1]  =   0.00;
        goal.trajectory.points[ind].positions[2]  =  -1.70;
        goal.trajectory.points[ind].positions[3]  =   1.80;
        goal.trajectory.points[ind].positions[4]  =  -0.10;
        goal.trajectory.points[ind].positions[5]  =   0.00;

        goal.trajectory.points[ind].positions[6]  =   0.00;
        goal.trajectory.points[ind].positions[7]  =   0.00;
        goal.trajectory.points[ind].positions[8]  =  -1.70;
        goal.trajectory.points[ind].positions[9]  =   1.80;
        goal.trajectory.points[ind].positions[10] =  -0.10;
        goal.trajectory.points[ind].positions[11] =   0.00;

        goal.trajectory.points[ind].positions[12] =  -1.60;
        goal.trajectory.points[ind].positions[13] =  -1.60;
        goal.trajectory.points[ind].positions[14] =   0.00;
        goal.trajectory.points[ind].positions[15] =   0.00;
        goal.trajectory.points[ind].positions[16] =   0.00;
        goal.trajectory.points[ind].positions[17] =   0.00;

        goal.trajectory.points[ind].positions[18] =   1.60;
        goal.trajectory.points[ind].positions[19] =   1.60;
        goal.trajectory.points[ind].positions[20] =   0.00;
        goal.trajectory.points[ind].positions[21] =   0.00;
        goal.trajectory.points[ind].positions[22] =   0.00;
        goal.trajectory.points[ind].positions[23] =   0.00;

        goal.trajectory.points[ind].positions[24] =   0.00;
        goal.trajectory.points[ind].positions[25] =   0.00;
        goal.trajectory.points[ind].positions[26] =   0.00;
        goal.trajectory.points[ind].positions[27] =   0.00;
        // Velocities
        goal.trajectory.points[ind].velocities.resize(28);
        for (size_t j = 0; j < 28; ++j)
        {
          goal.trajectory.points[ind].velocities[j] = 0.0;
        }
        // To be reached 1 second after starting along the trajectory
        goal.trajectory.points[ind].time_from_start = ros::Duration(1.0);

        // Velocities
        goal.trajectory.points[ind].velocities.resize(28);
        for (size_t j = 0; j < 28; ++j)
        {
          goal.trajectory.points[ind].velocities[j] = 0.0;
        }

        // tolerances
        /*
        for (unsigned j = 0; j < goal.trajectory.joint_names.size(); ++j)
        {
          control_msgs::JointTolerance jt;
          jt.name = goal.trajectory.joint_names[j];
          jt.position = 0.1;
          jt.velocity = 0.1;
          jt.acceleration = 0.1;
          goal.path_tolerance.push_back(jt);
        }
        */

        // tolerances
        for (unsigned j = 0; j < goal.trajectory.joint_names.size(); ++j)
        {
          control_msgs::JointTolerance jt;
          jt.name = goal.trajectory.joint_names[j];
          jt.position = 1000;
          jt.velocity = 1000;
          jt.acceleration = 1000;
          goal.goal_tolerance.push_back(jt);
        }

        goal.goal_time_tolerance.sec = 10;
        goal.goal_time_tolerance.nsec = 0;

        //we are done; return the goal
        return goal;
      }

      control_msgs::FollowJointTrajectoryGoal standingConfiguration()
      {
        //our goal variable
        control_msgs::FollowJointTrajectoryGoal goal;

        // First, the joint names, which apply to all waypoints
        goal.trajectory.joint_names.push_back("l_leg_uhz");
        goal.trajectory.joint_names.push_back("l_leg_mhx");
        goal.trajectory.joint_names.push_back("l_leg_lhy");
        goal.trajectory.joint_names.push_back("l_leg_kny");
        goal.trajectory.joint_names.push_back("l_leg_uay");
        goal.trajectory.joint_names.push_back("l_leg_lax");

        goal.trajectory.joint_names.push_back("r_leg_uhz");
        goal.trajectory.joint_names.push_back("r_leg_mhx");
        goal.trajectory.joint_names.push_back("r_leg_lhy");
        goal.trajectory.joint_names.push_back("r_leg_kny");
        goal.trajectory.joint_names.push_back("r_leg_uay");
        goal.trajectory.joint_names.push_back("r_leg_lax");

        goal.trajectory.joint_names.push_back("l_arm_usy");
        goal.trajectory.joint_names.push_back("l_arm_shx");
        goal.trajectory.joint_names.push_back("l_arm_ely");
        goal.trajectory.joint_names.push_back("l_arm_elx");
        goal.trajectory.joint_names.push_back("l_arm_uwy");
        goal.trajectory.joint_names.push_back("l_arm_mwx");

        goal.trajectory.joint_names.push_back("r_arm_usy");
        goal.trajectory.joint_names.push_back("r_arm_shx");
        goal.trajectory.joint_names.push_back("r_arm_ely");
        goal.trajectory.joint_names.push_back("r_arm_elx");
        goal.trajectory.joint_names.push_back("r_arm_uwy");
        goal.trajectory.joint_names.push_back("r_arm_mwx");

        goal.trajectory.joint_names.push_back("neck_ay"  );
        goal.trajectory.joint_names.push_back("back_lbz" );
        goal.trajectory.joint_names.push_back("back_mby" );
        goal.trajectory.joint_names.push_back("back_ubx" );

        // We will have two waypoints in this goal trajectory
        goal.trajectory.points.resize(1);

        // First trajectory point
        // Positions
        int ind = 0;
        goal.trajectory.points[ind].positions.resize(28);
        goal.trajectory.points[ind].positions[0]  =   0.00;
        goal.trajectory.points[ind].positions[1]  =   0.00;
        goal.trajectory.points[ind].positions[2]  =   0.00;
        goal.trajectory.points[ind].positions[3]  =   0.00;
        goal.trajectory.points[ind].positions[4]  =   0.00;
        goal.trajectory.points[ind].positions[5]  =   0.00;

        goal.trajectory.points[ind].positions[6]  =   0.00;
        goal.trajectory.points[ind].positions[7]  =   0.00;
        goal.trajectory.points[ind].positions[8]  =   0.00;
        goal.trajectory.points[ind].positions[9]  =   0.00;
        goal.trajectory.points[ind].positions[10] =   0.00;
        goal.trajectory.points[ind].positions[11] =   0.00;

        goal.trajectory.points[ind].positions[12] =   0.00;
        goal.trajectory.points[ind].positions[13] =  -1.60;
        goal.trajectory.points[ind].positions[14] =   0.00;
        goal.trajectory.points[ind].positions[15] =   0.00;
        goal.trajectory.points[ind].positions[16] =   0.00;
        goal.trajectory.points[ind].positions[17] =   0.00;

        goal.trajectory.points[ind].positions[18] =   0.00;
        goal.trajectory.points[ind].positions[19] =   1.60;
        goal.trajectory.points[ind].positions[20] =   0.00;
        goal.trajectory.points[ind].positions[21] =   0.00;
        goal.trajectory.points[ind].positions[22] =   0.00;
        goal.trajectory.points[ind].positions[23] =   0.00;

        goal.trajectory.points[ind].positions[24] =   0.00;
        goal.trajectory.points[ind].positions[25] =   0.00;
        goal.trajectory.points[ind].positions[26] =   0.00;
        goal.trajectory.points[ind].positions[27] =   0.00;
        // Velocities
        goal.trajectory.points[ind].velocities.resize(28);
        for (size_t j = 0; j < 28; ++j)
        {
          goal.trajectory.points[ind].velocities[j] = 0.0;
        }
        // To be reached 1 second after starting along the trajectory
        goal.trajectory.points[ind].time_from_start = ros::Duration(1.0);

        // Velocities
        goal.trajectory.points[ind].velocities.resize(28);
        for (size_t j = 0; j < 28; ++j)
        {
          goal.trajectory.points[ind].velocities[j] = 0.0;
        }

        // tolerances
        /*
        for (unsigned j = 0; j < goal.trajectory.joint_names.size(); ++j)
        {
          control_msgs::JointTolerance jt;
          jt.name = goal.trajectory.joint_names[j];
          jt.position = 0.1;
          jt.velocity = 0.1;
          jt.acceleration = 0.1;
          goal.path_tolerance.push_back(jt);
        }
        */

        // tolerances
        for (unsigned j = 0; j < goal.trajectory.joint_names.size(); ++j)
        {
          control_msgs::JointTolerance jt;
          jt.name = goal.trajectory.joint_names[j];
          jt.position = 1000;
          jt.velocity = 1000;
          jt.acceleration = 1000;
          goal.goal_tolerance.push_back(jt);
        }

        goal.goal_time_tolerance.sec = 10;
        goal.goal_time_tolerance.nsec = 0;

        //we are done; return the goal
        return goal;
      }

      //! Returns the current state of the action
      actionlib::SimpleClientGoalState getState()
      {
        return traj_client_->getState();
      }
     
    } joint_trajectory_controller;

    ////////////////////////////////////////////////////////////////////////////
    //                                                                        //
    //   List of available actions                                            //
    //                                                                        //
    ////////////////////////////////////////////////////////////////////////////
    /// Move the robot's pinned joint to a certain location in the world.
    private: void Teleport(const physics::LinkPtr &_pinLink,
                          physics::JointPtr &_pinJoint,
                          const math::Pose &_pose,
                      const std::map<std::string, double> &/*_jointPositions*/);

    /// \brief sets robot's absolute world pose
    private: void Teleport(const physics::LinkPtr &_pinLink,
                          physics::JointPtr &_pinJoint,
                          const math::Pose &_pose)
      {
        // default empty joint positions if not provided
        std::map<std::string, double> jointPositions;
        this->Teleport(_pinLink, _pinJoint, _pose, jointPositions);
      }

    /// \brief Sets DRC Robot planar navigational command velocity
    /// \param[in] _cmd A Vector3, where:
    ///   - x is the desired forward linear velocity, positive is robot-forward
    ///     and negative is robot-back.
    ///   - y is the desired lateral linear velocity, positive is robot-left
    ///     and negative is robot-right.
    ///   - z is the desired heading angular velocity, positive makes
    ///     the robot turn left, and negative makes the robot turn right
    public: void SetRobotCmdVel(const geometry_msgs::Twist::ConstPtr &_cmd);

    /// \brief sets robot's absolute world pose
    public: void SetRobotPose(const geometry_msgs::Pose::ConstPtr &_cmd);

    /// \brief sets robot's joint positions
    public: void SetRobotConfiguration(const sensor_msgs::JointState::ConstPtr
                                       &/*_cmd*/);

    /// \brief sets robot mode via ros topic
    public: void SetRobotModeTopic(const std_msgs::String::ConstPtr &_str);

    /// \brief sets robot mode
    public: void SetRobotMode(const std::string &_str);


    /// \brief Robot Vehicle Interaction, put robot in driver's seat.
    /// \param[in] _pose Relative pose offset, Pose()::Zero provides default
    ///                 behavior.
    public: void RobotEnterCar(const geometry_msgs::Pose::ConstPtr &_pose);

    /// \brief Robot Vehicle Interaction, put robot outside driver's side door.
    /// \param[in] _pose Relative pose offset, Pose()::Zero provides default
    ///                 behavior.
    public: void RobotExitCar(const geometry_msgs::Pose::ConstPtr &_pose);

    // \brief Cheats to teleport fire hose to hand and make a fixed joint
    public: void RobotGrabFireHose(const geometry_msgs::Pose::ConstPtr &_cmd);

    // \brief create a fixed joint between robot hand link and a nearby link
    public: void RobotGrabLink(const geometry_msgs::Pose::ConstPtr &/*_cmd*/);
    public: void RobotReleaseLink(const geometry_msgs::Pose::ConstPtr &/*_cmd*/);



    ////////////////////////////////////////////////////////////////////////////
    //                                                                        //
    //   Generic tools for manipulating models                                //
    //                                                                        //
    ////////////////////////////////////////////////////////////////////////////
    /// \brief add a constraint between 2 links
    private: physics::JointPtr AddJoint(physics::WorldPtr _world,
                                        physics::ModelPtr _model,
                                        physics::LinkPtr _link1,
                                        physics::LinkPtr _link2,
                                        std::string _type,
                                        math::Vector3 _anchor,
                                        math::Vector3 _axis,
                                        double _upper, double _lower);

    /// \brief Remove a joint.
    /// \param[in] _joint Joint to remove.
    private: void RemoveJoint(physics::JointPtr &_joint);

    /// \brief setup Robot ROS publication and sbuscriptions for the Robot
    /// These ros api describes Robot only actions
    private: void LoadRobotROSAPI();

    /// \brief setup ROS publication and sbuscriptions for VRC
    /// These ros api describes interactions between different models
    /// /drc_robot/cmd_vel - in pinned mode, the robot teleports based on
    ///                      messages from the cmd_vel
    private: void LoadVRCROSAPI();

    ////////////////////////////////////////////////////////////////////////////
    //                                                                        //
    //   Private variables                                                    //
    //                                                                        //
    ////////////////////////////////////////////////////////////////////////////
    // private: std::map<physics::LinkPtr, physics::JointPtr> pins;

    private: bool warpRobotWithCmdVel;

    private: double lastUpdateTime;
    private: geometry_msgs::Twist robotCmdVel;

    // ros stuff
    private: ros::NodeHandle* rosnode_;
    private: ros::CallbackQueue ros_queue_;
    private: void ROSQueueThread();
    private: boost::thread callback_queue_thread_;

    // ros subscription for grabbing objects
    public: ros::Subscriber robot_grab_sub_;
    public: ros::Subscriber robot_release_sub_;
    public: ros::Subscriber robot_enter_car_sub_;
    public: ros::Subscriber robot_exit_car_sub_;
    private: physics::JointPtr grabJoint;

    // deferred load in case ros is blocking
    private: sdf::ElementPtr sdf;
    private: void LoadThread();
    private: boost::thread deferred_load_thread_;
  };
/** \} */
/// @}
}
#endif
