#!/usr/bin/env bash
# Builds the pinned FFmpeg as static libraries into libs/ffmpeg-install.
# Video import/export only: decoders for the formats other DAWs accept, encoders for
# ProRes, DNxHR, H.264/HEVC (VideoToolbox on macOS) and PCM/AAC audio. No GPL parts.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/libs/ffmpeg"
OUT="$ROOT/libs/ffmpeg-install"
URL=$(awk '$1=="ffmpeg"{print $2}' "$ROOT/deps.lock")
REV=$(awk '$1=="ffmpeg"{print $3}' "$ROOT/deps.lock")

if [ -f "$OUT/.rev" ] && [ "$(cat "$OUT/.rev")" = "$REV" ]; then
    echo "FFmpeg $REV already built"; exit 0
fi

if [ ! -d "$SRC/.git" ] || [ "$(git -C "$SRC" rev-parse HEAD 2>/dev/null)" != "$REV" ]; then
    rm -rf "$SRC"
    git init -q "$SRC"
    git -C "$SRC" remote add origin "$URL"
    git -C "$SRC" fetch -q --depth 1 origin "$REV"
    git -C "$SRC" checkout -q FETCH_HEAD
fi

DECODERS="prores,dnxhd,h264,hevc,mpeg1video,mpeg2video,mpeg4,mjpeg,dvvideo,vp8,vp9,av1,png,rawvideo,v210,v210x,ffv1,cfhd,hap,qtrle,targa,tiff,\
pcm_s16le,pcm_s16be,pcm_s24le,pcm_s24be,pcm_s32le,pcm_s32be,pcm_f32le,pcm_f32be,pcm_f64le,pcm_s24daud,pcm_bluray,pcm_dvd,\
aac,aac_latm,ac3,eac3,mp2,mp3,alac,flac,opus,vorbis,dca,truehd,mlp"
DEMUXERS="mov,matroska,avi,mxf,mpegts,mpegps,mpegvideo,dv,h264,hevc,m4v,flv,asf,wav,aiff,mjpeg,image2"
PARSERS="h264,hevc,mpegvideo,mpeg4video,mjpeg,dvbsub,aac,ac3,mpegaudio,dnxhd,prores,vp8,vp9,av1,opus,vorbis,flac,dca,mlp"
ENCODERS="prores_ks,dnxhd,pcm_s16le,pcm_s24le,pcm_s32le,pcm_f32le,pcm_s16be,pcm_s24be,pcm_f32be,aac,png"
MUXERS="mov,mp4,mxf,mxf_opatom,matroska,wav"

EXTRA=()
case "$(uname -s)" in
    Darwin)
        ENCODERS="$ENCODERS,h264_videotoolbox,hevc_videotoolbox,prores_videotoolbox"
        EXTRA+=(--enable-videotoolbox --enable-audiotoolbox --arch=arm64 --extra-cflags=-mmacosx-version-min=13.0)
        ;;
esac

cd "$SRC"
make distclean >/dev/null 2>&1 || true
./configure --prefix="$OUT" \
    --enable-static --disable-shared --enable-pic \
    --disable-programs --disable-doc --disable-avdevice --disable-avfilter --disable-debug --disable-network --disable-autodetect \
    --disable-everything --enable-zlib \
    --enable-avcodec --enable-avformat --enable-avutil --enable-swscale --enable-swresample \
    --enable-protocol=file \
    --enable-decoder="$DECODERS" --enable-demuxer="$DEMUXERS" --enable-parser="$PARSERS" \
    --enable-encoder="$ENCODERS" --enable-muxer="$MUXERS" \
    --enable-bsf=h264_mp4toannexb,hevc_mp4toannexb,aac_adtstoasc,extract_extradata \
    ${EXTRA[@]+"${EXTRA[@]}"} >/dev/null

JOBS=$( (sysctl -n hw.ncpu 2>/dev/null || nproc) )
make -j"$JOBS" >/dev/null 2>"$OUT.log" || { tail -40 "$OUT.log"; exit 1; }
make install >/dev/null 2>&1
echo "$REV" > "$OUT/.rev"
echo "FFmpeg built into $OUT"
