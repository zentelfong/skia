/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkSurface_Gpu.h"

#include "SkCanvas.h"
#include "SkGpuDevice.h"
#include "SkImage_Base.h"
#include "SkImage_Gpu.h"
#include "SkImagePriv.h"
#include "SkSurface_Base.h"

#if SK_SUPPORT_GPU

///////////////////////////////////////////////////////////////////////////////

SkSurface_Gpu::SkSurface_Gpu(SkGpuDevice* device)
    : INHERITED(device->width(), device->height(), &device->surfaceProps())
    , fDevice(SkRef(device)) {
}

SkSurface_Gpu::~SkSurface_Gpu() {
    fDevice->unref();
}

SkCanvas* SkSurface_Gpu::onNewCanvas() {
    SkCanvas::InitFlags flags = SkCanvas::kDefault_InitFlags;
    // When we think this works...
//    flags |= SkCanvas::kConservativeRasterClip_InitFlag;

    return SkNEW_ARGS(SkCanvas, (fDevice, &this->props(), flags));
}

SkSurface* SkSurface_Gpu::onNewSurface(const SkImageInfo& info) {
    GrRenderTarget* rt = fDevice->accessRenderTarget();
    int sampleCount = rt->numColorSamples();
    // TODO: Make caller specify this (change virtual signature of onNewSurface).
    static const Budgeted kBudgeted = kNo_Budgeted;
    return SkSurface::NewRenderTarget(fDevice->context(), kBudgeted, info, sampleCount,
                                      &this->props());
}

SkImage* SkSurface_Gpu::onNewImageSnapshot(Budgeted budgeted) {
    const SkImageInfo info = fDevice->imageInfo();
    const int sampleCount = fDevice->accessRenderTarget()->numColorSamples();
    SkImage* image = NULL;
    GrTexture* tex = fDevice->accessRenderTarget()->asTexture();
    if (tex) {
        image = SkNEW_ARGS(SkImage_Gpu,
                           (info.width(), info.height(), info.alphaType(),
                            tex, sampleCount, budgeted));
    }
    if (image) {
        as_IB(image)->initWithProps(this->props());
    }
    return image;
}

// Create a new render target and, if necessary, copy the contents of the old
// render target into it. Note that this flushes the SkGpuDevice but
// doesn't force an OpenGL flush.
void SkSurface_Gpu::onCopyOnWrite(ContentChangeMode mode) {
    GrRenderTarget* rt = fDevice->accessRenderTarget();
    // are we sharing our render target with the image? Note this call should never create a new
    // image because onCopyOnWrite is only called when there is a cached image.
    SkImage* image = this->getCachedImage(kNo_Budgeted);
    SkASSERT(image);
    if (rt->asTexture() == as_IB(image)->getTexture()) {
        this->fDevice->replaceRenderTarget(SkSurface::kRetain_ContentChangeMode == mode);
        SkTextureImageApplyBudgetedDecision(image);
    } else if (kDiscard_ContentChangeMode == mode) {
        this->SkSurface_Gpu::onDiscard();
    }
}

void SkSurface_Gpu::onDiscard() {
    fDevice->accessRenderTarget()->discard();
}

///////////////////////////////////////////////////////////////////////////////

SkSurface* SkSurface::NewRenderTargetDirect(GrRenderTarget* target, const SkSurfaceProps* props) {
    SkAutoTUnref<SkGpuDevice> device(SkGpuDevice::Create(target, props));
    if (!device) {
        return NULL;
    }
    return SkNEW_ARGS(SkSurface_Gpu, (device));
}

SkSurface* SkSurface::NewRenderTarget(GrContext* ctx, Budgeted budgeted, const SkImageInfo& info,
                                      int sampleCount, const SkSurfaceProps* props) {
    SkAutoTUnref<SkGpuDevice> device(SkGpuDevice::Create(ctx, budgeted, info, sampleCount, props,
                                                         SkGpuDevice::kNeedClear_Flag));
    if (!device) {
        return NULL;
    }
    return SkNEW_ARGS(SkSurface_Gpu, (device));
}

SkSurface* SkSurface::NewWrappedRenderTarget(GrContext* context, GrBackendTextureDesc desc,
                                             const SkSurfaceProps* props) {
    if (NULL == context) {
        return NULL;
    }
    if (!SkToBool(desc.fFlags & kRenderTarget_GrBackendTextureFlag)) {
        return NULL;
    }
    SkAutoTUnref<GrSurface> surface(context->textureProvider()->wrapBackendTexture(desc));
    if (!surface) {
        return NULL;
    }
    SkAutoTUnref<SkGpuDevice> device(SkGpuDevice::Create(surface->asRenderTarget(), props,
                                                         SkGpuDevice::kNeedClear_Flag));
    if (!device) {
        return NULL;
    }
    return SkNEW_ARGS(SkSurface_Gpu, (device));
}

#endif
