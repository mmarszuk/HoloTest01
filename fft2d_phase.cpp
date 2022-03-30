#include <climits>
#include <cmath>

#include "ipp/latest/include/ipp.h"

#include "fft2d_phase.h"


FFT2DPhase::~FFT2DPhase() {
    allFree();
}

bool FFT2DPhase::isNeedRealloc(const int imageWidth, const int imageHeight) {
    return imageWidth != this->imageWidth || imageHeight != this->imageHeight;
}

void FFT2DPhase::reAlloc(const int imageWidth, const int imageHeight) {
    if( ! isNeedRealloc(imageWidth,imageHeight) ) {
        return;
    }
    this->imageWidth  = imageWidth;
    this->imageHeight = imageHeight;
    allFree();
    buffFlt0 = new Ipp32f[imageWidth * imageHeight];
    buffC0   = new Ipp32fc[imageWidth * imageHeight];
    buffC1   = new Ipp32fc[imageWidth * imageHeight];

    {
        const IppiSize roiSize = {imageWidth, imageHeight};
        int pSizeSpec, pSizeInit, pSizeBuf;
        ippiDFTGetSize_C_32fc(roiSize, IPP_FFT_DIV_INV_BY_N, ippAlgHintNone, &pSizeSpec, &pSizeInit, &pSizeBuf);
        pDFTSpec = (IppiDFTSpec_C_32fc*) ippMalloc(pSizeSpec);
        if(pSizeInit > 0) {
            pMemInit = (Ipp8u*) ippMalloc(pSizeInit);
        }
        if(pSizeBuf > 0) {
            pMemBuffer = (Ipp8u*) ippMalloc(pSizeBuf);
        }
        ippiDFTInit_C_32fc(roiSize, IPP_FFT_DIV_INV_BY_N, ippAlgHintNone, pDFTSpec, pMemInit);
    }
}

void FFT2DPhase::allFree() {
    oneFree( &buffFlt0 );
    oneFree( &buffC0 );
    oneFree( &buffC1 );
    ippOneFree( &pDFTSpec );
    ippOneFree( &pMemInit );
    ippOneFree( &pMemBuffer );
}

void FFT2DPhase::performFFTPhase(
    const Ipp8u imageInput[],
    const int imageWidth,
    const int imageHeight,
    const int rowSize,
    Ipp8u imageOutput[],
    const int percentTop,
    int roi
) {
    const int imageSize = imageWidth * imageHeight;
    reAlloc(imageWidth,imageHeight);

    convert(imageInput, buffFlt0, imageWidth, imageHeight, rowSize);

    // Returns a complex vector pDst constructed from real parts of the input vector buffFlt0.
    ippsRealToCplx_32f(buffFlt0, nullptr, buffC0, imageSize );

    //buffC0 = shift( fft( ishift( buffC0 ) ) )
    invRectShift( buffC0, buffC1, imageWidth, imageHeight );
    ippiDFTFwd_CToC_32fc_C1IR(buffC1, sizeof(Ipp32fc) * imageWidth, pDFTSpec, pMemBuffer);
    rectShift( buffC1, buffC0, imageWidth, imageHeight );

    const Ipp32fc *const maxAmplitude = findMaxAmplitude(buffC0,percentTop/2,imageWidth,imageHeight);
    const int idxMaxX = static_cast<int>( (maxAmplitude - buffC0) % imageWidth );
    const int idxMaxY = static_cast<int>( (maxAmplitude - buffC0) / imageHeight );

    if( roi > idxMaxX              ) roi = idxMaxX;              //too near left?
    if( roi > idxMaxY              ) roi = idxMaxY;              //too near top?
    if( roi > imageWidth - idxMaxX ) roi = imageWidth - idxMaxX; //too near right?
    //never too near bottom, because we cutting out from TOP_PERCENT.

    memset( buffC1 , 0 , sizeof(buffC1[0]) * imageSize );
    FFT2D_PHASE_ASSERT( idxMaxX + roi <  imageWidth );
    FFT2D_PHASE_ASSERT( idxMaxX - roi >= 0 );
    FFT2D_PHASE_ASSERT( idxMaxY + roi <  imageHeight );
    FFT2D_PHASE_ASSERT( idxMaxY - roi >= 0 );

    copySquareToCenter( buffC0, buffC1, imageWidth, imageHeight, idxMaxX, idxMaxY, roi );

    //buffC1 = shift( fft( ishift( buffC1 ) ) )
    invRectShift( buffC1, buffC0, imageWidth, imageHeight );
    ippiDFTInv_CToC_32fc_C1IR(buffC0, sizeof(Ipp32fc) * imageWidth, pDFTSpec, pMemBuffer);
    rectShift( buffC0, buffC1, imageWidth, imageHeight );

    // Phase FFT
    ippsPhase_32fc(buffC1, buffFlt0, imageSize);
    Ipp32f minPhase, maxPhase;
    findMinMax(buffFlt0,imageSize,minPhase,maxPhase);
    FFT2D_PHASE_ASSERT( minPhase <= -PI - 0.1 );
    FFT2D_PHASE_ASSERT( maxPhase >= +PI + 0.1 );

    const Ipp32f add = -minPhase;
    const Ipp32f range = maxPhase - minPhase;
    const Ipp32f mul = range < 0.0001 ? 1 : std::numeric_limits<Ipp8u>::max() / range;  //do not div by zero.

    convertWithScale(buffFlt0,imageOutput,imageSize,add,mul);
}
