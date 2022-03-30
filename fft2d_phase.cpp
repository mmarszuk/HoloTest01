#include <climits>
#include <cmath>

#include "ipp/latest/include/ipp.h"

#include "fft2d_phase.h"

//If below makro is uncommented the algorithm use copy on the pointers
#define FFT2D_PHASE_COPY_ON_THE_POINTERS


//The following method release one variable and set it on the zero value.
template<typename T>
static void oneFree( T **memory ) {
    if( *memory != nullptr ) {
        delete[] *memory;
        *memory = nullptr;
    }
}

//The following method release one variable and set it on the zero value.
template<typename T>
static void ippOneFree( T **memory ) {
    if( *memory != nullptr ) {
        ippFree( *memory );
        *memory = nullptr;
    }
}

//Circle shift
template<class T>
static void circShift(const T src[], T dst[], const int width, const int height, const int xshift, const int yshift)
{
  for (int y = 0, yy = yshift; y < height; y++,yy++) {
    if( yy >= height ) yy = 0;
    for (int x = 0, xx=xshift; x < width; x++,xx++) {
      if( xx >= width ) xx = 0;
      dst[yy * width + xx] = src[y * width + x];
    }
  }
}

//Rect shift
template<class T>
inline static void rectShift( const T src[], T dst[], const int width, const int height)
{
    circShift(src, dst, width, height, width/2, height/2 );
}

//Inversion rect shift
template<class T>
inline static void invRectShift( const T src[], T dst[], const int width, const int height)
{
    circShift(src, dst, width, height, (width+1)/2, (height+1)/2 );
}

//The following method compute the amplitude of the complex number
template<typename T=Ipp32fc>
inline static auto amplitude( const T *const a) {
    return sqrt( a->re * a->re + a->im * a->im );
};

//The following method compare two complex number by amplitude.
template<typename T=Ipp32fc>
inline static bool cmpAmplitude( const T *const a, const T *const b ) {
    return amplitude(a) > amplitude(b);
};

//The following method find the pointer to the pixel with max aplitude in the top image.
template<typename T>
static const T* findMaxAmplitude(
    const T *image,        //the image as complex numbers.
    const int topPercent,  //the percent of image height.
    const int imageWidth,  //the width of the image.
    const int imageHeight  //the height of the image.
) {
    FFT2D_PHASE_ASSERT( imageHeight * topPercent / 100 > 0 );
    const T *const end = image + imageWidth * (imageHeight * topPercent / 100);
    const T *max = image++;
    while( image < end ) {                   // from begin to the end of the image top.
        if( cmpAmplitude(image,max) ) {
            max = image;
        }
        image ++ ;
    }
    return max;
}

//The following method convert the input to the output.
template<typename T_FROM, typename T_TO>
static void convert(
    const T_FROM *input,           //pointer to the input buffer with padding.
    T_TO *output,                  //pointer to the out buffer.
    const int inputWidth,          //the width of the input buffer given as number of elements (e.g. all pixels from row without padding).
    const int inputHeight,         //the height of the input buffer given as number of elements (e.g. all pixels from row without padding).
    const int rowSize              //the row size of the buffer given in the bytes with padding.
) {
    using ONE_BYTE_TYPE = uint8_t;
    static_assert( sizeof(ONE_BYTE_TYPE) == 1, "sizeof(ONE_BYTE_TYPE) == 1" );
    FFT2D_PHASE_ASSERT( rowSize >= sizeof(T_FROM) * inputWidth );
    const int paddingSize = rowSize - sizeof(T_FROM) * inputWidth;
    const T_FROM *const endInput = reinterpret_cast<const T_FROM*>( reinterpret_cast<const ONE_BYTE_TYPE*>(input) + inputHeight * rowSize );
    while( input < endInput ) {
        const T_FROM *const endRow = input + inputWidth;
        while( input < endRow ) {
            *output++ = static_cast<T_TO>( *input++ );
        }
        input = reinterpret_cast<const T_FROM*>( reinterpret_cast<const ONE_BYTE_TYPE*>(input) + paddingSize );
    }
}

//The following method convert the input to the output with scaling.
template<typename T_FROM, typename T_TO>
static void convertWithScale(
    const T_FROM *input,           //pointer to the input buffer with padding.
    T_TO *output,                  //pointer to the out buffer.
    const int size,                //the width of the input buffer given as number of elements (e.g. all pixels from row without padding).
    const T_FROM add,              //the value to the add
    const T_FROM mul               //the value to the mul
) {
    const T_FROM *const endInput = input + size;
    while( input < endInput ) {
        *output++ = static_cast<T_TO>( (*input + add) * mul );
        input ++ ;
    }
}

