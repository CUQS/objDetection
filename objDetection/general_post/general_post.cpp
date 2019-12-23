/**
 * ============================================================================
 *
 * Copyright (C) 2018, Hisilicon Technologies Co., Ltd. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1 Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *   2 Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *
 *   3 Neither the names of the copyright holders nor the names of the
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * ============================================================================
 */

#include "general_post.h"

#include <unistd.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

#include "hiaiengine/log.h"
#include "opencv2/opencv.hpp"
#include "tool_api.h"

using hiai::Engine;
using namespace std;

// namespace
namespace {
  // callback port (engine port begin with 0)
  const uint32_t kSendDataPort = 0;

  // sleep interval when queue full (unit:microseconds)
  const __useconds_t kSleepInterval = 200000;

  // size of output tensor vector should be 2.
  const uint32_t kOutputTensorSize = 2;
  const uint32_t kOutputNumIndex = 0;
  const uint32_t kOutputTesnorIndex = 1;

  const uint32_t kCategoryIndex = 2;
  const uint32_t kScorePrecision = 3;

  // bounding box line solid
  const uint32_t kLineSolid = 2;

  // output image prefix
  const string kOutputFilePrefix = "out_";

  // boundingbox tensor shape
  const static std::vector<uint32_t> kDimDetectionOut = {64, 304, 8};

  // num tensor shape
  const static std::vector<uint32_t> kDimBBoxCnt = {32};

  // opencv draw label params.
  const double kFountScale = 0.5;
  const cv::Scalar kFontColor(0, 0, 255);
  const uint32_t kLabelOffset = 11;
  const string kFileSperator = "/";

  // opencv color list for boundingbox
  const vector<cv::Scalar> kColors {
    cv::Scalar(237, 149, 100), cv::Scalar(0, 215, 255), cv::Scalar(50, 205, 50),
    cv::Scalar(139, 85, 26)};
    // output tensor index
    enum BBoxIndex {kTopLeftX, kTopLeftY, kLowerRigltX, kLowerRightY, kScore};

}

// register custom data type
HIAI_REGISTER_DATA_TYPE("EngineTrans", EngineTrans);

