#!/bin/bash

DIR="."
OUTDIR="./combined"
mkdir -p "$OUTDIR"
GAP=20  # gap in pixels
TARGET_SIZE="1600x900"

for cov_file in "$DIR"/*_covariance.png; do
    prefix=$(basename "$cov_file" "_covariance.png")
    agree_file="$DIR/${prefix}_agreement.png"

    if [[ -f "$agree_file" ]]; then
        out_file="$OUTDIR/${prefix}_combined.png"

        # Get heights of both images
        cov_height=$(identify -format "%h" "$cov_file")
        agree_height=$(identify -format "%h" "$agree_file")

        # Max height
        max_height=$(( cov_height > agree_height ? cov_height : agree_height ))

        # Calculate padding for vertical centering
        cov_pad_top=$(( (max_height - cov_height) / 2 ))
        cov_pad_bottom=$(( max_height - cov_height - cov_pad_top ))

        agree_pad_top=$(( (max_height - agree_height) / 2 ))
        agree_pad_bottom=$(( max_height - agree_height - agree_pad_top ))

        # Combine and resize to target
        convert \
            \( "$cov_file" -background none -gravity north -splice 0x$cov_pad_top -background none -gravity south -splice 0x$cov_pad_bottom \) \
            \( -size ${GAP}x${max_height} xc:none \) \
            \( "$agree_file" -background none -gravity north -splice 0x$agree_pad_top -background none -gravity south -splice 0x$agree_pad_bottom \) \
            +append \
            -resize "$TARGET_SIZE" \
            "$out_file"

        echo "Combined and resized -> $out_file"
    else
        echo "No agreement file for $cov_file, skipping."
    fi
done
