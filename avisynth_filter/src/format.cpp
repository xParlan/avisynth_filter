// License: https://github.com/CrendKing/avisynth_filter/blob/master/LICENSE

#include "pch.h"
#include "format.h"
#include "api.h"
#include "constants.h"


namespace AvsFilter {

static const __m128i DEINTERLEAVE_MASK_8_BIT_1 = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 14, 12, 10, 8, 6, 4, 2, 0);
static const __m128i DEINTERLEAVE_MASK_8_BIT_2 = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 15, 13, 11, 9, 7, 5, 3, 1);
static const __m128i DEINTERLEAVE_MASK_16_BIT_1 = _mm_set_epi8(29, 28, 25, 24, 21, 20, 17, 16, 13, 12, 9, 8, 5, 4, 1, 0);
static const __m128i DEINTERLEAVE_MASK_16_BIT_2 = _mm_set_epi8(31, 30, 27, 26, 23, 22, 19, 18, 15, 14, 11, 10, 7, 6, 3, 2);

const std::unordered_map<std::wstring, Format::Definition> Format::FORMATS = {
    { L"NV12", { .mediaSubtype = MEDIASUBTYPE_NV12, .avsType = VideoInfo::CS_YV12, .bitCount = 12, .componentsPerPixel = 1 } },
    { L"YV12", { .mediaSubtype = MEDIASUBTYPE_YV12, .avsType = VideoInfo::CS_YV12, .bitCount = 12, .componentsPerPixel = 1 } },
    { L"I420", { .mediaSubtype = MEDIASUBTYPE_I420, .avsType = VideoInfo::CS_YV12, .bitCount = 12, .componentsPerPixel = 1 } },
    { L"IYUV", { .mediaSubtype = MEDIASUBTYPE_IYUV, .avsType = VideoInfo::CS_YV12, .bitCount = 12, .componentsPerPixel = 1 } },

    // P010 has the most significant 6 bits zero-padded, while AviSynth expects the least significant bits padded
    // P010 without right shifting 6 bits on every WORD is equivalent to P016, without precision loss
    { L"P010", { .mediaSubtype = MEDIASUBTYPE_P010, .avsType = VideoInfo::CS_YUV420P16, .bitCount = 24, .componentsPerPixel = 1 } },

    { L"P016", { .mediaSubtype = MEDIASUBTYPE_P016, .avsType = VideoInfo::CS_YUV420P16, .bitCount = 24, .componentsPerPixel = 1 } },

    // packed formats such as YUY2 are twice as wide as unpacked formats per pixel
    { L"YUY2", { .mediaSubtype = MEDIASUBTYPE_YUY2, .avsType = VideoInfo::CS_YUY2, .bitCount = 16, .componentsPerPixel = 2 } },
    { L"UYUY", { .mediaSubtype = MEDIASUBTYPE_UYVY, .avsType = VideoInfo::CS_YUY2, .bitCount = 16, .componentsPerPixel = 2 } },

    { L"RGB24", { .mediaSubtype = MEDIASUBTYPE_RGB24, .avsType = VideoInfo::CS_BGR24, .bitCount = 24, .componentsPerPixel = 3 } },
    { L"RGB32", { .mediaSubtype = MEDIASUBTYPE_RGB32, .avsType = VideoInfo::CS_BGR32, .bitCount = 24, .componentsPerPixel = 4 } },
};

auto Format::VideoFormat::operator!=(const VideoFormat &other) const -> bool {
    return name != other.name
        || memcmp(&videoInfo, &other.videoInfo, sizeof(videoInfo)) != 0
        || pixelAspectRatio != other.pixelAspectRatio
        || hdrType != other.hdrType
        || hdrLuminance != other.hdrLuminance
        || bmi.biSize != other.bmi.biSize
        || memcmp(&bmi, &other.bmi, bmi.biSize) != 0;
}

auto Format::VideoFormat::GetCodecFourCC() const -> DWORD {
    return FOURCCMap(&FORMATS.at(name).mediaSubtype).GetFOURCC();
}

auto Format::LookupMediaSubtype(const CLSID &mediaSubtype) -> std::optional<std::wstring> {
    for (const auto &[formatName, definition] : FORMATS) {
        if (mediaSubtype == definition.mediaSubtype) {
            return formatName;
        }
    }

    return std::nullopt;
}

auto Format::LookupAvsType(int avsType) -> std::vector<std::wstring> {
    std::vector<std::wstring> ret;

    for (const auto &[formatName, definition] : FORMATS) {
        if (avsType == definition.avsType) {
            ret.emplace_back(formatName);
        }
    }

    return ret;
}

