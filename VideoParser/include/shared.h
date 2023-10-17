/**
 * @file shared.h
 * @author Werner Robitza
 * @copyright Copyright (c) 2023, AVEQ GmbH. Copyright (c) 2023, videoparser-ng
 * contributors.
 */

#ifndef VIDEOPARSER_SHARED_H
#define VIDEOPARSER_SHARED_H

/**
 * @brief Shared frame information to extract from ffmpeg into the parser.
 */
typedef struct SharedFrameInfo {
  int frame_idx; /**< Index of the frame in the video stream, just a dummy value
                  */
} SharedFrameInfo;

#endif
