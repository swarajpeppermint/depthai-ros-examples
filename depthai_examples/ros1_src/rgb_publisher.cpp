
#include "ros/ros.h"

#include <iostream>
#include <cstdio>
// #include "utility.hpp"
#include "sensor_msgs/Image.h"
#include <camera_info_manager/camera_info_manager.h>

// Inludes common necessary includes for development using depthai library
#include "depthai/depthai.hpp"
#include <depthai_bridge/BridgePublisher.hpp>
#include <depthai_bridge/ImageConverter.hpp>

dai::Pipeline createPipeline(){
    dai::Pipeline pipeline;
    auto colorCam = pipeline.create<dai::node::ColorCamera>();
    auto xlinkOut = pipeline.create<dai::node::XLinkOut>();
    xlinkOut->setStreamName("video");
    
    colorCam->setPreviewSize(300, 300);
    colorCam->setResolution(dai::ColorCameraProperties::SensorResolution::THE_1080_P);
    colorCam->setInterleaved(false);

    // Link plugins CAM -> XLINK
    colorCam->video.link(xlinkOut->input);
    return pipeline;
}

int main(int argc, char** argv){

    ros::init(argc, argv, "rgb_node");
    ros::NodeHandle pnh("~");
    
    std::string deviceName;
    std::string camera_param_uri;
    int badParams = 0;

    badParams += !pnh.getParam("camera_name", deviceName);
    badParams += !pnh.getParam("camera_param_uri", camera_param_uri);

    if (badParams > 0)
    {
        throw std::runtime_error("Couldn't find one of the parameters");
    }
    
    dai::Pipeline pipeline = createPipeline();
    dai::Device device(pipeline);
    std::shared_ptr<dai::DataOutputQueue> imgQueue = device.getOutputQueue("video", 30, false);
    
    std::string color_uri = camera_param_uri + "/" + "color.yaml";

    dai::rosBridge::ImageConverter rgbConverter(deviceName + "_rgb_camera_optical_frame", false);
    dai::rosBridge::BridgePublisher<sensor_msgs::Image, dai::ImgFrame> rgbPublish(imgQueue,
                                                                                  pnh, 
                                                                                  std::string("color/image"),
                                                                                  std::bind(&dai::rosBridge::ImageConverter::toRosMsg, 
                                                                                  &rgbConverter, // since the converter has the same frame name
                                                                                                  // and image type is also same we can reuse it
                                                                                  std::placeholders::_1, 
                                                                                  std::placeholders::_2) , 
                                                                                  30,
                                                                                  color_uri,
                                                                                  "color");

    rgbPublish.addPubisherCallback();
    ros::spin();

    return 0;
}

