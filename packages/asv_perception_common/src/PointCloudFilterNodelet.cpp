
#include "PointCloudFilterNodelet.h"

#include <pcl/pcl_base.h>
#include <pcl/point_types.h>
#include <pluginlib/class_list_macros.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/filters/conditional_removal.h>
#include <pcl/filters/radius_outlier_removal.h>

namespace {
  using namespace asv_perception_common;

  static const std::string 
    TOPIC_NAME_INPUT = "input"
    , TOPIC_NAME_OUTPUT = "output"
  ;
} // ns


//////////////////////////////////////////////////////////////////////////////////////////////
void PointCloudFilterNodelet::onInit ()
{
  // Call the super onInit ()
  base_type::onInit ();

  // parameters
  pnh_->getParam("min_distance_x", this->min_distance_x_ );
  pnh_->getParam("min_distance_y", this->min_distance_y_ );
  pnh_->getParam("min_distance_z", this->min_distance_z_ );

  pnh_->getParam( "outlier_min_neighbors", this->outlier_min_neighbors_ );
  pnh_->getParam( "outlier_radius", this->outlier_radius_ );

  // publisher
  this->pub_ = advertise<sensor_msgs::PointCloud2>( *pnh_, TOPIC_NAME_OUTPUT, 1 );

  onInitPostProcess ();
}

//////////////////////////////////////////////////////////////////////////////////////////////
void PointCloudFilterNodelet::subscribe ()
{
  this->sub_ = pnh_->subscribe<sensor_msgs::PointCloud2> (
    TOPIC_NAME_INPUT
    , 1
    , bind (&PointCloudFilterNodelet::sub_callback, this, _1 )
  );
}

//////////////////////////////////////////////////////////////////////////////////////////////
void PointCloudFilterNodelet::unsubscribe ()
{
  this->sub_.shutdown();
}

void PointCloudFilterNodelet::sub_callback (
      const sensor_msgs::PointCloud2::ConstPtr& cloud
)
{
  using point_type = pcl::PointXYZ;
  using pointcloud_type = pcl::PointCloud<point_type>;

  // No subscribers, no work
  if ( this->pub_.getNumSubscribers () < 1 )
    return;

  // empty cloud, no work
  if ( cloud->data.empty() )
    return;

  try {
    pointcloud_type::Ptr pc_ptr(new pointcloud_type());
    pcl::fromROSMsg( *cloud, *pc_ptr );

    if ( pc_ptr->empty() )
      return;

    // min distance filter
    if ( ( this->min_distance_x_ > 0.f ) || ( this->min_distance_y_ > 0.f ) || ( this->min_distance_z_ > 0.f ) ) {

        using PointType = point_type;
        pcl::ConditionOr<PointType>::Ptr range_cond (new pcl::ConditionOr<PointType> ());
        
        if ( this->min_distance_x_ > 0.f ) {
            range_cond->addComparison (pcl::FieldComparison<PointType>::ConstPtr (new
                                            pcl::FieldComparison<PointType> ("x", pcl::ComparisonOps::GT, this->min_distance_x_ )));
            range_cond->addComparison (pcl::FieldComparison<PointType>::ConstPtr (new
                                            pcl::FieldComparison<PointType> ("x", pcl::ComparisonOps::LT, -this->min_distance_x_ )));
        }

        if ( this->min_distance_y_ > 0.f ) {
            range_cond->addComparison (pcl::FieldComparison<PointType>::ConstPtr (new
                                            pcl::FieldComparison<PointType> ("y", pcl::ComparisonOps::GT, this->min_distance_y_ )));
            range_cond->addComparison (pcl::FieldComparison<PointType>::ConstPtr (new
                                            pcl::FieldComparison<PointType> ("y", pcl::ComparisonOps::LT, -this->min_distance_y_ )));
        }

        if ( this->min_distance_z_ > 0.f ) {
            range_cond->addComparison (pcl::FieldComparison<PointType>::ConstPtr (new
                                            pcl::FieldComparison<PointType> ("z", pcl::ComparisonOps::GT, this->min_distance_z_ )));
            range_cond->addComparison (pcl::FieldComparison<PointType>::ConstPtr (new
                                            pcl::FieldComparison<PointType> ("z", pcl::ComparisonOps::LT, -this->min_distance_z_ )));
        }

        pointcloud_type::Ptr cloud_post_filter (new pointcloud_type);
        pcl::ConditionalRemoval<PointType> condrem;
        condrem.setCondition(range_cond);
        condrem.setInputCloud(pc_ptr);
        condrem.filter(*cloud_post_filter);
        pc_ptr = cloud_post_filter;

        // radius outlier removal
        if ( !pc_ptr->empty() && ( this->outlier_min_neighbors_ > 0 ) && ( this->outlier_radius_ > 0. ) ) {
            pcl::RadiusOutlierRemoval<point_type> outrem = {};
            outrem.setInputCloud( pc_ptr );
            outrem.setRadiusSearch( this->outlier_radius_ );
            outrem.setMinNeighborsInRadius( this->outlier_min_neighbors_ );
            outrem.filter (*pc_ptr );
        }

    }
    
    // publish
    this->pub_.publish( pc_ptr );

  } catch ( const std::exception& ex ) {  // pcl exceptions inherit from std::runtime_error
    ROS_ERROR("std::exception: %s", ex.what() );
  } catch ( ... ) {
    ROS_ERROR("unknown exception type");
  }
}


PLUGINLIB_EXPORT_CLASS( asv_perception_common::PointCloudFilterNodelet, nodelet::Nodelet)