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

#ifndef GENERAL_IMAGE_GENERAL_IMAGE_H_
#define GENERAL_IMAGE_GENERAL_IMAGE_H_

#include <iostream>
#include <string>
#include <vector>
#include <stdint.h>
#include <stdio.h>

#include "hiaiengine/engine.h"
#include "hiaiengine/data_type.h"
#include "hiaiengine/data_type_reg.h"
#include "data_type.h"

#define CAMERAL_1 (0)
#define CAMERAL_2 (1)

#define INPUT_SIZE 1
#define OUTPUT_SIZE 1

#define CAMERADATASETS_INIT (0)
#define CAMERADATASETS_RUN  (1)
#define CAMERADATASETS_STOP (2)
#define CAMERADATASETS_EXIT (3)

#define PARSEPARAM_FAIL (-1)
#define MAX_VALUESTRING_LENGTH 25

/**
 * @brief: inference engine class
 */
class GeneralImage : public hiai::Engine {
public:
  struct CameraDatasetsConfig {
    int fps;
    int channel_id;
    int image_format;
    int resolution_width;
    int resolution_height;
    int image_num;
    int mode;
    std::string ToString() const;
  };

  enum CameraOperationCode {
    kCameraOk = 0,
    kCameraNotClosed = -1,
    kCameraOpenFailed = -2,
    kCameraSetPropeptyFailed = -3,
  };

  /**
   * @brief   constructor
   */
  GeneralImage();

  /**
   * @brief   destructor
   */
  ~GeneralImage();

  /**
   * @brief: engine initialize
   * @param [in]: engine's parameters which configured in graph.config
   * @param [in]: model description
   * @return: HIAI_StatusT
   */
  HIAI_StatusT Init(const hiai::AIConfig& config,
                    const std::vector<hiai::AIModelDescription>& model_desc);

  /**
   * @brief   translate value to string
   * @param [in] value    channel id of camera
   * @return   string translate by value
   */
  static std::string IntToString(int value);

  /**
   * @brief  Splite String source by obj and store in tmp
   * @param [in] string source
   * @param [in] string obj     value used to cut resolution ratio
   * @param [in] vector tmp     used to conserve the value of width and heigth
   * @param [out] vector tmp    value of width and heigth
   */
  static void SplitString(const std::string& source,
                          std::vector<std::string>& tmp,
                          const std::string& obj);

  /**
   * @brief: engine processor which override HIAI engine
   *         get every image, and then send data to inference engine
   * @param [in]: input size
   * @param [in]: output size
   */
  HIAI_DEFINE_PROCESS(INPUT_SIZE, OUTPUT_SIZE);

private:

  /**
   * @brief: arrange image information
   * @param [out]: image_handle: image handler
   * @param [in]: image file path
   * @return: true: success; false: failed
   */
  bool ArrangeImageInfo(std::shared_ptr<EngineTrans> &image_handle,
                        const std::string &image_path);

  /**
   * @brief: send result
   * @param [in]: image_handle: engine transform image
   * @return: true: success; false: failed
   */
  bool SendToEngine(const std::shared_ptr<EngineTrans> &image_handle);

  /**
   * @brief  cap camera
   * @return  success-->true ; fail-->false
   */
  bool DoCapProcess();

  /**
   * @brief  picture
   * @return  success-->true ; fail-->false
   */
  bool DoPictureProcess();

  /**
   * @brief   preprocess for cap camera
   * @return  camera code
   */
  GeneralImage::CameraOperationCode PreCapProcess();

  /**
   * @brief  parse param
   * @return value of config
   */
  int CommonParseParam(const std::string& val) const;

  /**
   * @brief  get width and height from string val
   * @param [in]  val     resolution ratio of picture
   * @param [in]  width   used ot conserve width of picture
   * @param [in]  height  used ot conserve height of picture
   * @param [out] width   value of width
   * @param [out] height  value of height
   */
  void ParseImageSize(const std::string& val, int& width, int& height) const;

  /**
   * @brief  get exit flag
   * @return the value of exit
   */
  int GetExitFlag();

  /**
   * @brief  set exit flag
   * @param [in]  which value want to set
   */
  void SetExitFlag(int flag = CAMERADATASETS_STOP);

private:
    typedef std::unique_lock<std::mutex> TLock;
    std::shared_ptr<CameraDatasetsConfig> config_;
    std::map<std::string, std::string> params_;
    // thread variable to protect exitFlag
    std::mutex mutex_;
    // ret of cameradataset
    int exit_flag_;
    uint32_t frame_id_;

};

#endif /* GENERAL_IMAGE_GENERAL_IMAGE_H_ */
