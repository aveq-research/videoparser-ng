/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "VideoParser", "index.html", [
    [ "VideoParser – The Next Generation", "index.html", "index" ],
    [ "Developer Guide", "d0/dc5/md_DEVELOPERS.html", [
      [ "General Structure", "d0/dc5/md_DEVELOPERS.html#autotoc_md22", null ],
      [ "Modifications Made", "d0/dc5/md_DEVELOPERS.html#autotoc_md23", [
        [ "QP Information", "d0/dc5/md_DEVELOPERS.html#autotoc_md24", null ],
        [ "Motion Vector Information", "d0/dc5/md_DEVELOPERS.html#autotoc_md25", null ],
        [ "AV1 / libaom Specific Changes", "d0/dc5/md_DEVELOPERS.html#autotoc_md26", [
          [ "MVD and Bit Count Extraction", "d0/dc5/md_DEVELOPERS.html#autotoc_md27", null ]
        ] ]
      ] ],
      [ "Metrics", "d0/dc5/md_DEVELOPERS.html#autotoc_md28", [
        [ "QP Metrics", "d0/dc5/md_DEVELOPERS.html#autotoc_md29", [
          [ "qp_avg", "d0/dc5/md_DEVELOPERS.html#autotoc_md30", null ],
          [ "qp_stdev", "d0/dc5/md_DEVELOPERS.html#autotoc_md31", null ],
          [ "qp_min", "d0/dc5/md_DEVELOPERS.html#autotoc_md32", null ],
          [ "qp_max", "d0/dc5/md_DEVELOPERS.html#autotoc_md33", null ],
          [ "qp_init", "d0/dc5/md_DEVELOPERS.html#autotoc_md34", null ],
          [ "qp_bb_avg", "d0/dc5/md_DEVELOPERS.html#autotoc_md35", null ],
          [ "qp_bb_stdev", "d0/dc5/md_DEVELOPERS.html#autotoc_md36", null ]
        ] ],
        [ "Motion Vector Metrics", "d0/dc5/md_DEVELOPERS.html#autotoc_md37", [
          [ "motion_avg", "d0/dc5/md_DEVELOPERS.html#autotoc_md38", null ],
          [ "motion_stdev", "d0/dc5/md_DEVELOPERS.html#autotoc_md39", null ],
          [ "motion_x_avg", "d0/dc5/md_DEVELOPERS.html#autotoc_md40", null ],
          [ "motion_y_avg", "d0/dc5/md_DEVELOPERS.html#autotoc_md41", null ],
          [ "motion_x_stdev", "d0/dc5/md_DEVELOPERS.html#autotoc_md42", null ],
          [ "motion_y_stdev", "d0/dc5/md_DEVELOPERS.html#autotoc_md43", null ],
          [ "motion_diff_avg", "d0/dc5/md_DEVELOPERS.html#autotoc_md44", null ],
          [ "motion_diff_stdev", "d0/dc5/md_DEVELOPERS.html#autotoc_md45", null ]
        ] ],
        [ "Bit Count Metrics", "d0/dc5/md_DEVELOPERS.html#autotoc_md46", [
          [ "motion_bit_count", "d0/dc5/md_DEVELOPERS.html#autotoc_md47", null ],
          [ "coefs_bit_count", "d0/dc5/md_DEVELOPERS.html#autotoc_md48", null ]
        ] ],
        [ "Block Count Metrics", "d0/dc5/md_DEVELOPERS.html#autotoc_md49", [
          [ "mb_mv_count", "d0/dc5/md_DEVELOPERS.html#autotoc_md50", null ],
          [ "mv_coded_count", "d0/dc5/md_DEVELOPERS.html#autotoc_md51", null ]
        ] ],
        [ "POC Metrics", "d0/dc5/md_DEVELOPERS.html#autotoc_md52", [
          [ "current_poc", "d0/dc5/md_DEVELOPERS.html#autotoc_md53", null ],
          [ "poc_diff", "d0/dc5/md_DEVELOPERS.html#autotoc_md54", null ]
        ] ],
        [ "Frame Metadata", "d0/dc5/md_DEVELOPERS.html#autotoc_md55", [
          [ "frame_type", "d0/dc5/md_DEVELOPERS.html#autotoc_md56", null ],
          [ "frame_idx", "d0/dc5/md_DEVELOPERS.html#autotoc_md57", null ],
          [ "is_idr", "d0/dc5/md_DEVELOPERS.html#autotoc_md58", null ],
          [ "size", "d0/dc5/md_DEVELOPERS.html#autotoc_md59", null ],
          [ "pts", "d0/dc5/md_DEVELOPERS.html#autotoc_md60", null ],
          [ "dts", "d0/dc5/md_DEVELOPERS.html#autotoc_md61", null ]
        ] ]
      ] ],
      [ "Differences with Legacy Implementation", "d0/dc5/md_DEVELOPERS.html#autotoc_md62", [
        [ "All Codecs: POC-based Motion Vector Normalization", "d0/dc5/md_DEVELOPERS.html#autotoc_md63", null ],
        [ "VP9: Motion Statistics for All Inter Modes", "d0/dc5/md_DEVELOPERS.html#autotoc_md64", null ],
        [ "VP9: Motion Bit Count", "d0/dc5/md_DEVELOPERS.html#autotoc_md65", null ],
        [ "VP9: Removed Weighted Variance Accumulation", "d0/dc5/md_DEVELOPERS.html#autotoc_md66", null ],
        [ "All Codecs: Removed Arbitrary Scaling Factors", "d0/dc5/md_DEVELOPERS.html#autotoc_md67", null ]
      ] ],
      [ "Testing", "d0/dc5/md_DEVELOPERS.html#autotoc_md68", [
        [ "Feature Testing", "d0/dc5/md_DEVELOPERS.html#autotoc_md69", null ],
        [ "Regenerating Test Reference Files", "d0/dc5/md_DEVELOPERS.html#autotoc_md70", null ],
        [ "Legacy Testing", "d0/dc5/md_DEVELOPERS.html#autotoc_md71", null ],
        [ "CLI Testing", "d0/dc5/md_DEVELOPERS.html#autotoc_md72", null ]
      ] ],
      [ "Debugging", "d0/dc5/md_DEVELOPERS.html#autotoc_md73", null ],
      [ "Maintenance", "d0/dc5/md_DEVELOPERS.html#autotoc_md74", [
        [ "Fetching new FFmpeg commits", "d0/dc5/md_DEVELOPERS.html#autotoc_md75", null ],
        [ "Fetching new libaom commits", "d0/dc5/md_DEVELOPERS.html#autotoc_md76", null ]
      ] ],
      [ "Black Border Implementation Notes", "d0/dc5/md_DEVELOPERS.html#autotoc_md77", [
        [ "Algorithm Details", "d0/dc5/md_DEVELOPERS.html#autotoc_md78", null ],
        [ "1. BlackLine Array Population (Per-Codec)", "d0/dc5/md_DEVELOPERS.html#autotoc_md79", [
          [ "2. Black Border Detection (VideoStatCommon.c:12-31)", "d0/dc5/md_DEVELOPERS.html#autotoc_md80", null ],
          [ "3. Threshold Values (Codec-Specific)", "d0/dc5/md_DEVELOPERS.html#autotoc_md81", null ],
          [ "4. QP Statistics with Border Exclusion", "d0/dc5/md_DEVELOPERS.html#autotoc_md82", null ],
          [ "5. Final Computation (VideoStatCommon.c:165-171)", "d0/dc5/md_DEVELOPERS.html#autotoc_md83", null ],
          [ "FFmpeg Modifications", "d0/dc5/md_DEVELOPERS.html#autotoc_md84", null ],
          [ "Design Decisions", "d0/dc5/md_DEVELOPERS.html#autotoc_md85", null ]
        ] ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", null ],
        [ "Functions", "functions_func.html", null ],
        [ "Variables", "functions_vars.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html"
];

var SYNCONMSG = 'click to disable panel synchronization';
var SYNCOFFMSG = 'click to enable panel synchronization';
var LISTOFALLMEMBERS = 'List of all members';