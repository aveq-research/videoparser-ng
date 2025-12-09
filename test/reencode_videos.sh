#!/bin/bash
#
# Very simple script to re-encode all y4m videos in a directory.
# Uses fixed settings for each codec.

INPUT_DIR="/Volumes/Data/Databases/a2_2k"
OUTPUT_DIR="/Volumes/Data/Databases/a2_2k_reencoded"

# Function to encode a single file with all codecs
encode_file() {
    local input_file="$1"
    local basename
    basename=$(basename "$input_file" .y4m)

    echo "Processing: $basename"

    # AV1 (librav1e)
    ffmpeg -y -i "$input_file" -c:v librav1e -c:a copy "$OUTPUT_DIR/${basename}-av1.mkv" 2>/dev/null &

    # HEVC (hevc_videotoolbox - macOS hardware encoder)
    ffmpeg -y -i "$input_file" -c:v hevc_videotoolbox -c:a copy "$OUTPUT_DIR/${basename}-hevc.mkv" 2>/dev/null &

    # H.264 (h264_videotoolbox - macOS hardware encoder)
    ffmpeg -y -i "$input_file" -c:v h264_videotoolbox -c:a copy "$OUTPUT_DIR/${basename}-h264.mkv" 2>/dev/null &

    # VP9 (libvpx-vp9)
    ffmpeg -y -i "$input_file" -c:v libvpx-vp9 -c:a copy "$OUTPUT_DIR/${basename}-vp9.mkv" 2>/dev/null &

    # Wait for all 4 encodings of this file to complete
    wait

    echo "Completed: $basename"
}

export -f encode_file
export OUTPUT_DIR

echo "Starting re-encoding of y4m files in $INPUT_DIR..."

# Find all y4m files and process them (excluding macOS resource forks)
# Using GNU parallel if available, otherwise xargs
if command -v parallel &> /dev/null; then
    find "$INPUT_DIR" -type f -name "*.y4m" ! -name "._*" | \
        parallel -j 4 encode_file {}
else
    echo "GNU parallel not found, using xargs for sequential processing."
    # Fallback: process files sequentially (each file's 4 codecs run in parallel)
    find "$INPUT_DIR" -type f -name "*.y4m" ! -name "._*" | \
        while read -r file; do
            encode_file "$file"
        done
fi

echo "All encoding complete!"
echo "Re-encoded files are located in $OUTPUT_DIR."