//The following method computing the min and max value.
template<typename T>
static void findMinMax( const T *input, const int inputSize, T &min, T &max) {
    FFT2D_PHASE_ASSERT( inputSize > 0 );
    const T *const end = input + inputSize;
    min = max = *input;
    while( ++input < end ) {
        if( min > *input ) {
            min = *input;
        } else if( max < *input ) {
            max = *input;
        }
    }
}

#ifdef FFT2D_PHASE_COPY_ON_THE_POINTERS
//The following method copy N elements from input to output, where N == endInput - input.
template<typename T>
inline static void copyLine(
    const T *input,
    const T *const endInput,
    T *output
) {
    while( input < endInput ) {
       *output++ = *input++;
    }
}

//The following method copy the square region from input to the center of output.
template<typename T>
static void copySquareToCenter(
    const T *input,                //the begin of input data (pointer to first element)
    T *output,                     //the begin of output data  (pointer to first element)
    const int imageWidth,          //the width of the input buffer given as number of elements.
    const int imageHeight,         //the height of the input buffer given as number of elements.
    const int inputX,              //the center X input image.
    const int inputY,              //the center Y input image.
    const int size                 //the side of the square.
) {
    input += inputX + (inputY-size) * imageWidth - size;
    const T *const endInput = input + (2*size+1) * imageWidth;
    output += imageWidth/2 + (imageHeight/2-size) * imageWidth - size;
    while( input < endInput ) {
        copyLine( input , input + 2*size+1 , output );
        input += imageWidth;
        output += imageHeight;
    }
}

#else

//The following method copy the square region from input to the center of output.
template<typename T>
static void copySquareToCenter(
    const T input[],               //the begin of input data (pointer to first element).
    T output[],                    //the begin of output data  (pointer to first element).
    const int imageWidth,          //the width of the input buffer given as number of elements.
    const int imageHeight,         //the height of the input buffer given as number of elements.
    const int inputX,              //the center X input image.
    const int inputY,              //the center Y input image.
    const int size                 //the side of the square.
) {
    for( int ys=inputY-size, yd=imageHeight/2-size ; ys<=inputY+size  ; ys++, yd++ ) {
        for( int xs=inputX-size, xd=imageWidth/2-size ; xs<=inputX+size  ; xs++, xd++ ) {
            output[ yd * imageWidth + xd ] = input[ ys * imageWidth + xs];
        }
    }
}

#endif



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
    FFT2D_PHASE_ASSERT( idxMaxX + roi <  imageWidth  );
    FFT2D_PHASE_ASSERT( idxMaxX - roi >= 0           );
    FFT2D_PHASE_ASSERT( idxMaxY + roi <  imageHeight );
    FFT2D_PHASE_ASSERT( idxMaxY - roi >= 0           );

    copySquareToCenter( buffC0, buffC1, imageWidth, imageHeight, idxMaxX, idxMaxY, roi );

    //buffC1 = shift( fft( ishift( buffC1 ) ) )
    invRectShift( buffC1, buffC0, imageWidth, imageHeight );
    ippiDFTInv_CToC_32fc_C1IR(buffC0, sizeof(Ipp32fc) * imageWidth, pDFTSpec, pMemBuffer);
    rectShift( buffC0, buffC1, imageWidth, imageHeight );

    // Phase FFT
    ippsPhase_32fc(buffC1, buffFlt0, imageSize);
    Ipp32f minPhase, maxPhase;
    findMinMax(buffFlt0,imageSize,minPhase,maxPhase);
    FFT2D_PHASE_ASSERT( minPhase >= -PI - EPSILON );
    FFT2D_PHASE_ASSERT( maxPhase <= +PI + EPSILON );

    //unwrapPhase( buffFlt0 )
    //Some links to the phase unwrapping
    //https://docs.opencv.org/3.4/d9/dfb/tutorial_unwrap.html
    //https://wiki.qt.io/How_to_setup_Qt_and_openCV_on_Windows

    const Ipp32f add = -minPhase;
    const Ipp32f range = maxPhase - minPhase;
    const Ipp32f mul = range < 0.0001 ? 1 : std::numeric_limits<Ipp8u>::max() / range;  //do not div by zero.
    convertWithScale(buffFlt0,imageOutput,imageSize,add,mul);

    //Color map.
}
