/**
 * @file shared.h
 * @author Werner Robitza
 * @copyright Copyright (c) 2023, AVEQ GmbH. Copyright (c) 2023, videoparser-ng
 * contributors.
 */

#ifndef VIDEOPARSER_SHARED_H
#define VIDEOPARSER_SHARED_H

#include <math.h>

/**
 * @brief Shared frame information to extract from ffmpeg into the parser.
 * This is a struct that is shared between the parser and the ffmpeg library.
 * Some fields are staying inside this struct, others will be copied into the
 * parser.
 */
typedef struct SharedFrameInfo {
  int frame_idx; /**< Index of the frame in the video stream, just a dummy value
                  */

  // INTERNAL -- stays in the struct
  // temporary QP values
  uint32_t qp_sum;        /**< Sum of QP values */
  uint32_t qp_sum_sqr;    /**< Sum of squared QP values */
  uint32_t qp_cnt;        /**< Count of QP values */
  uint32_t qp_sum_bb;     /**< Sum of QP values without black border */
  uint32_t qp_sum_sqr_bb; /**< Sum of squared QP values without black border */
  uint32_t qp_cnt_bb;     /**< Count of QP values without black border */

  // EXTERNAL -- shared with videoparser
  // derived QP values
  uint32_t qp_min;  /**< Minimum QP value encountered in this frame */
  uint32_t qp_max;  /**< Maximum QP value encountered in this frame */
  uint32_t qp_init; /**< QP Value the frame is starting with (to be found in the
                       slice- or frame-header) */
  double qp_avg;    /**< Average QP of the whole frame */
  double qp_stdev;  /**< Standard deviation of Av_QP */
  double qp_bb_avg; /**< Average QP without the black border */
  double qp_bb_stdev; /**< Standard deviation of the average QP */

  /**
   * @brief Update QP statistics with a new QP value
   * @param sf Pointer to SharedFrameInfo struct
   * @param qp New QP value to add
   */
  void (*update_qp)(struct SharedFrameInfo *sf, uint32_t qp);
} SharedFrameInfo;

#endif