auto Format::GetVideoFormat(const AM_MEDIA_TYPE &mediaType) -> VideoFormat {
    VideoFormat info { .name = *LookupMediaSubtype(mediaType.subtype), .bmi = *GetBitmapInfo(mediaType) };
    const VIDEOINFOHEADER *vih = reinterpret_cast<VIDEOINFOHEADER *>(mediaType.pbFormat);
    const REFERENCE_TIME frameDuration = vih->AvgTimePerFrame > 0 ? vih->AvgTimePerFrame : DEFAULT_AVG_TIME_PER_FRAME;

    info.videoInfo.width = info.bmi.biWidth;
    info.videoInfo.height = abs(info.bmi.biHeight);
    info.videoInfo.fps_numerator = UNITS;
    info.videoInfo.fps_denominator = static_cast<unsigned int>(frameDuration);
    info.videoInfo.pixel_type = FORMATS.at(info.name).avsType;
    info.videoInfo.num_frames = NUM_FRAMES_FOR_INFINITE_STREAM;

    info.pixelAspectRatio = PAR_SCALE_FACTOR;
    if (SUCCEEDED(CheckVideoInfo2Type(&mediaType))) {
        const VIDEOINFOHEADER2* vih2 = reinterpret_cast<VIDEOINFOHEADER2 *>(mediaType.pbFormat);
        if (vih2->dwPictAspectRatioY > 0) {
            /*
             * pixel aspect ratio = display aspect ratio (DAR) / storage aspect ratio (SAR)
             * DAR comes from VIDEOINFOHEADER2.dwPictAspectRatioX / VIDEOINFOHEADER2.dwPictAspectRatioY
             * SAR comes from info.videoInfo.width / info.videoInfo.height
             */
            info.pixelAspectRatio = static_cast<int>(llMulDiv(static_cast<LONGLONG>(vih2->dwPictAspectRatioX) * info.videoInfo.height,
                                                     PAR_SCALE_FACTOR,
                                                     static_cast<LONGLONG>(vih2->dwPictAspectRatioY) * info.videoInfo.width,
                                                     0));
        }
    }

    info.hdrType = 0;
    info.hdrLuminance = 0;

    return info;
}

auto Format::WriteSample(const VideoFormat &format, PVideoFrame srcFrame, BYTE *dstBuffer, IScriptEnvironment *avsEnv) -> void {
    const BYTE *srcSlices[] = { srcFrame->GetReadPtr(), srcFrame->GetReadPtr(PLANAR_U), srcFrame->GetReadPtr(PLANAR_V) };
    const int srcStrides[] = { srcFrame->GetPitch(), srcFrame->GetPitch(PLANAR_U), srcFrame->GetPitch(PLANAR_V) };

    CopyToOutput(format, srcSlices, srcStrides, dstBuffer, srcFrame->GetRowSize(), srcFrame->GetHeight(), avsEnv);
}

auto Format::CreateFrame(const VideoFormat &format, const BYTE *srcBuffer, IScriptEnvironment *avsEnv) -> PVideoFrame {
    PVideoFrame frame = avsEnv->NewVideoFrame(format.videoInfo, sizeof(__m128i));

    BYTE *dstSlices[] = { frame->GetWritePtr(), frame->GetWritePtr(PLANAR_U), frame->GetWritePtr(PLANAR_V) };
    const int dstStrides[] = { frame->GetPitch(), frame->GetPitch(PLANAR_U), frame->GetPitch(PLANAR_V) };

    CopyFromInput(format, srcBuffer, dstSlices, dstStrides, frame->GetRowSize(), frame->GetHeight(), avsEnv);

    return frame;
}

