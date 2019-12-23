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

#include "general_image.h"

#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <memory>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <stdlib.h>
#include <time.h>
#include <cstring>
#include <chrono>

#include "hiaiengine/log.h"
#include "opencv2/opencv.hpp"
#include "tool_api.h"

extern "C" {
#include "driver/peripheral_api.h"
}

using hiai::Engine;
using namespace std;

namespace {
// output port (engine port begin with 0)
const uint32_t kSendDataPort = 0;

// sleep interval when queue full (unit:microseconds)
const __useconds_t kSleepInterval = 200000;

// get stat success
const int kStatSuccess = 0;
// image file path split character
const string kImagePathSeparator = ",";
// path separator
const string kPathSeparator = "/";

}

// register custom data type
HIAI_REGISTER_DATA_TYPE("EngineTrans", EngineTrans);

GeneralImage::GeneralImage() {
  config_ = nullptr;
  frame_id_ = 0;
  exit_flag_ = CAMERADATASETS_INIT;
  params_.insert(pair<string, string>("Channel-1", IntToString(CAMERAL_1)));
  params_.insert(pair<string, string>("Channel-2", IntToString(CAMERAL_2)));
  params_.insert(pair<string, string>("YUV420SP", IntToString(CAMERA_IMAGE_YUV420_SP)));
}

GeneralImage::~GeneralImage() {
}

HIAI_StatusT GeneralImage::Init(
    const hiai::AIConfig& ai_config,
    const vector<hiai::AIModelDescription>& model_desc) {
  HIAI_ENGINE_LOG("[CameraDatasets] start init!");
  if (config_ == nullptr) {
    config_ = make_shared<CameraDatasetsConfig>();
  }

  for (int index = 0; index < ai_config.items_size(); ++index) {
    const ::hiai::AIConfigItem& item = ai_config.items(index);
    string name = item.name();
    string value = item.value();

    if (name == "fps") {
      config_->fps = atoi(value.data());
    } else if (name == "image_format") {
      config_->image_format = CommonParseParam(value);
    } else if (name == "data_source") {
      config_->channel_id = CommonParseParam(value);
    } else if (name == "image_size") {
      ParseImageSize(value, config_->resolution_width,
                     config_->resolution_height);
    } else if (name == "image_num") {
      config_->image_num = atoi(value.data());
    } else if (name == "mode") {
      config_->mode = atoi(value.data());
    } else {
      HIAI_ENGINE_LOG("unused config name: %s", name.c_str());
    }
  }

  HIAI_StatusT ret = HIAI_OK;
  bool failed_flag = (config_->image_format == PARSEPARAM_FAIL
      || config_->channel_id == PARSEPARAM_FAIL
      || config_->resolution_width == 0 || config_->resolution_height == 0);
  if (failed_flag) {
    string msg = config_->ToString();
    msg.append(" config data failed");
    HIAI_ENGINE_LOG(msg.data());
    ret = HIAI_ERROR;
  }
  HIAI_ENGINE_LOG("[CameraDatasets] end init!");
  return ret;
}

string GeneralImage::IntToString(int value) {
  char msg[MAX_VALUESTRING_LENGTH] = { 0 };
  // MAX_VALUESTRING_LENGTH ensure no error occurred
  sprintf_s(msg, MAX_VALUESTRING_LENGTH, "%d", value);
  string ret = msg;

  return ret;
}