HIAI_StatusT GeneralPost::Init(
  const hiai::AIConfig &config,
  const vector<hiai::AIModelDescription> &model_desc) {
  addrLen = sizeof(struct sockaddr_in);
  serverAddr.sin_family = PF_INET;

  for (int index = 0; index < config.items_size(); ++index) {
    const ::hiai::AIConfigItem& item = config.items(index);
    string name = item.name();
    string value = item.value();

    if (name == "serverIP") {
      serverAddr.sin_addr.s_addr = inet_addr(value.data());
      cout << "--post-- serverIP: " << value.data() << endl;
    } else if (name == "serverPort") {
      int serverPort = atoi(value.data());
      serverAddr.sin_port = htons(serverPort);
      cout << "--post-- serverPort: " << serverPort << endl;
    } else {
      HIAI_ENGINE_LOG("unused config name: %s", name.c_str());
    }
  }
  if ((sokt = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    cout << "--post-- socket() failed" << endl;
    return HIAI_ERROR;
  }
  if (connect(sokt, (sockaddr*)&serverAddr, addrLen) < 0) {
    cout << "--post-- connect() failed!" << endl;
    cout << "--post-- close socket" << endl;
    close(sokt);
  }
  return HIAI_OK;
}

bool GeneralPost::SendSentinel() {
  // can not discard when queue full
  HIAI_StatusT hiai_ret = HIAI_OK;
  shared_ptr<string> sentinel_msg(new (nothrow) string);
  do {
    hiai_ret = SendData(kSendDataPort, "string",
                        static_pointer_cast<void>(sentinel_msg));
    // when queue full, sleep
    if (hiai_ret == HIAI_QUEUE_FULL) {
      HIAI_ENGINE_LOG("queue full, sleep 200ms");
      usleep(kSleepInterval);
    }
  } while (hiai_ret == HIAI_QUEUE_FULL);

  // send failed
  if (hiai_ret != HIAI_OK) {
    HIAI_ENGINE_LOG(HIAI_ENGINE_RUN_ARGS_NOT_RIGHT,
                    "call SendData failed, err_code=%d", hiai_ret);
    return false;
  }
  return true;
}

HIAI_StatusT GeneralPost::ModelPostProcessCap(const shared_ptr<EngineTrans> &result) {

  vector<Output> outputs = result->inference_res;
  
  if (outputs.size() != kOutputTensorSize) {
    ERROR_LOG("Detection output size does not match.");
    return HIAI_ERROR;
  }
  // cout << "--post-- start get outputs" << endl;
  float *bbox_buffer = reinterpret_cast<float *>(outputs[kOutputTesnorIndex].data.get());
  // cout << "--post-- convert outputs" << endl;
  uint32_t *num_buffer = reinterpret_cast<uint32_t *>(outputs[kOutputNumIndex].data.get());
  Tensor<uint32_t> tensor_num;
  Tensor<float> tensor_bbox;
  bool ret = true;
  ret = tensor_num.FromArray(num_buffer, kDimBBoxCnt);
  if (!ret) {
    ERROR_LOG("Failed to resolve tensor from array.");
    return HIAI_ERROR;
  }
  ret = tensor_bbox.FromArray(bbox_buffer, kDimDetectionOut);
  if (!ret) {
    ERROR_LOG("Failed to resolve tensor from array.");
    return HIAI_ERROR;
  }
  vector<BoundingBox> bboxes;
  for (uint32_t attr = 0; attr < result->console_params.output_nums; ++attr) {
    for (uint32_t bbox_idx = 0; bbox_idx < tensor_num[attr]; ++bbox_idx) {
      uint32_t class_idx = attr * kCategoryIndex;

      uint32_t lt_x = tensor_bbox(class_idx, bbox_idx, BBoxIndex::kTopLeftX);
      uint32_t lt_y = tensor_bbox(class_idx, bbox_idx, BBoxIndex::kTopLeftY);
      uint32_t rb_x = tensor_bbox(class_idx, bbox_idx, BBoxIndex::kLowerRigltX);
      uint32_t rb_y = tensor_bbox(class_idx, bbox_idx, BBoxIndex::kLowerRightY);

      float score = tensor_bbox(class_idx, bbox_idx, BBoxIndex::kScore);
      bboxes.push_back( {lt_x, lt_y, rb_x, rb_y, attr, score});
    }
  }

  // cout << "--post-- get outputs" << endl;
  // cout << "--post-- unsigned char to mat" << endl;
  uint8_t* pdata = result->image_info.data.get();
  cv::Mat yuvImg;
  yuvImg.create(result->image_info.height*3/2, result->image_info.width, CV_8UC1);
  memcpy(yuvImg.data, pdata, result->image_info.size);
  cv::Mat mat;
  cv::cvtColor(yuvImg, mat, CV_YUV2RGB_NV21);
  // crop image
  cv::Rect rect(0,0,801,601);
  cv::Mat imageCrop = mat(rect);
  // resize iamge
  cv::resize(imageCrop, imageCrop, cv::Size(800, 600));

  if (bboxes.empty()) {
    INFO_LOG("There is none object detected in image %s",
             result->image_info.path.c_str());
    // cout << "--post-- mat changed!!" << endl;
    int bytes = 0;
    int image_size = imageCrop.total() * imageCrop.elemSize();
    // cout << "--post-- send image to server, image_size: " << image_size << endl;
    if ((bytes = send(sokt, imageCrop.data, image_size, 0)) < 0){
      close(sokt);
      cout << "bytes = " << bytes << endl;
    }
    return HIAI_OK;
  }

  stringstream sstream;
  for (int i = 0; i < bboxes.size(); ++i) {
    cv::Point p1, p2;
    p1.x = bboxes[i].lt_x;
    p1.y = bboxes[i].lt_y;
    p2.x = bboxes[i].rb_x;
    p2.y = bboxes[i].rb_y;
    cv::rectangle(imageCrop, p1, p2, kColors[i % kColors.size()], kLineSolid);

    sstream.str("");
    sstream << bboxes[i].attribute << " ";
    sstream.precision(kScorePrecision);
    sstream << 100 * bboxes[i].score;
    string obj_str = sstream.str();
    cv::putText(imageCrop, obj_str, cv::Point(p1.x, p1.y + kLabelOffset),
                cv::FONT_HERSHEY_COMPLEX, kFountScale, kFontColor);
  }

  // cout << "--post-- mat changed!!" << endl;
  int bytes = 0;
  int image_size = imageCrop.total() * imageCrop.elemSize();
  // cout << "--post-- send image to server, image_size: " << image_size << endl;
  if ((bytes = send(sokt, imageCrop.data, image_size, 0)) < 0){
    close(sokt);
    cout << "bytes = " << bytes << endl;
  }
  return HIAI_OK;
}

HIAI_StatusT GeneralPost::ModelPostProcessPic(const shared_ptr<EngineTrans> &result) {

  vector<Output> outputs = result->inference_res;
  
  if (outputs.size() != kOutputTensorSize) {
    ERROR_LOG("Detection output size does not match.");
    return HIAI_ERROR;
  }
  // cout << "--post-- start get outputs" << endl;
  float *bbox_buffer = reinterpret_cast<float *>(outputs[kOutputTesnorIndex].data.get());
  // cout << "--post-- convert outputs" << endl;
  uint32_t *num_buffer = reinterpret_cast<uint32_t *>(outputs[kOutputNumIndex].data.get());
  Tensor<uint32_t> tensor_num;
  Tensor<float> tensor_bbox;
  bool ret = true;
  ret = tensor_num.FromArray(num_buffer, kDimBBoxCnt);
  if (!ret) {
    ERROR_LOG("Failed to resolve tensor from array.");
    return HIAI_ERROR;
  }
  ret = tensor_bbox.FromArray(bbox_buffer, kDimDetectionOut);
  if (!ret) {
    ERROR_LOG("Failed to resolve tensor from array.");
    return HIAI_ERROR;
  }

  vector<BoundingBox> bboxes;
  for (uint32_t attr = 0; attr < result->console_params.output_nums; ++attr) {
    for (uint32_t bbox_idx = 0; bbox_idx < tensor_num[attr]; ++bbox_idx) {
      uint32_t class_idx = attr * kCategoryIndex;

      uint32_t lt_x = tensor_bbox(class_idx, bbox_idx, BBoxIndex::kTopLeftX);
      uint32_t lt_y = tensor_bbox(class_idx, bbox_idx, BBoxIndex::kTopLeftY);
      uint32_t rb_x = tensor_bbox(class_idx, bbox_idx, BBoxIndex::kLowerRigltX);
      uint32_t rb_y = tensor_bbox(class_idx, bbox_idx, BBoxIndex::kLowerRightY);

      float score = tensor_bbox(class_idx, bbox_idx, BBoxIndex::kScore);
      bboxes.push_back( {lt_x, lt_y, rb_x, rb_y, attr, score});
    }
  }

  if (bboxes.empty()) {
    INFO_LOG("There is none object detected in image %s",
             result->image_info.path.c_str());
    return HIAI_OK;
  }
  // cout << "get outputs" << endl;

  cv::Mat mat = cv::imread(result->image_info.path, CV_LOAD_IMAGE_UNCHANGED);
  if (mat.empty()) {
    ERROR_LOG("Fialed to deal file=%s. Reason: read image failed.",
              result->image_info.path.c_str());
    return HIAI_ERROR;
  }

  cv::resize(mat, mat, cv::Size(800, 600));

  // float scale_width = (float)mat.cols / result->image_info.width;
  // float scale_height = (float)mat.rows / result->image_info.height;
  stringstream sstream;
  for (int i = 0; i < bboxes.size(); ++i) {
    cv::Point p1, p2;
    p1.x = bboxes[i].lt_x;
    p1.y = bboxes[i].lt_y;
    p2.x = bboxes[i].rb_x;
    p2.y = bboxes[i].rb_y;
    cv::rectangle(mat, p1, p2, kColors[i % kColors.size()], kLineSolid);

    sstream.str("");
    sstream << bboxes[i].attribute << " ";
    sstream.precision(kScorePrecision);
    sstream << 100 * bboxes[i].score;
    string obj_str = sstream.str();
    cv::putText(mat, obj_str, cv::Point(p1.x, p1.y + kLabelOffset),
                cv::FONT_HERSHEY_COMPLEX, kFountScale, kFontColor);
  }

  // cout << "mat changed!!" << endl;
  int bytes = 0;
  int image_size = mat.total() * mat.elemSize();
  // cout << "--post-- send image to server, image_size: " << image_size << endl;
  if ((bytes = send(sokt, mat.data, image_size, 0)) < 0){
    close(sokt);
    cout << "bytes = " << bytes << endl;
  }
  return HIAI_OK;
}

HIAI_IMPL_ENGINE_PROCESS("general_post", GeneralPost, INPUT_SIZE) {
  HIAI_StatusT ret = HIAI_OK;

  // check arg0
  if (arg0 == nullptr) {
    ERROR_LOG("Failed to deal file=nothing. Reason: arg0 is empty.");
    return HIAI_ERROR;
  }

  // just send to callback function when finished
  shared_ptr<EngineTrans> result = static_pointer_cast<EngineTrans>(arg0);
  if (result->is_finished) {
    cout << "--post-- finished" << endl;
    close(sokt);
    if (SendSentinel()) {
      return HIAI_OK;
    }
    ERROR_LOG("Failed to send finish data. Reason: SendData failed.");
    ERROR_LOG("Please stop this process manually.");
    return HIAI_ERROR;
  }

  // inference failed
  if (result->err_msg.error) {
    ERROR_LOG("%s", result->err_msg.err_msg.c_str());
    return HIAI_ERROR;
  }
  // arrange result
  if (result->image_info.mode==0) {
    return ModelPostProcessCap(result);
  }
  else {
    return ModelPostProcessPic(result);
  }
  return HIAI_ERROR;
}
