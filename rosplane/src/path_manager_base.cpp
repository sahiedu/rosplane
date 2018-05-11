#include "path_manager_base.h"
#include "path_manager_example.h"

namespace rosplane
{

path_manager_base::path_manager_base():
  nh_(ros::NodeHandle()), /** nh_ stuff added here */
  nh_private_(ros::NodeHandle("~"))
{
  nh_private_.param<double>("R_min", params_.R_min, 75.0);
  nh_private_.param<double>("update_rate", update_rate_, 10.0);

  vehicle_state_sub_ = nh_.subscribe("state", 10, &path_manager_base::vehicle_state_callback, this);
  new_waypoint_service_ = nh_.advertiseService("/waypoint_path", &rosplane::path_manager_base::new_waypoint_callback, this);

  current_path_pub_ = nh_.advertise<rosplane_msgs::Current_Path>("current_path", 10);

  update_timer_ = nh_.createTimer(ros::Duration(1.0/update_rate_), &path_manager_base::current_path_publish, this);

  num_waypoints_ = 0;

  state_init_ = false;
}

void path_manager_base::vehicle_state_callback(const rosplane_msgs::StateConstPtr &msg)
{
  vehicle_state_ = *msg;

  state_init_ = true;
}

bool path_manager_base::new_waypoint_callback(rosplane_msgs::NewWaypoints::Request &req, rosplane_msgs::NewWaypoints::Response &res)
{
  int priority_level = 0;
  for (int i = 0; i < req.waypoints.size(); i++)
    if (req.waypoints[i].priority > priority_level)
      priority_level = req.waypoints[i].priority;
  for (int i = 0; i < waypoints_.size(); i++)
    if (waypoints_[i].priority < priority_level)
    {
      waypoints_.erase(waypoints_.begin() + i);
      i--;
    }
  for (int i = 0; i < req.waypoints.size(); i++)
  {
    if (req.waypoints[i].set_current || num_waypoints_ == 0)
    {
      waypoint_s currentwp;
      currentwp.w[0]         = vehicle_state_.position[0];
      currentwp.w[1]         = vehicle_state_.position[1];
      currentwp.w[2]         = (vehicle_state_.position[2] > -25 ? req.waypoints[i].w[2] : vehicle_state_.position[2]);
      currentwp.Va_d         = req.waypoints[i].Va_d;
      currentwp.landing      = false;
      currentwp.priority     = 5;
      currentwp.loiter_point = false;

      waypoints_.clear();
      waypoints_.push_back(currentwp);
      num_waypoints_ = 1;
      idx_a_ = 0;
    }
    waypoint_s nextwp;
    nextwp.w[0]         = req.waypoints[i].w[0];
    nextwp.w[1]         = req.waypoints[i].w[1];
    nextwp.w[2]         = req.waypoints[i].w[2];
    nextwp.Va_d         = req.waypoints[i].Va_d;
  	nextwp.landing			= req.waypoints[i].landing;
    nextwp.priority     = req.waypoints[i].priority;
    nextwp.loiter_point = req.waypoints[i].loiter_point;
    ROS_WARN("recieved waypoint: n: %f, e: %f, d: %f", nextwp.w[0], nextwp.w[1], nextwp.w[2]);
    ROS_WARN("                   Va_d: %f, priority %i", nextwp.Va_d, nextwp.priority);
    waypoints_.push_back(nextwp);
    num_waypoints_++;
    if (req.waypoints[i].clear_wp_list == true)
    {
      waypoints_.clear();
      num_waypoints_ = 0;
      idx_a_ = 0;
    }
  }
  return true;
}

void path_manager_base::current_path_publish(const ros::TimerEvent &)
{

  struct input_s input;
  input.pn = vehicle_state_.position[0];               /** position north */
  input.pe = vehicle_state_.position[1];               /** position east */
  input.h =  -vehicle_state_.position[2];                /** altitude */
  input.chi = vehicle_state_.chi;

  struct output_s output;

  if (state_init_ == true)
  {
    manage(params_, input, output);
  }

  rosplane_msgs::Current_Path current_path;

  if (output.flag)
    current_path.path_type = current_path.LINE_PATH;
  else
    current_path.path_type = current_path.ORBIT_PATH;
  current_path.Va_d = output.Va_d;
  for (int i = 0; i < 3; i++)
  {
    current_path.r[i] = output.r[i];
    current_path.q[i] = output.q[i];
    current_path.c[i] = output.c[i];

    if (std::isnan(current_path.r[i])) {ROS_FATAL("caught nan 1 path_manager %i", i);}
    if (std::isnan(current_path.q[i])) {ROS_FATAL("caught nan 2 path_manager %i", i);}
    if (std::isnan(current_path.c[i])) {ROS_FATAL("caught nan 3 path_manager %i", i);}
  }
  current_path.rho = output.rho;
  current_path.lambda = output.lambda;
	current_path.landing = output.landing;

  if (std::isnan(current_path.path_type)) {ROS_FATAL("caught nan 5 path_manager");}
  if (std::isnan(current_path.rho)) {ROS_FATAL("caught nan 6 path_manager");}
  if (std::isnan(current_path.lambda)) {ROS_FATAL("caught nan 7 path_manager");}

  current_path_pub_.publish(current_path);
}

} //end namespace

int main(int argc, char **argv)
{
  ros::init(argc, argv, "rosplane_path_manager");
  rosplane::path_manager_base *est = new rosplane::path_manager_example();

  ros::spin();

  return 0;
}