bool GeneralImage::ArrangeImageInfo(shared_ptr<EngineTrans> &image_handle,
                                    const string &image_path) {
  // read image using OPENCV
  cv::Mat mat = cv::imread(image_path, CV_LOAD_IMAGE_COLOR);
  if (mat.empty()) {
    ERROR_LOG("Failed to deal file=%s. Reason: read image failed.",
              image_path.c_str());
    return false;
  }

  // set property
  image_handle->image_info.path = image_path;
  image_handle->image_info.width = mat.cols;
  image_handle->image_info.height = mat.rows;

  // set image data
  // cout << "--image-- mat.total(): " << mat.total() << endl;
  uint32_t size = mat.total() * mat.channels();
  u_int8_t *image_buf_ptr = new (nothrow) u_int8_t[size];
  if (image_buf_ptr == nullptr) {
    HIAI_ENGINE_LOG("new image buffer failed, size=%d!", size);
    ERROR_LOG("Failed to deal file=%s. Reason: new image buffer failed.",
              image_path.c_str());
    return false;
  }
  // cout << "--image-- copy mat from image" << endl;
  error_t mem_ret = memcpy_s(image_buf_ptr, size, mat.ptr<u_int8_t>(),
                             mat.total() * mat.channels());
  if (mem_ret != EOK) {
    cout << "--image-- copy mat from image failed" << endl;
    delete[] image_buf_ptr;
    ERROR_LOG("Failed to deal file=%s. Reason: memcpy_s failed.",
              image_path.c_str());
    image_buf_ptr = nullptr;
    return false;
  }

  image_handle->image_info.size = size;
  image_handle->image_info.data.reset(image_buf_ptr,
                                      default_delete<u_int8_t[]>());
  return true;
}

