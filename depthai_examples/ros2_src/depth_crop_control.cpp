/**
 * This example shows usage of depth camera in crop mode with the possibility to move the crop.
 * Use 'WASD' in order to do it.
 */
#include "rclcpp/rclcpp.hpp"

#include <iostream>
#include <memory>

#include <depthai_ros_msgs/srv/NormalizedImageCrop.hpp>
#include <sensor_msgs/msg/Image.hpp>

#include <depthai_bridge/BridgePublisher.hpp>
#include <depthai_bridge/ImageConverter.hpp>
#include "depthai/depthai.hpp"


// Step size ('W','A','S','D' controls)
static constexpr float stepSize = 0.02;
std::shared_ptr<dai::DataInputQueue> configQueue;

void cropDepthImage(depthai_ros_msgs::srv::NormalizedImageCrop::Request request, depthai_ros_msgs::srv::NormalizedImageCrop::Response response){
    dai::ImageManipConfig cfg;
    cfg.setCropRect(request.topLeft.x, request.topLeft.y, request.bottomRight.x, request.bottomRight.y);
    configQueue->send(cfg);
    response->status = true;
    return;    
}

int main() {

    rclcpp::init(argc, argv);
    auto node = rclcpp::Node::make_shared("depth_crop_control");
    std::string cameraName;
    int confidence, LRchecktresh;
    bool lrcheck, extended, subpixel;

    node->declare_parameter("camera_name", "oak");
    node->declare_parameter("lrcheck", true);
    node->declare_parameter("extended", false);
    node->declare_parameter("subpixel", true);
    node->declare_parameter("confidence",  200);
    node->declare_parameter("LRchecktresh",  5);

    node->get_parameter("camera_name", cameraName);
    node->get_parameter("lrcheck",      lrcheck);
    node->get_parameter("extended",     extended);
    node->get_parameter("subpixel",     subpixel);
    node->get_parameter("confidence",   confidence);
    node->get_parameter("LRchecktresh", LRchecktresh);

    rclcpp::Service<depthai_ros_msgs::srv::NormalizedImageCrop>::SharedPtr service =
    node->create_service<depthai_ros_msgs::srv::NormalizedImageCrop>("crop_control_srv", &cropDepthImage);

    // ros::ServiceServer service = n.advertiseService("crop_control_srv", cropDepthImage);

    // Create pipeline
    dai::Pipeline pipeline;

    // Define sources and outputs
    auto monoRight = pipeline.create<dai::node::MonoCamera>();
    auto monoLeft = pipeline.create<dai::node::MonoCamera>();
    auto manip = pipeline.create<dai::node::ImageManip>();
    auto stereo = pipeline.create<dai::node::StereoDepth>();

    auto configIn = pipeline.create<dai::node::XLinkIn>();
    auto xout = pipeline.create<dai::node::XLinkOut>();

    configIn->setStreamName("config");
    xout->setStreamName("depth");

    // Crop range
    dai::Point2f topLeft(0.2, 0.2);
    dai::Point2f bottomRight(0.8, 0.8);

    // Properties
    monoRight->setBoardSocket(dai::CameraBoardSocket::RIGHT);
    monoLeft->setBoardSocket(dai::CameraBoardSocket::LEFT);
    monoRight->setResolution(dai::MonoCameraProperties::SensorResolution::THE_400_P);
    monoLeft->setResolution(dai::MonoCameraProperties::SensorResolution::THE_400_P);

    manip->initialConfig.setCropRect(topLeft.x, topLeft.y, bottomRight.x, bottomRight.y);
    manip->setMaxOutputFrameSize(monoRight->getResolutionHeight() * monoRight->getResolutionWidth() * 3);
    stereo->initialConfig.setConfidenceThreshold(confidence);
    stereo->initialConfig.setLeftRightCheckThreshold(LRchecktresh);
    stereo->setLeftRightCheck(lrcheck);
    stereo->setExtendedDisparity(extended);
    stereo->setSubpixel(subpixel);

    // Linking
    configIn->out.link(manip->inputConfig);
    stereo->depth.link(manip->inputImage);
    manip->out.link(xout->input);
    monoRight->out.link(stereo->left);
    monoLeft->out.link(stereo->right);

    // Connect to device and start pipeline
    dai::Device device(pipeline);

    // Queues
    auto depthQueue = device.getOutputQueue(xout->getStreamName(), 5, false);
    configQueue = device.getInputQueue(configIn->getStreamName());

    auto calibrationHandler = device.readCalibration();

    dai::rosBridge::ImageConverter depthConverter(cameraName + "_right_camera_optical_frame", true);
    // TODO(sachin): Modify the calibration based on crop from service
    auto rightCameraInfo = converter.calibrationToCameraInfo(calibrationHandler, dai::CameraBoardSocket::RIGHT, 1280, 720); 

    dai::rosBridge::BridgePublisher<sensor_msgs::msg::Image, dai::ImgFrame> depthPublish(depthQueue,
                                                                                    node, 
                                                                                    std::string("stereo/depth"),
                                                                                    std::bind(&dai::rosBridge::ImageConverter::toRosMsg, 
                                                                                    &depthConverter, // since the converter has the same frame name
                                                                                                    // and image type is also same we can reuse it
                                                                                    std::placeholders::_1, 
                                                                                    std::placeholders::_2) , 
                                                                                    30,
                                                                                    rightCameraInfo,
                                                                                    "stereo");
    depthPublish.addPubisherCallback();
    rclcpp::spin(node);

    return 0;
}