auto Format::CopyFromInput(const VideoFormat &format, const BYTE *srcBuffer, BYTE *dstSlices[], const int dstStrides[], int dstRowSize, int dstHeight, IScriptEnvironment *avsEnv) -> void {
    const Definition &def = FORMATS.at(format.name);

    const int srcStride = format.bmi.biWidth * format.videoInfo.ComponentSize() * def.componentsPerPixel;
    const int rowSize = min(srcStride, dstRowSize);
    const int height = min(abs(format.bmi.biHeight), dstHeight);
    const int srcDefaultPlaneSize = srcStride * height;

    const BYTE *srcDefaultPlane;
    int srcDefaultPlaneStride;

    // for RGB DIB in Windows (biCompression == BI_RGB), positive biHeight is bottom-up, negative is top-down
    // AviSynth+'s convert functions always assume the input DIB is bottom-up, so we invert the DIB if it's top-down
    if (format.bmi.biCompression == BI_RGB && format.bmi.biHeight < 0) {
        srcDefaultPlane = srcBuffer + srcDefaultPlaneSize - srcStride;
        srcDefaultPlaneStride = -srcStride;
    } else {
        srcDefaultPlane = srcBuffer;
        srcDefaultPlaneStride = srcStride;
    }

    avsEnv->BitBlt(dstSlices[0], dstStrides[0], srcDefaultPlane, srcDefaultPlaneStride, rowSize, height);

    if ((def.avsType & VideoInfo::CS_INTERLEAVED) == 0) {
        // 4:2:0 unpacked formats

        if (def.mediaSubtype == MEDIASUBTYPE_IYUV || def.mediaSubtype == MEDIASUBTYPE_I420 || def.mediaSubtype == MEDIASUBTYPE_YV12) {
            // these formats' U and V planes are not interleaved. use BitBlt to efficiently copy

            const BYTE *srcPlane1 = srcBuffer + srcDefaultPlaneSize;
            const BYTE *srcPlane2 = srcPlane1 + srcDefaultPlaneSize / 4;

            const BYTE *srcU;
            const BYTE *srcV;
            if (def.mediaSubtype == MEDIASUBTYPE_YV12) {
                // YV12 has V plane first

                srcU = srcPlane2;
                srcV = srcPlane1;
            } else {
                srcU = srcPlane1;
                srcV = srcPlane2;
            }

            avsEnv->BitBlt(dstSlices[1], dstStrides[1], srcU, srcStride / 2, rowSize / 2, height / 2);
            avsEnv->BitBlt(dstSlices[2], dstStrides[2], srcV, srcStride / 2, rowSize / 2, height / 2);
        } else {
            // interleaved U and V planes. copy byte by byte
            // consider using intrinsics for better performance

            const BYTE *srcUVStart = srcBuffer + srcDefaultPlaneSize;
            __m128i mask1, mask2;

            if (format.videoInfo.ComponentSize() == 1) {
                mask1 = DEINTERLEAVE_MASK_8_BIT_1;
                mask2 = DEINTERLEAVE_MASK_8_BIT_2;
            } else {
                mask1 = DEINTERLEAVE_MASK_16_BIT_1;
                mask2 = DEINTERLEAVE_MASK_16_BIT_2;
            }

            if (format.videoInfo.ComponentSize() == 1) {
                Deinterleave<uint8_t>(srcUVStart, srcStride, dstSlices[1], dstSlices[2], dstStrides[1], rowSize, height / 2, mask1, mask2);
            } else {
                Deinterleave<uint16_t>(srcUVStart, srcStride, dstSlices[1], dstSlices[2], dstStrides[1], rowSize, height / 2, mask1, mask2);
            }
        }
    }
}

auto Format::CopyToOutput(const VideoFormat &format, const BYTE *srcSlices[], const int srcStrides[], BYTE *dstBuffer, int srcRowSize, int srcHeight, IScriptEnvironment *avsEnv) -> void {
    const Definition &def = FORMATS.at(format.name);

    const int dstStride = format.bmi.biWidth * format.videoInfo.ComponentSize() * def.componentsPerPixel;
    const int rowSize = min(dstStride, srcRowSize);
    const int height = min(abs(format.bmi.biHeight), srcHeight);
    const int dstDefaultPlaneSize = dstStride * height;

    BYTE *dstDefaultPlane;
    int dstDefaultPlaneStride;

    // AviSynth+'s convert functions always produce bottom-up DIB, so we invert the DIB if downstream needs top-down
    if (format.bmi.biCompression == BI_RGB && format.bmi.biHeight < 0) {
        dstDefaultPlane = dstBuffer + dstDefaultPlaneSize - dstStride;
        dstDefaultPlaneStride = -dstStride;
    } else {
        dstDefaultPlane = dstBuffer;
        dstDefaultPlaneStride = dstStride;
    }

    avsEnv->BitBlt(dstDefaultPlane, dstDefaultPlaneStride, srcSlices[0], srcStrides[0], rowSize, height);

    if ((def.avsType & VideoInfo::CS_INTERLEAVED) == 0) {
        if (def.mediaSubtype == MEDIASUBTYPE_IYUV || def.mediaSubtype == MEDIASUBTYPE_I420 || def.mediaSubtype == MEDIASUBTYPE_YV12) {
            BYTE *dstPlane1 = dstBuffer + dstDefaultPlaneSize;
            BYTE *dstPlane2 = dstPlane1 + dstDefaultPlaneSize / 4;

            BYTE *dstU;
            BYTE *dstV;
            if (def.mediaSubtype == MEDIASUBTYPE_YV12) {
                dstU = dstPlane2;
                dstV = dstPlane1;
            } else {
                dstU = dstPlane1;
                dstV = dstPlane2;
            }

            avsEnv->BitBlt(dstU, dstStride / 2, srcSlices[1], srcStrides[1], rowSize / 2, height / 2);
            avsEnv->BitBlt(dstV, dstStride / 2, srcSlices[2], srcStrides[2], rowSize / 2, height / 2);
        } else {
            BYTE *dstUVStart = dstBuffer + dstDefaultPlaneSize;

            if (format.videoInfo.ComponentSize() == 1) {
                Interleave<uint8_t>(srcSlices[1], srcSlices[2], srcStrides[1], dstUVStart, dstStride, rowSize / 2, height / 2);
            } else {
                Interleave<uint16_t>(srcSlices[1], srcSlices[2], srcStrides[1], dstUVStart, dstStride, rowSize / 2, height / 2);
            }
        }
    }
}

}