bool GeneralImage::SendToEngine(const shared_ptr<EngineTrans> &image_handle) {
  // can not discard when queue full
  HIAI_StatusT hiai_ret;
  do {
    hiai_ret = SendData(kSendDataPort, "EngineTrans",
                        static_pointer_cast<void>(image_handle));
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

int GeneralImage::CommonParseParam(const string& val) const {
  map<string, string>::const_iterator iter = params_.find(val);
  if (iter != params_.end()) {
    return atoi((iter->second).c_str());
  }

  return PARSEPARAM_FAIL;
}

void GeneralImage::SplitString(const string& source, vector<string>& tmp,
                                      const string& obj) {
  string::size_type pos1 = 0;
  string::size_type pos2 = source.find(obj);

  while (string::npos != pos2) {
    tmp.push_back(source.substr(pos1, pos2 - pos1));
    pos1 = pos2 + obj.size();
    pos2 = source.find(obj, pos1);
  }

  if (pos1 != source.length()) {
    tmp.push_back(source.substr(pos1));
  }
}

void GeneralImage::ParseImageSize(const string& val, int& width,
                                         int& height) const {
  vector < string > tmp;
  SplitString(val, tmp, "x");

  // val is not a format of resolution ratio(*x*),correct should have 2 array
  // in this wrong case,set width and height zero
  if (tmp.size() != 2) {
    width = 0;
    height = 0;
  } else {
    width = atoi(tmp[0].c_str());
    height = atoi(tmp[1].c_str());
  }
}

string GeneralImage::CameraDatasetsConfig::ToString() const {
  stringstream log_info_stream("");
  log_info_stream << "fps:" << this->fps << ", camera:" << this->channel_id
      << ", image_format:" << this->image_format << ", resolution_width:"
      << this->resolution_width << ", resolution_height:"
      << this->resolution_height;

  return log_info_stream.str();
}

void GeneralImage::SetExitFlag(int flag) {
  TLock lock(mutex_);
  exit_flag_ = flag;
}

int GeneralImage::GetExitFlag() {
  TLock lock(mutex_);
  return exit_flag_;
}

GeneralImage::CameraOperationCode GeneralImage::PreCapProcess() {
  cout << "--image-- start prepare camera" << endl;
  MediaLibInit();
  CameraStatus status = QueryCameraStatus(config_->channel_id);
  // cout << "--image-- camera status: " << status << endl;
  if (status != CAMERA_STATUS_CLOSED) {
    HIAI_ENGINE_LOG("[CameraDatasets] PreCapProcess.QueryCameraStatus "
                    "{status:%d} failed.",status);
    cout << "--image-- camera not closed!!" << endl;
    return kCameraNotClosed;
  }
  // Open Camera
  cout << "--image-- open camera" << endl;
  int ret = OpenCamera(config_->channel_id);
  // return 0 indicates failure
  if (ret == 0) {
    HIAI_ENGINE_LOG("[CameraDatasets] PreCapProcess OpenCamera {%d} "
                    "failed.",config_->channel_id);
    cout << "camera open failed!!" << endl;
    return kCameraOpenFailed;
  }
  // set fps
  // cout << "--image-- set camera fps" << endl;
  ret = SetCameraProperty(config_->channel_id, CAMERA_PROP_FPS, &(config_->fps));
  // return 0 indicates failure
  if (ret == 0) {
    HIAI_ENGINE_LOG("[CameraDatasets] PreCapProcess set fps {fps:%d} "
                    "failed.",config_->fps);
    cout << "--image-- camera set fps failed!!" << endl;
    return kCameraSetPropeptyFailed;
  }
  // set image format
  // cout << "--image-- set camera image_format" << endl;
  ret = SetCameraProperty(config_->channel_id, CAMERA_PROP_IMAGE_FORMAT,
                          &(config_->image_format));
  // return 0 indicates failure
  if (ret == 0) {
    HIAI_ENGINE_LOG("[CameraDatasets] PreCapProcess set image_fromat "
                    "{format:%d} failed.",config_->image_format);
    cout << "--image-- camera set format failed!!" << endl;
    return kCameraSetPropeptyFailed;
  }
  // set image resolution.
  CameraResolution resolution;
  resolution.width = config_->resolution_width;
  resolution.height = config_->resolution_height;
  // cout << "--image-- set camera image resolution" << endl;
  ret = SetCameraProperty(config_->channel_id, CAMERA_PROP_RESOLUTION,
                          &resolution);
  // return 0 indicates failure
  if (ret == 0) {
    HIAI_ENGINE_LOG("[CameraDatasets] PreCapProcess set resolution "
                    "{width:%d, height:%d } failed.",
                    config_->resolution_width, config_->resolution_height);
    cout << "--image-- camera set resolution failed!!" << endl;
    return kCameraSetPropeptyFailed;
  }

  // set work mode
  CameraCapMode mode = CAMERA_CAP_ACTIVE;
  // cout << "--image-- set camera work mode" << endl;
  ret = SetCameraProperty(config_->channel_id, CAMERA_PROP_CAP_MODE, &mode);
  // return 0 indicates failure
  if (ret == 0) {
    HIAI_ENGINE_LOG("[CameraDatasets] PreCapProcess set cap mode {mode:%d}"
                    " failed.",mode);
    cout << "--image-- camera set mode failed!!" << endl;
    return kCameraSetPropeptyFailed;
  }

  return kCameraOk;
}

bool GeneralImage::DoCapProcess() {
  CameraOperationCode ret_code = PreCapProcess();
  cout << "--image-- prepare camera ok" << endl;
  if (ret_code == kCameraSetPropeptyFailed) {
    CloseCamera(config_->channel_id);
    HIAI_ENGINE_LOG("[CameraDatasets] DoCapProcess.PreCapProcess failed");
    cout << "--image-- DoCapProcess.PreCapProcess failed, ret_code: " << ret_code << endl;
    return false;
  }
  // set procedure is running.
  // cout << "--image-- set camera procedure is running" << endl;
  SetExitFlag (CAMERADATASETS_RUN);

  int read_ret = 0;
  int read_size = 0;
  bool read_flag = false;
  int read_num = 0;

  while (GetExitFlag() == CAMERADATASETS_RUN) {

    read_num += 1;

    // new image_handle
    shared_ptr<EngineTrans> image_handle = nullptr;
    MAKE_SHARED_NO_THROW(image_handle, EngineTrans);

    image_handle->image_info.width = config_->resolution_width;
    image_handle->image_info.height = config_->resolution_height;
    image_handle->image_info.size = config_->resolution_width * config_->resolution_height * 3 / 2;
    image_handle->image_info.mode = config_->mode;
    shared_ptr <uint8_t> data(new uint8_t[image_handle->image_info.size], default_delete<uint8_t[]>());
    image_handle->image_info.data = data;
    char infopath[12];
    sprintf(infopath, "%d.png", read_num);
    image_handle->image_info.path = infopath;
    image_handle->console_params.model_height = 600;
    image_handle->console_params.model_width = 800;
    image_handle->console_params.output_path = "./";
    image_handle->console_params.output_nums = 21;
    
    read_size = (int) image_handle->image_info.size;
    uint8_t* pdata = image_handle->image_info.data.get();
    // do read frame from camera
    read_ret = ReadFrameFromCamera(config_->channel_id, (void*) pdata, &read_size);
    // indicates failure when readRet is 1
    read_flag = ((read_ret == 1) && (read_size == (int) image_handle->image_info.size));
    if (!read_flag) {
      HIAI_ENGINE_LOG("[CameraDatasets] readFrameFromCamera failed "
                      "{camera:%d, ret:%d, size:%d, expectsize:%d} ",
                      config_->channel_id, read_ret, read_size,
                      (int) image_handle->image_info.size);
      cout << "--image-- readFrameFromCamera failed" << endl;
      break;
    }
    if (read_num < 5) {
      continue;
    }
    else {
      // cout << "--image-- send to inference engine" << endl;
      SendToEngine(image_handle);
      if (read_num >= config_->image_num+5) break;
    }
  }

  CloseCamera(config_->channel_id);
  cout << "--image-- close camera" << endl;

  return true;
}

bool GeneralImage::DoPictureProcess() {
  cout << "--image-- picture test" << endl;
  int read_num = 0;
  while (true) {
    read_num += 1;
    string path = "test.png";
    // Step3: send every image to inference engine
    shared_ptr<EngineTrans> image_handle = nullptr;
    MAKE_SHARED_NO_THROW(image_handle, EngineTrans);
    if (image_handle == nullptr) {
      ERROR_LOG("Failed to deal file=%s. Reason: new EngineTrans failed.",
                path.c_str());
      return HIAI_ERROR;
    }
    // arrange image information, if failed, skip this image
    if (!ArrangeImageInfo(image_handle, path)) {
      break;
    }
    // send data to inference engine
    image_handle->console_params.input_path = "test.png";
    image_handle->console_params.model_height = 600;
    image_handle->console_params.model_width = 800;
    image_handle->console_params.output_path = "./";
    image_handle->console_params.output_nums = 21;
    image_handle->image_info.mode = config_->mode;
    if (read_num < 5) {
      continue;
    }
    else {
      // cout << "--image-- send to inference engine" << endl;
      SendToEngine(image_handle);
      if (read_num >= config_->image_num+5) break;
    }
  }
  return true;
}

HIAI_IMPL_ENGINE_PROCESS("general_image",
    GeneralImage, INPUT_SIZE) {
  
  if (arg0 == nullptr) {
    ERROR_LOG("Failed to deal file=nothing. Reason: arg0 is empty.");
    return HIAI_ERROR;
  }
  shared_ptr<string> src_data = static_pointer_cast<string>(arg0);
  if (*src_data=="1") {
    config_->mode = 1;
  }
  bool status;
  if (config_->mode==0) {
    status = DoCapProcess();
  } else {
    status = DoPictureProcess();
  }
  if (!status) cout << "--image-- DoCapProcess failed" << endl;

  // send finished data
  shared_ptr<EngineTrans> image_handle2 = nullptr;
  MAKE_SHARED_NO_THROW(image_handle2, EngineTrans);
  if (image_handle2 == nullptr) {
    ERROR_LOG("Failed to send finish data. Reason: new EngineTrans failed.");
    ERROR_LOG("Please stop this process manually.");
    return HIAI_ERROR;
  }

  image_handle2->is_finished = true;

  if (SendToEngine(image_handle2)) {
    return HIAI_OK;
  }
  ERROR_LOG("Failed to send finish data. Reason: SendData failed.");
  ERROR_LOG("Please stop this process manually.");
  return HIAI_ERROR;
}
